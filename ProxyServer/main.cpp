﻿#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <signal.h>
#include <vector>
#include <iostream>
#include <string>
#include <iomanip>

#include "base64.h"

bool isstopping = false;
bool isLog = false;
bool isshow = false;
bool isUsingVector = false;
bool isserver = false;

int server_fd;
int thread_cout = 0;
std::vector<int> fds;
void removeValue(int value) {
	while (isUsingVector)
		sleep(1);
	isUsingVector = true;
	for (std::vector<int>::iterator it = fds.begin(); it != fds.end(); ++it)
		if (*it == value) {
			it = fds.erase(it);
			break;
		}
	isUsingVector = false;
}
int sendstr(int socketfd, std::string str) {
	char buf[1];
	for (size_t i = 0; i < str.size(); i++)
	{
		memset(buf, 0, 1);
		buf[0] = str.at(i);
		if (send(socketfd, buf, sizeof(buf), 0) < 1) return -1;
	}
	return 1;
}

void closeA(int A)
{
	shutdown(A, SHUT_RDWR);
	removeValue(A);
}
void AToB(int A, int B, bool cl = true) {
	++thread_cout;
	ssize_t status = 1;
	std::string color;
	if (cl)color = "\033[34;1m"; else color = "\033[31;1m";
	char buffer[1];
	while (status > 0 && !isstopping)
	{
		if (cl)
		{
			memset(buffer, 0, 1);
			status = recv(A, buffer, sizeof(buffer), 0);
			if (status < 1) break;
			std::string s = base64_encode(std::string(1, buffer[0]));
			status = sendstr(B, s + "\r\n");
			if (status < 1) break;
			if (!isstopping) if (isLog) std::cout << color + (isshow ? buffer[0] : '+') + "\e[0m" << std::flush;
		}
		else
		{
			memset(buffer, 0, 1);
			std::string w;
			w.clear();
			while (buffer[0] != '\n')
			{
				status = recv(A, buffer, sizeof(buffer), 0);
				w += buffer[0];
				if (status < 1) goto close;
			}
			w = w.substr(0, w.length() - 2);
			std::string s = base64_decode(w);
			status = sendstr(B, s);
			if (status < 1) break;
			if (!isstopping) if (isLog) std::cout << color + (isshow ? s : "+") + "\e[0m" << std::flush;
		}
	}
close:
	closeA(B);
	if (!isstopping) std::cout << color + "$\e[0m" << std::flush;
	--thread_cout;
};

static void stop(int sig) {
	if (isstopping) return;
	isstopping = true;
	shutdown(server_fd, SHUT_RDWR);
	std::cout << std::endl << "Server Stopping: Shutting Down Sockets " << std::endl << std::endl;
	for (size_t i = 0; i < fds.size(); ++i) {
		std::cout << "\e[1A\e[K" << "[" << i << "/" << fds.size() << "]" << std::endl;
		shutdown(fds[i], SHUT_RDWR);
		usleep(33000);
	}
	sleep(1);
	std::cout << "\e[1A\e[K\e[1A\e[K[$$$$$$$$$$$$$$]" << std::endl;
	exit(0);
}

void thread_wait_stdin_stop() {
	std::string buf;
	while (buf != "stop")
		std::cin >> buf;
	stop(0);
}
void usage() {
	std::cout
		<< std::endl
		<< "Proxy "
		<< "--local-port port "
		<< "--remote-port port "
		<< "--remote-address address "
		<< "[--server]"
		<< "[--log] "
		<< "[--log-flow] "
		<< std::endl;
}
int main(int argc, char* argv[])
{
	uint16_t ThisPort = 0;
	uint16_t ObjectPort = 0;
	char* ObjectAddress = (char*)"";
	for (int i = 1; i < argc; ++i)
	{
		if (std::string(argv[i]) == "--local-port") {
			ThisPort = (uint16_t)std::stoi(argv[i + 1]);
			++i;
			continue;
		}
		else if (std::string(argv[i]) == "--remote-port") {
			ObjectPort = (uint16_t)std::stoi(argv[i + 1]);
			++i;
			continue;
		}
		else if (std::string(argv[i]) == "--remote-address") {
			ObjectAddress = argv[i + 1];
			++i;
			continue;
		}
		else if (std::string(argv[i]) == "--server") {
			isserver = true;
			continue;
		}
		else if (std::string(argv[i]) == "--log") {
			isLog = true;
			continue;
		}
		else if (std::string(argv[i]) == "--log-flow") {
			isshow = true;
			continue;
		}
	}
	if (std::string(ObjectAddress) == "") { std::cout << "Remote Address Not Found"; usage(); return -1; }
	if (ObjectPort == 0) { std::cout << "Remote Port Not Found"; usage(); return -1; }
	if (ThisPort == 0) { std::cout << "Local Port Not Found"; usage(); return -1; }

	signal(SIGINT, stop);
	signal(SIGPIPE, SIG_IGN);
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in t;
	t.sin_family = AF_INET;
	t.sin_addr.s_addr = INADDR_ANY;
	t.sin_port = htons(ThisPort);
	if (bind(server_fd, (struct sockaddr *)&t, sizeof(struct sockaddr)) < 0)
	{
		perror("bind error");
		return 1;
	}
	if (listen(server_fd, 5) < 0)
	{
		perror("listen error");
		return 1;
	};
	std::cout << std::endl << "[#%#%#%#%#%#%#%]" << std::endl;
	std::thread ws(thread_wait_stdin_stop);
	ws.detach();
	while (true) {
		sockaddr_in r;
		int sin_size = sizeof(struct sockaddr_in);
		int client_sockfd;
		if ((client_sockfd = accept(server_fd, (struct sockaddr *)&r, (socklen_t*)&sin_size)) < 0)
			continue;
		std::cout << "\033[34;1m#\033[0m" << std::flush;
		fds.push_back(client_sockfd);
		int socketfd = socket(AF_INET, SOCK_STREAM, 0);
		sockaddr_in sockaddr;
		sockaddr.sin_family = AF_INET;
		sockaddr.sin_port = htons(ObjectPort);
		inet_pton(AF_INET, ObjectAddress, &sockaddr.sin_addr);
		if ((connect(socketfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr))) < 0)
		{
			close(client_sockfd);
			removeValue(client_sockfd);
			std::cout << "\033[34;1m$\033[0m" << std::flush;
		}
		else {
			fds.push_back(socketfd);
			std::cout << "\033[31;1m#\033[0m" << std::flush;
			std::thread c2s(AToB, client_sockfd, socketfd, !isserver);
			std::thread s2c(AToB, socketfd, client_sockfd, isserver);
			c2s.detach();
			s2c.detach();
		}
	}
	close(server_fd);
	return 0;
}