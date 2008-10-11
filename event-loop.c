#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <unistd.h>
#include "wayland.h"

struct wl_event_loop {
	int epoll_fd;
	wl_event_loop_idle_func_t idle_func;
	void *idle_data;
};

struct wl_event_source {
	int fd;
	wl_event_loop_fd_func_t func;
	void *data;
};

struct wl_event_source *
wl_event_loop_add_fd(struct wl_event_loop *loop,
		     int fd, uint32_t mask,
		     wl_event_loop_fd_func_t func,
		     void *data)
{
	struct wl_event_source *source;
	struct epoll_event ep;

	source = malloc(sizeof *source);
	source->fd = fd;
	source->func = func;
	source->data = data;

	ep.events = 0;
	if (mask & WL_EVENT_READABLE)
		ep.events |= EPOLLIN;
	if (mask & WL_EVENT_WRITEABLE)
		ep.events |= EPOLLOUT;
	ep.data.ptr = source;

	if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ep) < 0) {
		free(source);
		return NULL;
	}

	return source;
}

struct wl_event_source idle_source;

int
wl_event_loop_remove_source(struct wl_event_loop *loop,
			    struct wl_event_source *source)
{
	int fd;

	if (source == &idle_source) {
		loop->idle_func = NULL;
		return 0;
	} else {
		fd = source->fd;
		free(source);
		return epoll_ctl(loop->epoll_fd,
				 EPOLL_CTL_DEL, fd, NULL);
	};
}

int
wl_event_loop_update_source(struct wl_event_loop *loop,
			    struct wl_event_source *source,
			    uint32_t mask)
{
	struct epoll_event ep;

	ep.events = 0;
	if (mask & WL_EVENT_READABLE)
		ep.events |= EPOLLIN;
	if (mask & WL_EVENT_WRITEABLE)
		ep.events |= EPOLLOUT;
	ep.data.ptr = source;

	return epoll_ctl(loop->epoll_fd,
			 EPOLL_CTL_MOD, source->fd, &ep);
}

struct wl_event_loop *
wl_event_loop_create(void)
{
	struct wl_event_loop *loop;

	loop = malloc(sizeof *loop);
	if (loop == NULL)
		return NULL;

	loop->epoll_fd = epoll_create(16);
	if (loop->epoll_fd < 0) {
		free(loop);
		return NULL;
	}

	return loop;
}

void
wl_event_loop_destroy(struct wl_event_loop *loop)
{
	close(loop->epoll_fd);
	free(loop);
}

struct wl_event_source *
wl_event_loop_add_idle(struct wl_event_loop *loop,
		       wl_event_loop_idle_func_t func,
		       void *data)
{
	loop->idle_func = func;
	loop->idle_data = data;

	return &idle_source;
}

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

int
wl_event_loop_wait(struct wl_event_loop *loop)
{
	struct epoll_event ep[32];
	struct wl_event_source *source;
	int i, count, timeout;
	uint32_t mask;

	if (loop->idle_func)
		timeout = 0;
	else
		timeout = -1;

	count = epoll_wait(loop->epoll_fd, ep, ARRAY_LENGTH(ep), timeout);
	if (count < 0)
		return -1;

	for (i = 0; i < count; i++) {
		source = ep[i].data.ptr;
		mask = 0;
		if (ep[i].events & EPOLLIN)
			mask |= WL_EVENT_READABLE;
		if (ep[i].events & EPOLLOUT)
			mask |= WL_EVENT_WRITEABLE;

		source->func(source->fd, mask, source->data);
	}

	if (count == 0 && loop->idle_func != NULL) {
		loop->idle_func(loop->idle_data);
		loop->idle_func = NULL;
	}
	
	return 0;
}
