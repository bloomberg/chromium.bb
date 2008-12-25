/*
 * Copyright © 2008 Kristian Høgsberg
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/uio.h>
#include <ffi.h>
#include <assert.h>

#include "wayland-util.h"
#include "connection.h"

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

struct wl_buffer {
	char data[4096];
	int head, tail;
};

struct wl_connection {
	struct wl_buffer in, out;
	int fd;
	void *data;
	wl_connection_update_func_t update;
};

struct wl_connection *
wl_connection_create(int fd,
		     wl_connection_update_func_t update,
		     void *data)
{
	struct wl_connection *connection;

	connection = malloc(sizeof *connection);
	memset(connection, 0, sizeof *connection);
	connection->fd = fd;
	connection->update = update;
	connection->data = data;

	connection->update(connection,
			   WL_CONNECTION_READABLE,
			   connection->data);

	return connection;
}

void
wl_connection_destroy(struct wl_connection *connection)
{
	free(connection);
}

void
wl_connection_copy(struct wl_connection *connection, void *data, size_t size)
{
	struct wl_buffer *b;
	int tail, rest;

	b = &connection->in;
	tail = b->tail;
	if (tail + size <= ARRAY_LENGTH(b->data)) {
		memcpy(data, b->data + tail, size);
	} else { 
		rest = ARRAY_LENGTH(b->data) - tail;
		memcpy(data, b->data + tail, rest);
		memcpy(data + rest, b->data, size - rest);
	}
}

void
wl_connection_consume(struct wl_connection *connection, size_t size)
{
	struct wl_buffer *b;
	int tail, rest;

	b = &connection->in;
	tail = b->tail;
	if (tail + size <= ARRAY_LENGTH(b->data)) {
		b->tail += size;
	} else { 
		rest = ARRAY_LENGTH(b->data) - tail;
		b->tail = size - rest;
	}
}

int wl_connection_data(struct wl_connection *connection, uint32_t mask)
{
	struct wl_buffer *b;
	struct iovec iov[2];
	int len, head, tail, count, size, available;

	if (mask & WL_CONNECTION_READABLE) {
		b = &connection->in;
		head = connection->in.head;
		if (head < b->tail) {
			iov[0].iov_base = b->data + head;
			iov[0].iov_len = b->tail - head;
			count = 1;
		} else {
			size = ARRAY_LENGTH(b->data) - head;
			iov[0].iov_base = b->data + head;
			iov[0].iov_len = size;
			iov[1].iov_base = b->data;
			iov[1].iov_len = b->tail;
			count = 2;
		}
		do {
			len = readv(connection->fd, iov, count);
		} while (len < 0 && errno == EINTR);
		if (len < 0) {
			fprintf(stderr,
				"read error from connection %p: %m (%d)\n",
				connection, errno);
			return -1;
		} else if (len == 0) {
			/* FIXME: Handle this better? */
			return -1;
		} else if (head + len <= ARRAY_LENGTH(b->data)) {
			b->head += len;
		} else {
			b->head = head + len - ARRAY_LENGTH(b->data);
		}

		/* We know we have data in the buffer at this point,
		 * so if head equals tail, it means the buffer is
		 * full. */

		available = b->head - b->tail;
		if (available == 0)
			available = sizeof b->data;
		else if (available < 0)
			available += ARRAY_LENGTH(b->data);
	} else {
		available = 0;
	}	

	if (mask & WL_CONNECTION_WRITABLE) {
		b = &connection->out;
		tail = b->tail;
		if (tail < b->head) {
			iov[0].iov_base = b->data + tail;
			iov[0].iov_len = b->head - tail;
			count = 1;
		} else {
			size = ARRAY_LENGTH(b->data) - tail;
			iov[0].iov_base = b->data + tail;
			iov[0].iov_len = size;
			iov[1].iov_base = b->data;
			iov[1].iov_len = b->head;
			count = 2;
		}
		do {
			len = writev(connection->fd, iov, count);
		} while (len < 0 && errno == EINTR);
		if (len < 0) {
			fprintf(stderr, "write error for connection %p: %m\n", connection);
			return -1;
		} else if (tail + len <= ARRAY_LENGTH(b->data)) {
			b->tail += len;
		} else {
			b->tail = tail + len - ARRAY_LENGTH(b->data);
		}

		/* We just took data out of the buffer, so at this
		 * point if head equals tail, the buffer is empty. */

		if (b->tail == b->head)
			connection->update(connection,
					   WL_CONNECTION_READABLE,
					   connection->data);
	}

	return available;
}

void
wl_connection_write(struct wl_connection *connection, const void *data, size_t count)
{
	struct wl_buffer *b;
	size_t size;
	int head;

	b = &connection->out;
	head = b->head;
	if (head + count <= ARRAY_LENGTH(b->data)) {
		memcpy(b->data + head, data, count);
		b->head += count;
	} else {
		size = ARRAY_LENGTH(b->data) - head;
		memcpy(b->data + head, data, size);
		memcpy(b->data, data + size, count - size);
		b->head = count - size;
	}

	if (b->tail == head)
		connection->update(connection,
				   WL_CONNECTION_READABLE |
				   WL_CONNECTION_WRITABLE,
				   connection->data);
}

void
wl_connection_vmarshal(struct wl_connection *connection,
		       struct wl_object *sender,
		       uint32_t opcode, va_list ap,
		       const struct wl_message *message)
{
	struct wl_object *object;
	uint32_t args[32], length, *p, size;
	const char *s;
	int i, count;

	count = strlen(message->signature);
	assert(count <= ARRAY_LENGTH(args));

	p = &args[2];
	for (i = 0; i < count; i++) {
		switch (message->signature[i]) {
		case 'u':
		case 'i':
			*p++ = va_arg(ap, uint32_t);
			break;
		case 's':
			s = va_arg(ap, const char *);
			length = strlen(s);
			*p++ = length;
			memcpy(p, s, length);
			p += DIV_ROUNDUP(length, sizeof(*p));
			break;
		case 'o':
		case 'n':
			object = va_arg(ap, struct wl_object *);
			*p++ = object->id;
			break;
		default:
			assert(0);
			break;
		}
	}

	size = (p - args) * sizeof *p;
	args[0] = sender->id;
	args[1] = opcode | (size << 16);
	wl_connection_write(connection, args, size);
}

void
wl_connection_demarshal(struct wl_connection *connection,
			uint32_t size,
			struct wl_hash *objects,
			void (*func)(void),
			void *data, struct wl_object *target,
			const struct wl_message *message)
{
	ffi_type *types[20];
	ffi_cif cif;
	uint32_t *p, result, length;
	int i, count;
	union {
		uint32_t uint32;
		char *string;
		void *object;
		uint32_t new_id;
	} values[20];
	void *args[20];
	struct wl_object *object;
	uint32_t buffer[64];

	count = strlen(message->signature) + 2;
	if (count > ARRAY_LENGTH(types)) {
		printf("too many args (%d)\n", count);
		return;
	}

	if (sizeof buffer < size) {
		printf("request too big, should malloc tmp buffer here\n");
		return;
	}

	types[0] = &ffi_type_pointer;
	values[0].object = data;
	args[0] =  &values[0];

	types[1] = &ffi_type_pointer;
	values[1].object = target;
	args[1] =  &values[1];

	wl_connection_copy(connection, buffer, size);
	p = &buffer[2];
	for (i = 2; i < count; i++) {
		switch (message->signature[i - 2]) {
		case 'u':
		case 'i':
			types[i] = &ffi_type_uint32;
			values[i].uint32 = *p++;
			break;
		case 's':
			types[i] = &ffi_type_pointer;
			length = *p++;
			values[i].string = malloc(length + 1);
			if (values[i].string == NULL) {
				/* FIXME: Send NO_MEMORY */
				return;
			}
			memcpy(values[i].string, p, length);
			values[i].string[length] = '\0';
			p += DIV_ROUNDUP(length, sizeof *p);
			break;
		case 'o':
			types[i] = &ffi_type_pointer;
			object = wl_hash_lookup(objects, *p);
			if (object == NULL)
				printf("unknown object (%d)\n", *p);
			values[i].object = object;
			p++;
			break;
		case 'n':
			types[i] = &ffi_type_uint32;
			values[i].new_id = *p;
			object = wl_hash_lookup(objects, *p);
			if (object != NULL)
				printf("object already exists (%d)\n", *p);
			p++;
			break;
		default:
			printf("unknown type\n");
			break;
		}
		args[i] = &values[i];
	}

	ffi_prep_cif(&cif, FFI_DEFAULT_ABI, count, &ffi_type_uint32, types);
	ffi_call(&cif, func, &result, args);

	for (i = 2; i < count; i++) {
		switch (message->signature[i - 2]) {
		case 's':
			free(values[i].string);
			break;
		}
	}
}
