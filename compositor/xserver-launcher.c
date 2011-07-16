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
#include "xserver-server-protocol.h"

/*
 * TODO:
 *  - Clean X socket and lock file on exit
 *  - Nuke lock file if process doesn't exist.
 *
 * WM TODO:
 *  - Send take focus, hook into wlsc_surface_activate.
 */

struct xserver {
	struct wl_object object;
};

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

	struct xserver xserver;

	struct wlsc_wm *wm;
};

struct wlsc_wm {
	xcb_connection_t *conn;
	struct wl_event_source *source;
	xcb_screen_t *screen;
	struct wl_hash_table *window_hash;
	struct {
		xcb_atom_t		 wm_protocols;
		xcb_atom_t		 wm_take_focus;
		xcb_atom_t		 wm_delete_window;
		xcb_atom_t		 net_wm_name;
		xcb_atom_t		 net_wm_icon;
		xcb_atom_t		 net_wm_state;
		xcb_atom_t		 net_wm_state_fullscreen;
		xcb_atom_t		 utf8_string;
	} atom;
};

struct wlsc_wm_window {
	xcb_window_t id;
	struct wlsc_surface *surface;
};

static void
wlsc_wm_handle_configure_request(struct wlsc_wm *wm, xcb_generic_event_t *event)
{
	xcb_configure_request_event_t *configure_request = 
		(xcb_configure_request_event_t *) event;
	uint32_t values[16];
	int i = 0;

	fprintf(stderr, "XCB_CONFIGURE_REQUEST\n");

	if (configure_request->value_mask & XCB_CONFIG_WINDOW_X)
		values[i++] = configure_request->x;
	if (configure_request->value_mask & XCB_CONFIG_WINDOW_Y)
		values[i++] = configure_request->y;
	if (configure_request->value_mask & XCB_CONFIG_WINDOW_WIDTH)
		values[i++] = configure_request->width;
	if (configure_request->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
		values[i++] = configure_request->height;
	if (configure_request->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
		values[i++] = configure_request->border_width;
	if (configure_request->value_mask & XCB_CONFIG_WINDOW_SIBLING)
		values[i++] = configure_request->sibling;
	if (configure_request->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
		values[i++] = configure_request->stack_mode;

	xcb_configure_window(wm->conn,
			     configure_request->window,
			     configure_request->value_mask, values);
}

static void
wlsc_wm_get_properties(struct wlsc_wm *wm, xcb_window_t window)
{
	xcb_generic_error_t *e;
	xcb_get_property_reply_t *reply;
	void *value;
	int i;

	struct {
		xcb_atom_t atom;
		xcb_get_property_cookie_t cookie;
	} props[] = {
		{ XCB_ATOM_WM_CLASS, },
		{ XCB_ATOM_WM_TRANSIENT_FOR },
		{ wm->atom.wm_protocols, },
		{ wm->atom.net_wm_name, },
	};

	for (i = 0; i < ARRAY_LENGTH(props); i++)
		props[i].cookie =
			xcb_get_property (wm->conn, 
					  0, /* delete */
					  window,
					  props[i].atom,
					  XCB_ATOM_ANY,
					  0, 2048);

	for (i = 0; i < ARRAY_LENGTH(props); i++)  {
		reply = xcb_get_property_reply(wm->conn, props[i].cookie, &e);
		value = xcb_get_property_value(reply);

		fprintf(stderr, "property %d, type %d, format %d, "
			"length %d (value_len %d), value \"%s\"\n",
			props[i].atom,
			reply->type, reply->format,
			xcb_get_property_value_length(reply), reply->value_len,
			reply->type ? (char *) value : "(nil)");

		free(reply);
	}
}

static void
wlsc_wm_activate(struct wlsc_wm *wm,
		 struct wlsc_wm_window *window, xcb_timestamp_t time)
{
	xcb_set_input_focus (wm->conn,
			     XCB_INPUT_FOCUS_POINTER_ROOT, window->id, time);
}

static void
wlsc_wm_handle_map_request(struct wlsc_wm *wm, xcb_generic_event_t *event)
{
	xcb_map_request_event_t *map_request =
		(xcb_map_request_event_t *) event;
	uint32_t values[1];

	fprintf(stderr, "XCB_MAP_REQUEST\n");

	wlsc_wm_get_properties(wm, map_request->window);
	values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE;
	xcb_change_window_attributes(wm->conn, map_request->window,
				     XCB_CW_EVENT_MASK, values);

	xcb_map_window(wm->conn, map_request->window);
}

static void
wlsc_wm_handle_map_notify(struct wlsc_wm *wm, xcb_generic_event_t *event)
{
	xcb_map_notify_event_t *map_notify =
		(xcb_map_notify_event_t *) event;
	struct wlsc_wm_window *window;
	xcb_client_message_event_t client_message;

	fprintf(stderr, "XCB_MAP_NOTIFY\n");

	wlsc_wm_get_properties(wm, map_notify->window);

	window = wl_hash_table_lookup(wm->window_hash, map_notify->window);
	wlsc_wm_activate(wm, window, XCB_TIME_CURRENT_TIME);

	client_message.response_type = XCB_CLIENT_MESSAGE;
	client_message.format = 32;
	client_message.window = window->id;
	client_message.type = wm->atom.wm_protocols;
	client_message.data.data32[0] = wm->atom.wm_take_focus;
	client_message.data.data32[1] = XCB_TIME_CURRENT_TIME;

	xcb_send_event(wm->conn, 0, window->id, 
		       XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
		       (char *) &client_message);

}

static void
wlsc_wm_handle_property_notify(struct wlsc_wm *wm, xcb_generic_event_t *event)
{
	xcb_property_notify_event_t *property_notify =
		(xcb_property_notify_event_t *) event;

	fprintf(stderr, "XCB_PROPERTY_NOTIFY\n");

	if (property_notify->atom == XCB_ATOM_WM_CLASS) {
		fprintf(stderr, "wm_class changed\n");
	} else if (property_notify->atom == XCB_ATOM_WM_TRANSIENT_FOR) {
		fprintf(stderr, "wm_transient_for changed\n");
	} else if (property_notify->atom == wm->atom.wm_protocols) {
		fprintf(stderr, "wm_protocols changed\n");
	} else if (property_notify->atom == wm->atom.net_wm_name) {
		fprintf(stderr, "wm_class changed\n");
	} else {
		fprintf(stderr, "unhandled property change: %d\n",
			property_notify->atom);
	}
}

static void
wlsc_wm_handle_create_notify(struct wlsc_wm *wm, xcb_generic_event_t *event)
{
	xcb_create_notify_event_t *create_notify =
		(xcb_create_notify_event_t *) event;
	struct wlsc_wm_window *window;

	fprintf(stderr, "XCB_CREATE_NOTIFY, win %d\n", create_notify->window);

	window = malloc(sizeof *window);
	if (window == NULL) {
		fprintf(stderr, "failed to allocate window\n");
		return;
	}

	window->id = create_notify->window;
	wl_hash_table_insert(wm->window_hash, window->id, window);

	fprintf(stderr, "created window %p\n", window);
}

static void
wlsc_wm_handle_destroy_notify(struct wlsc_wm *wm, xcb_generic_event_t *event)
{
	xcb_destroy_notify_event_t *destroy_notify =
		(xcb_destroy_notify_event_t *) event;
	struct wlsc_wm_window *window;

	fprintf(stderr, "XCB_DESTROY_NOTIFY, win %d\n",
		destroy_notify->window);

	window = wl_hash_table_lookup(wm->window_hash, destroy_notify->window);
	if (window == NULL) {
		fprintf(stderr, "destroy notify for unknow window %d\n",
			destroy_notify->window);
		return;
	}

	fprintf(stderr, "destroy window %p\n", window);
	wl_hash_table_remove(wm->window_hash, window->id);
	free(window);
}

static int
wlsc_wm_handle_event(int fd, uint32_t mask, void *data)
{
	struct wlsc_wm *wm = data;
	xcb_generic_event_t *event;
	int count = 0;

	while (event = xcb_poll_for_event(wm->conn), event != NULL) {
		switch (event->response_type) {
		case XCB_CREATE_NOTIFY:
			wlsc_wm_handle_create_notify(wm, event);
			break;
		case XCB_MAP_REQUEST:
			wlsc_wm_handle_map_request(wm, event);
			break;
		case XCB_MAP_NOTIFY:
			wlsc_wm_handle_map_notify(wm, event);
			break;
		case XCB_UNMAP_NOTIFY:
			fprintf(stderr, "XCB_UNMAP_NOTIFY\n");
			break;
		case XCB_CONFIGURE_REQUEST:
			wlsc_wm_handle_configure_request(wm, event);
			break;
		case XCB_CONFIGURE_NOTIFY:
			fprintf(stderr, "XCB_CONFIGURE_NOTIFY\n");
			break;
		case XCB_DESTROY_NOTIFY:
			wlsc_wm_handle_destroy_notify(wm, event);
			break;
		case XCB_MAPPING_NOTIFY:
			fprintf(stderr, "XCB_MAPPING_NOTIFY\n");
			break;
		case XCB_PROPERTY_NOTIFY:
			wlsc_wm_handle_property_notify(wm, event);
			break;
		default:
			fprintf(stderr, "Unhandled event %d\n",
				event->response_type);
			break;
		}
		free(event);
		count++;
	}

	xcb_flush(wm->conn);

	return count;
}

static void
wxs_wm_get_resources(struct wlsc_wm *wm)
{

#define F(field) offsetof(struct wlsc_wm, field)

	static const struct { const char *name; int offset; } atoms[] = {
		{ "WM_PROTOCOLS",	F(atom.wm_protocols) },
		{ "WM_TAKE_FOCUS",	F(atom.wm_take_focus) },
		{ "WM_DELETE_WINDOW",	F(atom.wm_delete_window) },
		{ "_NET_WM_NAME",	F(atom.net_wm_name) },
		{ "_NET_WM_ICON",	F(atom.net_wm_icon) },
		{ "_NET_WM_STATE",	F(atom.net_wm_state) },
		{ "_NET_WM_STATE_FULLSCREEN", F(atom.net_wm_state_fullscreen) },
		{ "UTF8_STRING",	F(atom.utf8_string) },
	};

	xcb_intern_atom_cookie_t cookies[ARRAY_LENGTH(atoms)];
	xcb_intern_atom_reply_t *reply;
	int i;

	for (i = 0; i < ARRAY_LENGTH(atoms); i++)
		cookies[i] = xcb_intern_atom (wm->conn, 0,
					      strlen(atoms[i].name),
					      atoms[i].name);

	for (i = 0; i < ARRAY_LENGTH(atoms); i++) {
		reply = xcb_intern_atom_reply (wm->conn, cookies[i], NULL);
		*(xcb_atom_t *) ((char *) wm + atoms[i].offset) = reply->atom;
		free(reply);
	}
}

static struct wlsc_wm *
wlsc_wm_create(struct wlsc_xserver *wxs)
{
	struct wlsc_wm *wm;
	struct wl_event_loop *loop;
	xcb_screen_iterator_t s;
	uint32_t values[1];
	int sv[2];

	wm = malloc(sizeof *wm);
	if (wm == NULL)
		return NULL;

	wm->window_hash = wl_hash_table_create();
	if (wm->window_hash == NULL) {
		free(wm);
		return NULL;
	}

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) < 0) {
		fprintf(stderr, "socketpair failed\n");
		wl_hash_table_destroy(wm->window_hash);
		free(wm);
		return NULL;
	}

	wl_client_post_event(wxs->client,
			     &wxs->xserver.object,
			     XSERVER_CLIENT, sv[1]);
	wl_client_flush(wxs->client);
	close(sv[1]);
	
	wm->conn = xcb_connect_to_fd(sv[0], NULL);
	if (xcb_connection_has_error(wm->conn)) {
		fprintf(stderr, "xcb_connect_to_fd failed\n");
		close(sv[0]);
		wl_hash_table_destroy(wm->window_hash);
		free(wm);
		return NULL;
	}

	s = xcb_setup_roots_iterator(xcb_get_setup(wm->conn));
	wm->screen = s.data;

	loop = wl_display_get_event_loop(wxs->wl_display);
	wm->source =
		wl_event_loop_add_fd(loop, sv[0],
				     WL_EVENT_READABLE,
				     wlsc_wm_handle_event, wm);
	wl_event_source_check(wm->source);

	wxs_wm_get_resources(wm);

	values[0] =
		XCB_EVENT_MASK_STRUCTURE_NOTIFY |
		XCB_EVENT_MASK_RESIZE_REDIRECT |
		XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
		XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
		XCB_EVENT_MASK_PROPERTY_CHANGE;
	xcb_change_window_attributes(wm->conn, wm->screen->root,
				     XCB_CW_EVENT_MASK, values);

	xcb_flush(wm->conn);
	fprintf(stderr, "created wm\n");

	return wm;
}

static void
wlsc_xserver_bind(struct wl_client *client,
		  struct wl_object *global,
		  uint32_t version)
{
	struct wlsc_xserver *wxs =
		container_of(global, struct wlsc_xserver, xserver.object);

	wxs->wm = wlsc_wm_create(wxs);
	if (wxs == NULL) {
		fprintf(stderr, "failed to create wm\n");
	}

	wl_client_post_event(wxs->client,
			     &wxs->xserver.object,
			     XSERVER_LISTEN_SOCKET, wxs->fd);
}

static int
wlsc_xserver_handle_event(int listen_fd, uint32_t mask, void *data)
{
	struct wlsc_xserver *mxs = data;
	char display[8], s[8], logfile[32];
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

		snprintf(s, sizeof s, "%d", sv[1]);
		setenv("WAYLAND_SOCKET", s, 1);

		snprintf(display, sizeof display, ":%d", mxs->display);
		snprintf(logfile, sizeof logfile,
			 "/tmp/x-log-%d", mxs->display);

		if (execl("/usr/bin/Xorg",
			  "/usr/bin/Xorg",
			  display,
			  "-wayland",
			  "-rootless",
			  "-retro",
			  "-logfile", logfile,
			  "-nolisten", "all",
			  "-terminate",
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

static void
xserver_set_window_id(struct wl_client *client, struct xserver *xserver,
		      struct wl_surface *surface, uint32_t id)
{
	struct wlsc_xserver *wxs =
		container_of(xserver, struct wlsc_xserver, xserver);
	struct wlsc_wm *wm = wxs->wm;
	struct wlsc_wm_window *window;

	window = wl_hash_table_lookup(wm->window_hash, id);
	if (window == NULL) {
		fprintf(stderr, "set_window_id for unknown window %d\n", id);
		return;
	}

	fprintf(stderr, "set_window_id %d for surface %p\n", id, surface);

	window->surface = (struct wlsc_surface *) surface;
	/* FIXME: Do we need a surface destroy listener? */
}

static const struct xserver_interface xserver_implementation = {
	xserver_set_window_id
};

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

	mxs->xserver.object.interface = &xserver_interface;
	mxs->xserver.object.implementation =
		(void (**)(void)) &xserver_implementation;
	wl_display_add_object(display, &mxs->xserver.object);
	wl_display_add_global(display,
			      &mxs->xserver.object, wlsc_xserver_bind);

	return 0;
}
