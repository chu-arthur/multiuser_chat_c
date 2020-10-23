#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>

struct user{
	char* name;
	uint16_t length;
};

struct send_msg_param{
	struct user *user;
	int sockfd;
};

void* send_msg(void* args){
	struct send_msg_param *arg = (struct send_msg_param *) args;
	struct user *user = (struct user *) arg->user;
	int sockfd = arg->sockfd;
        uint16_t msgLength;
	size_t bufsz = 255;
        while(1){
		char* buf = malloc(sizeof(char) * bufsz);
                bzero(buf, bufsz);
		printf(">");
                fgets(buf, bufsz, stdin);
		if (strncmp(buf, "\\QUIT", 5) == 0 || strncmp(buf, "\\quit", 5) == 0){
			send(sockfd, "QUIT", 4, 0);
			free(buf);
			exit(0);
		}
                msgLength = strlen(buf)-1;
		char tempo[msgLength];
		strcpy(tempo, buf);
                for (int i = 8 + user->length; i < 8 + user->length + msgLength; i++){
                        buf[i] = tempo[i - 8 - user->length];
                }
		char* temp = (char*) &user->length;
		user->length = be16toh(user->length);
                buf[0] = 'M';
                buf[1] = 'E';
                buf[2] = 'S';
                buf[3] = 'G';
                buf[4] = temp[0];
                buf[5] = temp[1];
		user->length = be16toh(user->length);
                for (int i = 6; i < 6 + user->length; i++){
                        buf[i] = user->name[i-6];
                }
                temp = (char*) &msgLength;
		msgLength = be16toh(msgLength);
                buf[6 + user->length] = temp[0];
                buf[6 + user->length + 1] = temp[1];
		msgLength = be16toh(msgLength);
                int n = send(sockfd, buf, msgLength + 8 + user->length, 0);
                if (n < 0){
                        printf("Write to server failed\n");
			exit(0);
                }
		printf("\n");
		free(buf);
        }
}

void* receive_msg(void* args){
	int sockfd = *(int *) args;
	while(1){
		char mtype[4];
		int r;
		r = recv(sockfd, mtype, 4, MSG_WAITALL);
		if (r < 4){
			printf("short read %d: server sent an invalid message.\n", r);
			exit(0);
		}
		if (strncmp(mtype, "MESG", 4) == 0){
			uint16_t len;
			printf("\n");
			r = recv(sockfd, &len, 2, MSG_WAITALL);
			if (r != 2){
				printf("failed to read message from server\n");
				exit(0);
			}
			len = be16toh(len);
			char* name = malloc(len+1);
			name[len] = '\0';
			r = recv(sockfd, name, len, MSG_WAITALL);
			if (r != len){
				printf("name isn't same length as what was said...\n");
				exit(0);
			}
			r = recv(sockfd, &len, 2, MSG_WAITALL);
			if (r != 2){
				printf("bad message sent..\n");
				exit(0);
			}
			len = be16toh(len);
			char *msg = calloc(len+1, 1);
			msg[len] = '\0';
			r = recv(sockfd, msg, len, MSG_WAITALL);
			if (r != len){
				printf("msg isnt same length as what was said...\n");
				exit(0);
			}
			printf("%s >> %s\n", name, msg);
		}
	}
}

int main(int argc, char** argv){
	if (argc!=4){
		printf("Usage: %s <host> <port> <username>\n", argv[0]);
	}
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0){
		printf("Can't make socket...\n");
		return 1;
	}
	struct hostent* srvr;
	srvr = gethostbyname(argv[1]);
	if (srvr == NULL){
		printf("No such host: %s\n", argv[1]);
		return 1;
	}

	struct sockaddr_in svraddr;
	bzero((char*)&svraddr, sizeof(svraddr));
	svraddr.sin_family = AF_INET;
	bcopy((char*)srvr->h_addr, (char*)&svraddr.sin_addr.s_addr,srvr->h_length);
	svraddr.sin_port = htons(atoi(argv[2]));

	if (connect(sockfd, (struct sockaddr*)&svraddr, sizeof(svraddr)) < 0){
		printf("Connection to server %s failed\n", argv[1]);
		return 1;
	}
	struct user *thisUser = malloc(sizeof(struct user) + sizeof(argv[3]));
	char len[2];
	thisUser->name = argv[3];
	thisUser->length = (uint16_t) strlen(argv[3]);
	thisUser->length = be16toh(thisUser->length);
	char* temp = (char*) &thisUser->length;
	len[0] = temp[0];
	len[1] = temp[1];
        char* setUser = calloc(6 + strlen(argv[3]), 1);
	strcpy(setUser, "CNCT");
	setUser[4] = len[0];
	setUser[5] = len[1];
	for (int i = 6; i < 6 + strlen(argv[3]); i++){
		setUser[i] = argv[3][i-6];
	}
	thisUser->length = be16toh(thisUser->length);
        send(sockfd, setUser, 6 + strlen(argv[3]), 0);
	
	char mtype[4];
	int n = recv(sockfd, mtype, 4, MSG_WAITALL);
	if(n != 4){
		printf("Server did not respond");
		return 1;
	}
	if (strncmp(mtype, "ACKC", 4) == 0){
		printf("Connected to server as %s\n", argv[3]);
	}

	int err;
	pthread_t writeThread;
	pthread_t readThread;

	struct send_msg_param *params = calloc(sizeof(struct user*) + sizeof(int), 1);
	params->user = thisUser;
	params->sockfd = sockfd;

	err = pthread_create(&writeThread, NULL, &send_msg, params);
	err = pthread_create(&readThread, NULL, &receive_msg, &sockfd);

	err = pthread_join(writeThread, NULL);
	err = pthread_join(readThread, NULL);

	free(thisUser);
	close(sockfd);
	return 0;
}
