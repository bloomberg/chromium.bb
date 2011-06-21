/*
 * Copyright © 2011 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <xcb/xcb.h>

#include <wayland-server.h>

#include "compositor.h"

/*
 * TODO:
 *  - Clean X socket and lock file on exit
 *  - Nuke lock file if process doesn't exist.
 */

struct wlsc_xserver {
	struct wl_display *wl_display;
	struct wl_event_loop *loop;
	struct wl_event_source *source;
	struct wl_event_source *sigchld_source;
	struct wl_client *client;
	int fd;
	struct sockaddr_un addr;
	char lock_addr[113];
	int display;
	struct wlsc_process process;
};

static int
wlsc_xserver_handle_event(int listen_fd, uint32_t mask, void *data)
{
	struct wlsc_xserver *mxs = data;
	char listen_fd_str[8], display[8], s[8];
	int sv[2], flags;

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) < 0) {
		fprintf(stderr, "socketpair failed\n");
		return 1;
	}

	mxs->process.pid = fork();
	switch (mxs->process.pid) {
	case 0:
		/* SOCK_CLOEXEC closes both ends, so we need to unset
		 * the flag on the client fd. */
		flags = fcntl(sv[1], F_GETFD);
		if (flags != -1)
			fcntl(sv[1], F_SETFD, flags & ~FD_CLOEXEC);

		flags = fcntl(listen_fd, F_GETFD);
		if (flags != -1)
			fcntl(listen_fd, F_SETFD, flags & ~FD_CLOEXEC);

		snprintf(s, sizeof s, "%d", sv[1]);
		setenv("WAYLAND_SOCKET", s, 1);

		snprintf(listen_fd_str, sizeof listen_fd_str, "%d", listen_fd);
		snprintf(display, sizeof display, ":%d", mxs->display);

		if (execl("/usr/bin/Xorg",
			  "/usr/bin/Xorg",
			  display,
			  "-wayland",
			  // "-rootless",
			  "-retro",
			  "-logfile", "/tmp/foo",
			  "-terminate",
			  "-socket", listen_fd_str,
			  NULL) < 0)
			fprintf(stderr, "exec failed: %m\n");
		exit(-1);

	default:
		fprintf(stderr, "forked X server, pid %d\n", mxs->process.pid);

		close(sv[1]);
		mxs->client = wl_client_create(mxs->wl_display, sv[0]);

		wlsc_watch_process(&mxs->process);

		wl_event_source_remove(mxs->source);
		break;

	case -1:
		fprintf(stderr, "failed to fork\n");
		break;
	}

	return 1;
}

static void
wlsc_xserver_cleanup(struct wlsc_process *process, int status)
{
	struct wlsc_xserver *mxs =
		container_of(process, struct wlsc_xserver, process);

	fprintf(stderr, "xserver exited, code %d\n", status);
	mxs->process.pid = 0;
	mxs->source =
		wl_event_loop_add_fd(mxs->loop, mxs->fd, WL_EVENT_READABLE,
				     wlsc_xserver_handle_event, mxs);
}

int
wlsc_xserver_init(struct wl_display *display)
{
	struct wlsc_xserver *mxs;
	char lockfile[256], pid[16];
	socklen_t size, name_size;
	int fd;

	mxs = malloc(sizeof *mxs);
	memset(mxs, 0, sizeof mxs);

	mxs->process.cleanup = wlsc_xserver_cleanup;
	mxs->wl_display = display;
	mxs->addr.sun_family = AF_LOCAL;
	mxs->fd = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (mxs->fd < 0) {
		free(mxs);
		return -1;
	}

	mxs->display = 0;
	do {
		snprintf(lockfile, sizeof lockfile,
			 "/tmp/.X%d-lock", mxs->display);
		fd = open(lockfile,
			  O_WRONLY | O_CLOEXEC | O_CREAT | O_EXCL, 0444);
		if (fd < 0 && errno == EEXIST) {
			fprintf(stderr, "server %d exists\n", mxs->display);
			mxs->display++;
			continue;
		} else if (fd < 0) {
			close(mxs->fd);
			free(mxs);
			return -1;
		}

		size = snprintf(pid, sizeof pid, "%10d\n", getpid());
		write(fd, pid, size);
		close(fd);

		name_size = snprintf(mxs->addr.sun_path,
				     sizeof mxs->addr.sun_path,
				     "/tmp/.X11-unix/X%d", mxs->display) + 1;
		size = offsetof(struct sockaddr_un, sun_path) + name_size;
		if (bind(mxs->fd, (struct sockaddr *) &mxs->addr, size) < 0) {
			fprintf(stderr, "failed to bind to %s (%m)\n",
				mxs->addr.sun_path);
			unlink(lockfile);
			close(mxs->fd);
			free(mxs);
			return -1;
		}
		break;
	} while (errno != 0);

	fprintf(stderr, "listening on display %d\n", mxs->display);

	if (listen(mxs->fd, 1) < 0) {
		unlink(mxs->addr.sun_path);
		unlink(lockfile);
		close(mxs->fd);
		free(mxs);
		return -1;
	}

	mxs->loop = wl_display_get_event_loop(display);
	mxs->source =
		wl_event_loop_add_fd(mxs->loop, mxs->fd, WL_EVENT_READABLE,
				     wlsc_xserver_handle_event, mxs);

	return 0;
}
