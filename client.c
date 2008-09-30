#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>

static const char socket_name[] = "\0wayland";

struct method {
	const char *name;
	uint32_t opcode;
};

static const struct method display_methods[] = {
	{ "get_interface", 0 },
	{ "create_surface", 1 },
	{ NULL }
};

static const struct method surface_methods[] = {
	{ "get_interface", 0 },
	{ "post", 1 },
	{ NULL }
};

struct interface {
	const char *name;
	const struct method *methods;
};

static const struct interface interfaces[] = {
	{ "display", display_methods },
	{ "surface", surface_methods },
	{ NULL },
};

int send_request(int sock, const char *buffer, int buffer_len)
{
	const struct method *methods;
	char interface[32], method[32];
	uint32_t request[3], id, new_id;
	int i;

	if (sscanf(buffer, "%d/%32[^:]:%32s %d\n",
		   &id, interface, method, &new_id) != 4) {
		printf("invalid input, expected <id>/<interface>:<method>\n");
		return -1;
	}

	printf("got object id %d, interface \"%s\", name \"%s\"\n",
	       id, interface, method);

	for (i = 0; interfaces[i].name != NULL; i++) {
		if (strcmp(interfaces[i].name, interface) == 0)
			break;
	}
	if (interfaces[i].name == NULL) {
		printf("unknown interface \"%s\"\n", interface);
		return -1;
	}

	methods = interfaces[i].methods;
	for (i = 0; methods[i].name != NULL; i++) {
		if (strcmp(methods[i].name, method) == 0)
			break;
	}

	if (methods[i].name == NULL) {
		printf("unknown request \"%s\"\n", method);
		return -1;
	}

	request[0] = id;
	request[1] = methods[i].opcode;
	request[2] = new_id;

	return write(sock, request, sizeof request);
}

int main(int argc, char *argv[])
{
	struct sockaddr_un name;
	socklen_t size;
	int sock, len;
	char buffer[256];

	sock = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (sock < 0)
		return -1;

	name.sun_family = AF_LOCAL;
	memcpy(name.sun_path, socket_name, sizeof socket_name);

	size = offsetof (struct sockaddr_un, sun_path) + sizeof socket_name;

	if (connect (sock, (struct sockaddr *) &name, size) < 0)
		return -1;

	while (len = read(STDIN_FILENO, buffer, sizeof buffer), len > 0)
		if (send_request(sock, buffer, len) < 0)
			break;

	return 0;
}
