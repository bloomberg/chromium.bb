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
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "wayland-util.h"
#include "connection.h"

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

struct wl_buffer {
	char data[4096];
	int head, tail;
};

#define MASK(i) ((i) & 4095)

struct wl_connection {
	struct wl_buffer in, out;
	struct wl_buffer fds_in, fds_out;
	int fds_in_tail;
	int fd;
	void *data;
	wl_connection_update_func_t update;
};

static void
wl_buffer_put(struct wl_buffer *b, const void *data, size_t count)
{
	int head, size;

	head = MASK(b->head);
	if (head + count <= sizeof b->data) {
		memcpy(b->data + head, data, count);
	} else {
		size = sizeof b->data - head;
		memcpy(b->data + head, data, size);
		memcpy(b->data, (const char *) data + size, count - size);
	}

	b->head += count;
}

static void
wl_buffer_put_iov(struct wl_buffer *b, struct iovec *iov, int *count)
{
	int head, tail;

	head = MASK(b->head);
	tail = MASK(b->tail);
	if (head < tail) {
		iov[0].iov_base = b->data + head;
		iov[0].iov_len = tail - head;
		*count = 1;
	} else if (tail == 0) {
		iov[0].iov_base = b->data + head;
		iov[0].iov_len = sizeof b->data - head;
		*count = 1;
	} else {
		iov[0].iov_base = b->data + head;
		iov[0].iov_len = sizeof b->data - head;
		iov[1].iov_base = b->data;
		iov[1].iov_len = tail;
		*count = 2;
	}
}

static void
wl_buffer_get_iov(struct wl_buffer *b, struct iovec *iov, int *count)
{
	int head, tail;

	head = MASK(b->head);
	tail = MASK(b->tail);
	if (tail < head) {
		iov[0].iov_base = b->data + tail;
		iov[0].iov_len = head - tail;
		*count = 1;
	} else if (head == 0) {
		iov[0].iov_base = b->data + tail;
		iov[0].iov_len = sizeof b->data - tail;
		*count = 1;
	} else {
		iov[0].iov_base = b->data + tail;
		iov[0].iov_len = sizeof b->data - tail;
		iov[1].iov_base = b->data;
		iov[1].iov_len = head;
		*count = 2;
	}
}

static void
wl_buffer_copy(struct wl_buffer *b, void *data, size_t count)
{
	int tail, size;

	tail = MASK(b->tail);
	if (tail + count <= sizeof b->data) {
		memcpy(data, b->data + tail, count);
	} else {
		size = sizeof b->data - tail;
		memcpy(data, b->data + tail, size);
		memcpy((char *) data + size, b->data, count - size);
	}
}

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
	close(connection->fd);
	free(connection);
}

void
wl_connection_copy(struct wl_connection *connection, void *data, size_t size)
{
	wl_buffer_copy(&connection->in, data, size);
}

void
wl_connection_consume(struct wl_connection *connection, size_t size)
{
	connection->in.tail += size;
	connection->fds_in.tail = connection->fds_in_tail;
}

static void
build_cmsg(struct wl_buffer *buffer, char *data, int *clen)
{
	struct cmsghdr *cmsg;
	size_t size;

	size = buffer->head - buffer->tail;
	if (size > 0) {
		cmsg = (struct cmsghdr *) data;
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(size);
		wl_buffer_copy(buffer, CMSG_DATA(cmsg), size);
		*clen = cmsg->cmsg_len;
	} else {
		*clen = 0;
	}
}

static void
close_fds(struct wl_buffer *buffer)
{
	int fds[32], i, count;
	size_t size;

	size = buffer->head - buffer->tail;
	if (size == 0)
		return;

	wl_buffer_copy(buffer, fds, size);
	count = size / 4;
	for (i = 0; i < count; i++)
		close(fds[i]);
	buffer->tail += size;
}

static void
decode_cmsg(struct wl_buffer *buffer, struct msghdr *msg)
{
	struct cmsghdr *cmsg;
	size_t size;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL;
	     cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
			size = cmsg->cmsg_len - CMSG_LEN(0);
			wl_buffer_put(buffer, CMSG_DATA(cmsg), size);
		}
	}
}

int
wl_connection_data(struct wl_connection *connection, uint32_t mask)
{
	struct iovec iov[2];
	struct msghdr msg;
	char cmsg[128];
	int len, count, clen;

	if (mask & WL_CONNECTION_READABLE) {
		wl_buffer_put_iov(&connection->in, iov, &count);

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = iov;
		msg.msg_iovlen = count;
		msg.msg_control = cmsg;
		msg.msg_controllen = sizeof cmsg;
		msg.msg_flags = 0;

		do {
			len = recvmsg(connection->fd, &msg, 0);
		} while (len < 0 && errno == EINTR);

		if (len < 0) {
			fprintf(stderr,
				"read error from connection %p: %m (%d)\n",
				connection, errno);
			return -1;
		} else if (len == 0) {
			/* FIXME: Handle this better? */
			return -1;
		}

		decode_cmsg(&connection->fds_in, &msg);

		connection->in.head += len;
	}	

	if (mask & WL_CONNECTION_WRITABLE) {
		wl_buffer_get_iov(&connection->out, iov, &count);

		build_cmsg(&connection->fds_out, cmsg, &clen);

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = iov;
		msg.msg_iovlen = count;
		msg.msg_control = cmsg;
		msg.msg_controllen = clen;
		msg.msg_flags = 0;

		do {
			len = sendmsg(connection->fd, &msg, 0);
		} while (len < 0 && errno == EINTR);

		if (len < 0) {
			fprintf(stderr,
				"write error for connection %p, fd %d: %m\n",
				connection, connection->fd);
			return -1;
		}

		close_fds(&connection->fds_out);

		connection->out.tail += len;
		if (connection->out.tail == connection->out.head)
			connection->update(connection,
					   WL_CONNECTION_READABLE,
					   connection->data);
	}

	return connection->in.head - connection->in.tail;
}

void
wl_connection_write(struct wl_connection *connection,
		    const void *data, size_t count)
{
	wl_buffer_put(&connection->out, data, count);

	if (connection->out.head - connection->out.tail == count)
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
	int32_t dup_fd;
	struct wl_array *array;
	const char *s;
	int i, count, fd;

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
			length = s ? strlen(s) : 0;
			*p++ = length;
			memcpy(p, s, length);
			p += DIV_ROUNDUP(length, sizeof(*p));
			break;
		case 'o':
		case 'n':
			object = va_arg(ap, struct wl_object *);
			*p++ = object ? object->id : 0;
			break;
		case 'a':
			array = va_arg(ap, struct wl_array *);
			if (array == NULL || array->size == 0) {
				*p++ = 0;
				break;
			}
			*p++ = array->size;
			memcpy(p, array->data, array->size);
			p = (void *) p + array->size;
			break;
		case 'h':
			fd = va_arg(ap, int);
			dup_fd = dup(fd);
			if (dup_fd < 0) {
				fprintf(stderr, "dup failed: %m");
				abort();
			}
			wl_buffer_put(&connection->fds_out,
				      &dup_fd, sizeof dup_fd);
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

int
wl_connection_demarshal(struct wl_connection *connection,
			uint32_t size,
			struct wl_hash_table *objects,
			void (*func)(void),
			void *data, struct wl_object *target,
			const struct wl_message *message)
{
	ffi_type *types[20];
	ffi_cif cif;
	uint32_t *p, *next, *end, result, length;
	int i, count, fds_tail, ret = 0;
	union {
		uint32_t uint32;
		char *string;
		void *object;
		uint32_t new_id;
		struct wl_array *array;
	} values[20];
	void *args[20];
	struct wl_object *object;
	uint32_t buffer[64];

	count = strlen(message->signature) + 2;
	if (count > ARRAY_LENGTH(types)) {
		printf("too many args (%d)\n", count);
		assert(0);
	}

	if (sizeof buffer < size) {
		printf("request too big, should malloc tmp buffer here\n");
		assert(0);
	}

	types[0] = &ffi_type_pointer;
	values[0].object = data;
	args[0] =  &values[0];

	types[1] = &ffi_type_pointer;
	values[1].object = target;
	args[1] =  &values[1];

	wl_connection_copy(connection, buffer, size);
	p = &buffer[2];
	end = (uint32_t *) ((char *) (p + size));
	fds_tail = connection->fds_in.tail;
	for (i = 2; i < count; i++) {
		if (p + 1 > end) {
			printf("message too short, "
			       "object (%d), message %s(%s)\n",
			       *p, message->name, message->signature);
			errno = EINVAL;
			ret = -1;
			goto out;
		}

		switch (message->signature[i - 2]) {
		case 'u':
		case 'i':
			types[i] = &ffi_type_uint32;
			values[i].uint32 = *p++;
			break;
		case 's':
			types[i] = &ffi_type_pointer;
			length = *p++;

			next = p + DIV_ROUNDUP(length, sizeof *p);
			if (next > end) {
				printf("message too short, "
				       "object (%d), message %s(%s)\n",
				       *p, message->name, message->signature);
				errno = EINVAL;
				ret = -1;
				goto out;
			}

			if (length == 0) {
				values[i].string = NULL;
			} else {
				values[i].string = malloc(length + 1);
				if (values[i].string == NULL) {
					errno = ENOMEM;
					ret = -1;
					goto out;
				}
				memcpy(values[i].string, p, length);
				values[i].string[length] = '\0';
			}
			p = next;
			break;
		case 'o':
			types[i] = &ffi_type_pointer;
			object = wl_hash_table_lookup(objects, *p);
			if (object == NULL && *p != 0) {
				printf("unknown object (%d), message %s(%s)\n",
				       *p, message->name, message->signature);
				errno = EINVAL;
				ret = -1;
				goto out;
			}
			values[i].object = object;
			p++;
			break;
		case 'n':
			types[i] = &ffi_type_uint32;
			values[i].new_id = *p;
			object = wl_hash_table_lookup(objects, *p);
			if (object != NULL) {
				printf("not a new object (%d), "
				       "message %s(%s)\n",
				       *p, message->name, message->signature);
				errno = EINVAL;
				ret = -1;
				goto out;
			}
			p++;
			break;
		case 'a':
			types[i] = &ffi_type_pointer;
			length = *p++;

			next = p + DIV_ROUNDUP(length, sizeof *p);
			if (next > end) {
				printf("message too short, "
				       "object (%d), message %s(%s)\n",
				       *p, message->name, message->signature);
				errno = EINVAL;
				ret = -1;
				goto out;
			}

			values[i].array =
				malloc(length + sizeof *values[i].array);
			if (values[i].array == NULL) {
				errno = ENOMEM;
				ret = -1;
				goto out;
			}
			values[i].array->size = length;
			values[i].array->alloc = 0;
			values[i].array->data = values[i].array + 1;
			memcpy(values[i].array->data, p, length);
			p = next;
			break;
		case 'h':
			types[i] = &ffi_type_uint32;
			wl_buffer_copy(&connection->fds_in,
				       &values[i].uint32,
				       sizeof values[i].uint32);
			connection->fds_in.tail += sizeof values[i].uint32;
			break;
		default:
			printf("unknown type\n");
			assert(0);
			break;
		}
		args[i] = &values[i];
	}

	ffi_prep_cif(&cif, FFI_DEFAULT_ABI, count, &ffi_type_uint32, types);
	ffi_call(&cif, func, &result, args);

	/* Slight hack here.  We store the tail of fds_in here and
	 * consume will set fds_in.tail to that value */
	connection->fds_in_tail = connection->fds_in.tail;
	connection->fds_in.tail = fds_tail;

 out:
	count = i;
	for (i = 2; i < count; i++) {
		switch (message->signature[i - 2]) {
		case 's':
			free(values[i].string);
			break;
		case 'a':
			free(values[i].array);
			break;
		}
	}

	return ret;
}
