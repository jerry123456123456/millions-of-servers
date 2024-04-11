#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#define BUFFER_LENGTH   1024
#define EPOLL_SIZE      1024

#define MAX_PORT 100   //一共可以开（监听）100个端口

int islistenfd(int fd, int *fds) {
    int i;
    for (i = 0; i < MAX_PORT; i++) {
        if (fd == *(fds + i)) return fd;
    }
    return 0;
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Param Error\n");
        return -1;
    }

    int port = atoi(argv[1]);   //从这个端口开始
    int sockfds[MAX_PORT] = {0};

    int epfd = epoll_create(1);
    struct epoll_event events[EPOLL_SIZE];

    for (int i = 0; i < MAX_PORT; i++) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port + i);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) {
            perror("bind");
            return 2;
        }

        if (listen(sockfd, 5) < 0) {
            perror("listen");
            return 3;
        }
	
        printf("tcp server listen on port : %d\n",port+i);	

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = sockfd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

        sockfds[i] = sockfd;
    }

    while (1) {
        int nready = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        if (nready == -1) continue;

        for (int i = 0; i < nready; i++) {
            int sockfd = islistenfd(events[i].data.fd, sockfds);

            if (sockfd) {
                struct sockaddr_in client_addr;
                memset(&client_addr, 0, sizeof(struct sockaddr_in));
                socklen_t client_len = sizeof(client_addr);
                int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);

                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = clientfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);

                // 设置新连接的套接字为非阻塞模式
                int flags = fcntl(clientfd, F_GETFL, 0);
                fcntl(clientfd, F_SETFL, flags | O_NONBLOCK);
            } else {
                int clientfd = events[i].data.fd;
                while (1) {
                    char buffer[BUFFER_LENGTH];
                    memset(buffer, 0, sizeof(buffer));
                    int len = recv(clientfd, buffer, BUFFER_LENGTH, 0);
                    if (len < 0) {
                        if (errno == EAGAIN) {
                            printf("数据已经接收完毕...\n");
                            break;
                        } else {
                            close(clientfd);
                            epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, NULL);
                            exit(1);
                        }
                    } else if (len == 0) { // disconnect
                        printf("客户端已经断开连接!  \n");
                        close(clientfd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, NULL);
                    } else {
                        printf("Recv: %s, %d byte(s)\n", buffer, len);
                        send(clientfd, buffer, len, 0);
                    }
                }
            }
        }
    }

    return 0;
}
