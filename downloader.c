
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <netinet/tcp.h>


#define BUFFER_SIZE 4096
#define THREAD_COUNT 3
#define size_arrays 100000

void * run(void * idx);
int  bytes_per_thread, total_received = 0;

char *url, *host_name, *file_path, *file_name;

// given a full URL, strip it down to set host name, file path, and file name
int parseURL(char* url)
{
	int num_bytes, i, httpOffset;
	char *temp;

	// trim http or https from url
	if (strstr(url, "http://") != NULL) httpOffset = 7;
	else if (strstr(url, "https://") != NULL)	httpOffset = 8;
	else httpOffset = 0;

	temp = (char *) malloc(sizeof(char) * (sizeof(url)+1));

	if ((temp = strchr(url + httpOffset, '/')) != NULL)
	{	

		// set the name of the host
		num_bytes = temp - url - httpOffset;
		host_name = (char *) malloc(sizeof(char) * (num_bytes+1));
		strncpy(host_name, url + httpOffset, num_bytes);

		// full file name in website's directory
		file_path = (char *) malloc(sizeof(char) * (strlen(temp)+1));
		strncpy(file_path, temp, strlen(temp));

		if ((temp = strrchr(url, '/')) != NULL)
		{	// set the name of the file being downloaded
			temp++;
			num_bytes = temp - url;
			file_name = (char *) malloc(sizeof(char) * (num_bytes + 1));
			strncpy(file_name, temp, strlen(temp));
			printf("local file_name = %s\n", file_name);

		}
		else 
		{
			printf("Error occurred while parsing the URL\n");
			exit(0);
		}


		return 0;

	}

	printf("Error occurred while parsing the URL\n");
	exit(0);
	return -1;
};

struct hostent *hp;
struct sockaddr_in addr;
int on = 1, sock;     
int port = 80;

int main(int argc, char* argv[])
{	// download a file from the internet using the URL provided as the argument


	int header_socket,
			num_bytes,
			bytes_in_file,
			bytes_in_header,
			i;

	char 	*get_request,
				*head_request,
				buffer[BUFFER_SIZE];



	FILE *file;


	// check for correct number of arguments
	if (argc != 2)
	{
		printf("Please provide a URL as a command line argument\n");
		exit(0);
	}
	else
	{
		// read the url from command line arg
		url = argv[1];
	}

	parseURL(url);
	printf("global file_name = %s\n", file_name);
	// create a HEAD request;

	head_request = (char *) malloc(sizeof(char) * (strlen(file_path) + strlen(host_name) + 30));

	strcpy(head_request, "HEAD ");
	strcat(head_request, file_path);
	strcat(head_request, " HTTP/1.1\r\nHost: ");
	strcat(head_request, host_name);
	strcat(head_request, "\r\n\r\n");

	printf("\n~~~~~~~HEAD Request~~~~~~~\n%s", head_request);



	if((hp = gethostbyname(host_name)) == NULL){
		herror("gethostbyname");
		exit(1);
	}

	bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;
	header_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	setsockopt(header_socket, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));

	if(header_socket == -1){
		perror("setsockopt");
		exit(1);
	}

	if(connect(header_socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1){
		perror("connect");
		exit(1);

	}


	// send HEAD request, store response in buffer
	send(header_socket, head_request, strlen(head_request), 0);
	bytes_in_header = recv(header_socket, buffer, sizeof(buffer), 0);

	// display HEAD of http file
	printf("\n~~~~~~~HEAD Request~~~~~~~\n%s\n", buffer);

	// parse header for content length
	char* start = strstr(buffer, "Content-Length: ") + 16;
	char* end = strchr(start, '\n');
	int num = end - start;
	char* bytes = (char *) malloc(sizeof(start));
	memcpy(bytes, start, num);
	bytes_in_file = atoi(bytes);

	bytes_per_thread = bytes_in_file / THREAD_COUNT + 1;


	pthread_t threads[THREAD_COUNT];
	for(i = 0; i<THREAD_COUNT; i++){
		pthread_create(&threads[i], NULL, &run, (void *)i);
	}

	for(i = 0; i<THREAD_COUNT; i++){
		pthread_join(threads[i], NULL);
	}
}


// Thread function
void * run(void * idx){
	int id = (int) idx;

	char get_request[10000];
	int data_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	setsockopt(data_socket, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));

	if(data_socket == -1){
		perror("setsockopt");
		exit(1);
	}
	if(connect(data_socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1){
		perror("connect");
		exit(1);
	}

	char start_byte[100];
	char end_byte[100];

	sprintf(start_byte, "%d", id*bytes_per_thread);
	sprintf(end_byte, "%d", (id+1)*bytes_per_thread - 1);

	strcpy(get_request, "GET ");
	strcat(get_request, file_path);
	strcat(get_request, " HTTP/1.1\r\nHost: ");
	strcat(get_request, host_name);
	strcat(get_request, "\r\nRange: bytes=");
	strcat(get_request, start_byte);
	strcat(get_request, "-");
	strcat(get_request, end_byte);
	strcat(get_request, "\r\n\r\n");


	// send get request
	send(data_socket, get_request, strlen(get_request), 0);
	int index, num_bytes;
	char buffer[BUFFER_SIZE];
	FILE * f = fopen (file_name, "w");
	printf("%d", bytes_per_thread);


	fseek (f, id*bytes_per_thread, SEEK_SET);
	int m = 0;
	while((num_bytes = recv(data_socket, buffer, sizeof(buffer), 0)))
	{

		char * ptr;
		char *temp;

		if (m == 0) {
			ptr = strstr(buffer, "\r\n\r\n") + 4;
			temp = ptr;
			num_bytes -= ptr - buffer;
		}
		else 
			temp = buffer;

		total_received += num_bytes;

		fwrite (temp, num_bytes, 1, f);
		m++;
		memset(buffer, 0, sizeof buffer);
	}

	fclose(f);
}
