#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

fd_set	fd_write;	// fds which can be written to
fd_set	fd_read;	// fds that have a message that can be read
fd_set	fd_all;		// all currently open fds from the network
int		max_fd = 0;	// highest possible open fd

int	client_id[70000];		// each client gets assigned an id, which is at the index of its fd. size is 70000 bc it's right over the maximum of connections one server can have
int	client_curr_msg[70000];	// each client has this flag, which is initialized to 0 at the beginning, and set to 1 when a message from this client has finished. why? i do not know. sth to do with long messages maybe
int	g_id = 0;				// this variable is incremented with each new client and assigned to the next, so each client has a unique id

char	msg[4096 * 42 + 42];	// buffer where we put messages we want to send
char	buf[4096 * 42];			// when a message is recieved, it's first stored in there
char	line[4096 * 42];		// here will be stored each line of a message

void	fatal(int server_fd) {
	close(server_fd);
	char* string = "Fatal error\n";
	write(2, string, strlen(string));
	exit(1);
}

void	send_msg_to_all(int except) {
	for (int i = 0; i <= max_fd; i++) {
		if (FD_ISSET(i, &fd_write) && i != except)
			send(i, msg, strlen(msg), 0);
	}
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		char* string = "Wrong number of arguments\n";
		write(2, string, strlen(string));
		exit(1);
	}
	uint16_t port = atoi(argv[1]);

	/*--- this part is given by the subject, but is changed up a bit ---*/
	int server_fd;
	struct sockaddr_in servaddr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0); 
	if (server_fd == -1)
		fatal(server_fd);

	bzero(&servaddr, sizeof(servaddr)); 

	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 

	if ((bind(server_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal(server_fd);
	if (listen(server_fd, 10) != 0)
		fatal(server_fd);
	/*----------------------------------------------------------------------*/

	/* init variables etc */
	FD_ZERO(&fd_all);
	FD_SET(server_fd, &fd_all);
	max_fd = server_fd;
	bzero(client_id, sizeof(client_id));
	bzero(client_curr_msg, sizeof(client_curr_msg));

	/* create variables to be used in loop */
	struct sockaddr_in	client_addr;
	socklen_t			len = sizeof(struct sockaddr_in);
	int					bytes_read;
	int					client_fd;

	/* checking for connections with clients */
	while (1) {
		fd_read = fd_all;
		fd_write = fd_all;
		if (select(max_fd + 1, &fd_read, &fd_write, NULL, 0) <= 0)
			continue;

		/* go through all open fds */
		for (int i = 0; i <= max_fd; i++) {
			/* is there sth to read?*/
			if (FD_ISSET(i, &fd_read)) {
				/* when you can read from the server/socket fd, this means a new client wants to connect */
				if (i == server_fd) {
					/* set up new client */
					bzero(&client_addr, len);
					client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &len);
					if (client_fd == -1)
						continue;
					
					FD_SET(client_fd, &fd_all);
					if (max_fd < client_fd)
						max_fd = client_fd;
					client_id[client_fd] = g_id++;
					client_curr_msg[client_fd] = 0;

					sprintf(msg, "server: client %d just arrived\n", client_id[client_fd]);
					send_msg_to_all(server_fd);
					break;
				}
				/* recieve client message */
				bzero(buf, sizeof(buf));
				bytes_read = recv(i, buf, sizeof(buf), 0);
				/* if nothing can be read, this means the client has disconnected */
				if (bytes_read <= 0) {
					/* clean up disconnected client */
					FD_CLR(i, &fd_all);
					close(i);

					sprintf(msg, "server: client %d just left\n", client_id[i]);
					send_msg_to_all(i);
					break;
				}
				/* print recieved message line by line */
				for (int a = 0, b = 0; a < bytes_read; a++, b++) {
					line[b] = buf[a];

					/* when line is finished, print/send to all other clients */
					if (line[b] == '\n') {
						line[b + 1] = '\0';

						if (client_curr_msg[i])
							sprintf(msg, "%s", line);
						else
							sprintf(msg, "client %d: %s", client_id[i], line);
						send_msg_to_all(i);

						client_curr_msg[i] = 0;
						b = -1;
					}
					/*
					basically the end of the loop, not sure why it's in an else if
					if the recieved message is finished, also send it and end the loop.
					*/
					else if (a == (bytes_read - 1)) {
						line[b + 1] = '\0';

						if (client_curr_msg[i])
							sprintf(msg, "%s", line);
						else
							sprintf(msg, "client %d: %s", client_id[i], line);
						send_msg_to_all(i);

						client_curr_msg[i] = 1;
						break;
					}
				}
			}
		}
	}
	return (0);
}