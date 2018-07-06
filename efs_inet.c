#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "private/efs_log.h"
#include "private/efs_inet.h"

int efs_inet_listen(int port) {
	int s = socket(AF_INET6, SOCK_STREAM, 0);

	if (s < 0) {
	  efs_log_error("error creating listener socket");
	  return -1;
	}

	struct sockaddr_in6 server_addr = {
		   .sin6_family = AF_INET6,
		   .sin6_addr = in6addr_any,
		   .sin6_port = htons(port),
	};

	int err = bind(s, (struct sockaddr *) &server_addr, sizeof(server_addr));
	if (err < 0) {
	  efs_log_error("error binding listener port %d", port);
	  return -1;
	}

	err = listen(s, 10);
	if (err < 0) {
	   efs_log_error("error listening on socket");
	   return -1;
	}

	return s;
}

int efs_inet_get_socket_port(int socket) {
	struct sockaddr_in6 data_port_addr;
	socklen_t data_port_addr_len = sizeof(struct sockaddr_in6);
	int err = getsockname(socket, (struct sockaddr*) &data_port_addr, &data_port_addr_len);
	if (err != 0) {
		efs_log_error("error getting listener socket port number");
		return -1;
	}
	return ntohs(data_port_addr.sin6_port);
}

int efs_inet_read_line(int socket, char* buffer, size_t buffer_len) {
	memset(buffer, 0, buffer_len);
	int total_read_len = 0;
	while (total_read_len < buffer_len) {
		// read up to length - 1 bytes. the - 1 leaves room for the
		// null terminator.
		int read_len = read(socket,
				buffer + total_read_len,
				buffer_len - total_read_len - 1);
		if (read_len == 0) {
			// end of stream - since we didn't find the end of line in
			// the previous pass we won't find it in this one, so this
			// is an error.
			return -1;
		}
		else if (read_len < 0) {
			// general error
			return read_len;
		}
		total_read_len += read_len;
		char* p = strstr(buffer, "\r\n");
		if (p) {
			// null terminate the line and return
			*p = '\0';
			efs_log_info("< '%s'", buffer);
			return 0;
		}
	}
	return -1;
}

int efs_inet_writef(int socket, char* buffer, size_t buffer_len, char* format, ...) {
	va_list args;
	va_start(args, format);
	int err = vsnprintf(buffer, buffer_len, format, args);
	va_end(args);
	if (err >= buffer_len) {
		return -1;
	}

	char* p = buffer;
	int length = strlen(buffer);
	while (length) {
		int write_len = write(socket, p, length);
		if (write_len < 0) {
			efs_log_error("write error");
			return write_len;
		}
		p += write_len;
		length -= write_len;
	}
	efs_log_info("> %s", buffer);
	return 0;
}