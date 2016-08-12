/*
 * Copyright © 2010-2012 Intel Corporation
 * Copyright © 2011-2012 Collabora, Ltd.
 * Copyright © 2013 Raspberry Pi Foundation
 * Copyright © 2016 Quentin "Sardem FF7" Glidic
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <assert.h>

#include <wayland-server.h>

#include "compositor.h"
#include "zalloc.h"

#include "libweston-desktop.h"
#include "internal.h"

enum weston_desktop_xwayland_surface_state {
	NONE,
	TOPLEVEL,
	MAXIMIZED,
	FULLSCREEN,
	TRANSIENT,
	XWAYLAND,
};

struct weston_desktop_xwayland {
	struct weston_desktop *desktop;
	struct weston_desktop_client *client;
	struct weston_layer layer;
};

struct shell_surface {
	struct weston_desktop_xwayland *xwayland;
	struct weston_desktop *desktop;
	struct weston_desktop_surface *surface;
	struct wl_listener resource_destroy_listener;
	struct weston_view *view;
	const struct weston_shell_client *client;
	struct weston_geometry next_geometry;
	bool has_next_geometry;
	bool added;
	enum weston_desktop_xwayland_surface_state state;
};

static void
weston_desktop_xwayland_surface_change_state(struct shell_surface *surface,
					     enum weston_desktop_xwayland_surface_state state,
					     struct weston_desktop_surface *parent,
					     int32_t x, int32_t y)
{
	bool to_add = (parent == NULL && state != XWAYLAND);

	assert(state != NONE);

	if (to_add && surface->added)
		return;

	if (surface->state != state) {
		if (surface->state == XWAYLAND) {
			weston_desktop_surface_destroy_view(surface->view);
			surface->view = NULL;
		}

		if (to_add) {
			weston_desktop_surface_unset_relative_to(surface->surface);
			weston_desktop_api_surface_added(surface->desktop,
							 surface->surface);
		} else if (surface->added) {
			weston_desktop_api_surface_removed(surface->desktop,
							   surface->surface);
		}

		if (state == XWAYLAND) {
			surface->view =
				weston_desktop_surface_create_view(surface->surface);
			weston_layer_entry_insert(&surface->xwayland->layer.view_list,
						  &surface->view->layer_link);
			weston_view_set_position(surface->view, x, y);
		}

		surface->state = state;
		surface->added = to_add;
	}

	if (parent != NULL)
		weston_desktop_surface_set_relative_to(surface->surface, parent,
						       x, y, false);
}

static void
weston_desktop_xwayland_surface_committed(struct weston_desktop_surface *dsurface,
					  void *user_data, bool new_buffer,
					  int32_t sx, int32_t sy)
{
	struct shell_surface *surface = user_data;

	if (surface->has_next_geometry) {
		surface->has_next_geometry = false;
		weston_desktop_surface_set_geometry(surface->surface,
						    surface->next_geometry);
	}

	if (surface->added)
		weston_desktop_api_committed(surface->desktop, surface->surface,
					     sx, sy);
}

static void
weston_desktop_xwayland_surface_set_size(struct weston_desktop_surface *dsurface,
					  void *user_data,
					  int32_t width, int32_t height)
{
	struct shell_surface *surface = user_data;

	surface->client->send_configure(weston_desktop_surface_get_surface(surface->surface),
				       width, height);
}

static void
weston_desktop_xwayland_surface_destroy(struct weston_desktop_surface *dsurface,
					void *user_data)
{
	struct shell_surface *surface = user_data;

	wl_list_remove(&surface->resource_destroy_listener.link);

	weston_desktop_surface_unset_relative_to(surface->surface);
	if (surface->added)
		weston_desktop_api_surface_removed(surface->desktop,
						   surface->surface);
	else if (surface->state == XWAYLAND)
		weston_desktop_surface_destroy_view(surface->view);

	free(surface);
}

static bool
weston_desktop_xwayland_surface_get_maximized(struct weston_desktop_surface *dsurface,
					       void *user_data)
{
	struct shell_surface *surface = user_data;

	return surface->state == MAXIMIZED;
}

static bool
weston_desktop_xwayland_surface_get_fullscreen(struct weston_desktop_surface *dsurface,
					        void *user_data)
{
	struct shell_surface *surface = user_data;

	return surface->state == FULLSCREEN;
}

static const struct weston_desktop_surface_implementation weston_desktop_xwayland_surface_internal_implementation = {
	.committed = weston_desktop_xwayland_surface_committed,
	.set_size = weston_desktop_xwayland_surface_set_size,

	.get_maximized = weston_desktop_xwayland_surface_get_maximized,
	.get_fullscreen = weston_desktop_xwayland_surface_get_fullscreen,

	.destroy = weston_desktop_xwayland_surface_destroy,
};

static void
weston_destop_xwayland_resource_destroyed(struct wl_listener *listener,
					  void *data)
{
	struct shell_surface *surface =
		wl_container_of(listener, surface, resource_destroy_listener);

	weston_desktop_surface_destroy(surface->surface);
}

static struct shell_surface *
create_shell_surface(void *shell,
		     struct weston_surface *wsurface,
		     const struct weston_shell_client *client)
{
	struct weston_desktop_xwayland *xwayland = shell;
	struct shell_surface *surface;

	surface = zalloc(sizeof(struct shell_surface));
	if (surface == NULL)
		return NULL;

	surface->xwayland = xwayland;
	surface->desktop = xwayland->desktop;
	surface->client = client;

	surface->surface =
		weston_desktop_surface_create(surface->desktop,
					      xwayland->client, wsurface,
					      &weston_desktop_xwayland_surface_internal_implementation,
					      surface);
	if (surface->surface == NULL) {
		free(surface);
		return NULL;
	}

	surface->resource_destroy_listener.notify =
		weston_destop_xwayland_resource_destroyed;
	wl_resource_add_destroy_listener(wsurface->resource,
					 &surface->resource_destroy_listener);

	return surface;
}

static void
set_toplevel(struct shell_surface *surface)
{
	weston_desktop_xwayland_surface_change_state(surface, TOPLEVEL, NULL,
						     0, 0);
}

static void
set_transient(struct shell_surface *surface,
	      struct weston_surface *wparent, int x, int y, uint32_t flags)
{
	struct weston_desktop_surface *parent;

	if (!weston_surface_is_desktop_surface(wparent))
		return;

	parent = weston_surface_get_desktop_surface(wparent);
	if (flags & WL_SHELL_SURFACE_TRANSIENT_INACTIVE) {
		weston_desktop_xwayland_surface_change_state(surface, TRANSIENT,
							     parent, x, y);
	} else {
		weston_desktop_xwayland_surface_change_state(surface, TOPLEVEL,
							     NULL, 0, 0);
		weston_desktop_api_set_parent(surface->desktop,
					      surface->surface, parent);
	}
}

static void
set_fullscreen(struct shell_surface *surface, uint32_t method,
	       uint32_t framerate, struct weston_output *output)
{
	weston_desktop_xwayland_surface_change_state(surface, FULLSCREEN, NULL,
						     0, 0);
	weston_desktop_api_fullscreen_requested(surface->desktop,
						surface->surface, true, output);
}

static void
set_xwayland(struct shell_surface *surface, int x, int y,
	     uint32_t flags)
{
	weston_desktop_xwayland_surface_change_state(surface, XWAYLAND, NULL,
						     x, y);
}

static int
move(struct shell_surface *surface,
     struct weston_pointer *pointer)
{
	if (surface->state == TOPLEVEL ||
	    surface->state == MAXIMIZED ||
	    surface->state == FULLSCREEN)
		weston_desktop_api_move(surface->desktop, surface->surface,
					pointer->seat, pointer->grab_serial);
	return 0;
}

static int
resize(struct shell_surface *surface,
       struct weston_pointer *pointer, uint32_t edges)
{
	if (surface->state == TOPLEVEL ||
	    surface->state == MAXIMIZED ||
	    surface->state == FULLSCREEN)
		weston_desktop_api_resize(surface->desktop, surface->surface,
					  pointer->seat, pointer->grab_serial,
					  edges);
	return 0;
}

static void
set_title(struct shell_surface *surface, const char *title)
{
	weston_desktop_surface_set_title(surface->surface, title);
}

static void
set_window_geometry(struct shell_surface *surface,
		    int32_t x, int32_t y, int32_t width, int32_t height)
{
	surface->has_next_geometry = true;
	surface->next_geometry.x = x;
	surface->next_geometry.y = y;
	surface->next_geometry.width = width;
	surface->next_geometry.height = height;
}

static void
set_maximized(struct shell_surface *surface)
{
	weston_desktop_xwayland_surface_change_state(surface, MAXIMIZED, NULL,
						     0, 0);
	weston_desktop_api_maximized_requested(surface->desktop,
					       surface->surface, true);
}

static void
set_pid(struct shell_surface *surface, pid_t pid)
{
}

void
weston_desktop_xwayland_init(struct weston_desktop *desktop)
{
	struct weston_compositor *compositor = weston_desktop_get_compositor(desktop);
	struct weston_desktop_xwayland *xwayland;

	xwayland = zalloc(sizeof(struct weston_desktop_xwayland));
	if (xwayland == NULL)
		return;

	xwayland->desktop = desktop;
	xwayland->client = weston_desktop_client_create(desktop, NULL, NULL, NULL, NULL, 0, 0);

	weston_layer_init(&xwayland->layer, &compositor->cursor_layer.link);

	compositor->shell_interface.shell = xwayland;
	compositor->shell_interface.create_shell_surface = create_shell_surface;
	compositor->shell_interface.set_toplevel = set_toplevel;
	compositor->shell_interface.set_transient = set_transient;
	compositor->shell_interface.set_fullscreen = set_fullscreen;
	compositor->shell_interface.set_xwayland = set_xwayland;
	compositor->shell_interface.move = move;
	compositor->shell_interface.resize = resize;
	compositor->shell_interface.set_title = set_title;
	compositor->shell_interface.set_window_geometry = set_window_geometry;
	compositor->shell_interface.set_maximized = set_maximized;
	compositor->shell_interface.set_pid = set_pid;
}
