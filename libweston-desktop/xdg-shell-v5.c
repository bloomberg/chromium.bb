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

#include <stdbool.h>

#include <wayland-server.h>

#include "compositor.h"
#include "zalloc.h"
#include "xdg-shell-unstable-v5-server-protocol.h"

#include "libweston-desktop.h"
#include "internal.h"

#define WD_XDG_SHELL_PROTOCOL_VERSION 1

struct weston_desktop_xdg_surface {
	struct wl_resource *resource;
	struct weston_desktop_surface *surface;
	struct weston_desktop *desktop;
	bool added;
	struct wl_event_source *add_idle;
	struct wl_event_source *configure_idle;
	uint32_t configure_serial;
	struct weston_size requested_size;
	struct {
		bool maximized;
		bool fullscreen;
		bool resizing;
		bool activated;
	} requested_state, next_state, state;
	bool has_next_geometry;
	struct weston_geometry next_geometry;
};

struct weston_desktop_xdg_popup {
	struct wl_resource *resource;
	struct weston_desktop_surface *popup;
	struct weston_desktop *desktop;
	struct weston_desktop_seat *seat;
	struct wl_display *display;
};

static void
weston_desktop_xdg_surface_ensure_added(struct weston_desktop_xdg_surface *surface)
{
	if (surface->added)
		return;

	if (surface->add_idle != NULL)
		wl_event_source_remove(surface->add_idle);
	surface->add_idle = NULL;
	weston_desktop_api_surface_added(surface->desktop, surface->surface);
	surface->added = true;
}

static void
weston_desktop_xdg_surface_send_configure(void *data)
{
	struct weston_desktop_xdg_surface *surface = data;
	uint32_t *s;
	struct wl_array states;

	surface->configure_idle = NULL;

	surface->configure_serial =
		wl_display_next_serial(weston_desktop_get_display(surface->desktop));

	wl_array_init(&states);
	if (surface->requested_state.maximized) {
		s = wl_array_add(&states, sizeof(uint32_t));
		*s = XDG_SURFACE_STATE_MAXIMIZED;
	}
	if (surface->requested_state.fullscreen) {
		s = wl_array_add(&states, sizeof(uint32_t));
		*s = XDG_SURFACE_STATE_FULLSCREEN;
	}
	if (surface->requested_state.resizing) {
		s = wl_array_add(&states, sizeof(uint32_t));
		*s = XDG_SURFACE_STATE_RESIZING;
	}
	if (surface->requested_state.activated) {
		s = wl_array_add(&states, sizeof(uint32_t));
		*s = XDG_SURFACE_STATE_ACTIVATED;
	}

	xdg_surface_send_configure(surface->resource,
				   surface->requested_size.width,
				   surface->requested_size.height,
				   &states,
				   surface->configure_serial);

	wl_array_release(&states);
};

static void
weston_desktop_xdg_surface_schedule_configure(struct weston_desktop_xdg_surface *surface)
{
	struct wl_display *display = weston_desktop_get_display(surface->desktop);
	struct wl_event_loop *loop = wl_display_get_event_loop(display);

	if (surface->configure_idle != NULL)
		return;
	surface->configure_idle =
		wl_event_loop_add_idle(loop,
				       weston_desktop_xdg_surface_send_configure,
				       surface);
}

static void
weston_desktop_xdg_surface_set_maximized(struct weston_desktop_surface *dsurface,
					 void *user_data, bool maximized)
{
	struct weston_desktop_xdg_surface *surface = user_data;

	if (surface->state.maximized == maximized)
		return;

	surface->requested_state.maximized = maximized;
	weston_desktop_xdg_surface_schedule_configure(surface);
}

static void
weston_desktop_xdg_surface_set_fullscreen(struct weston_desktop_surface *dsurface,
					  void *user_data, bool fullscreen)
{
	struct weston_desktop_xdg_surface *surface = user_data;

	if (surface->state.fullscreen == fullscreen)
		return;

	surface->requested_state.fullscreen = fullscreen;
	weston_desktop_xdg_surface_schedule_configure(surface);
}

static void
weston_desktop_xdg_surface_set_resizing(struct weston_desktop_surface *dsurface,
					void *user_data, bool resizing)
{
	struct weston_desktop_xdg_surface *surface = user_data;

	if (surface->state.resizing == resizing)
		return;

	surface->requested_state.resizing = resizing;
	weston_desktop_xdg_surface_schedule_configure(surface);
}

static void
weston_desktop_xdg_surface_set_activated(struct weston_desktop_surface *dsurface,
					 void *user_data, bool activated)
{
	struct weston_desktop_xdg_surface *surface = user_data;

	if (surface->state.activated == activated)
		return;

	surface->requested_state.activated = activated;
	weston_desktop_xdg_surface_schedule_configure(surface);
}

static void
weston_desktop_xdg_surface_set_size(struct weston_desktop_surface *dsurface,
				    void *user_data,
				    int32_t width, int32_t height)
{
	struct weston_desktop_xdg_surface *surface = user_data;
	struct weston_surface *wsurface = weston_desktop_surface_get_surface(surface->surface);

	surface->requested_size.width = width;
	surface->requested_size.height = height;

	if ((wsurface->width == width && wsurface->height == height) ||
	    (width == 0 && height == 0))
		return;

	weston_desktop_xdg_surface_schedule_configure(surface);
}

static void
weston_desktop_xdg_surface_committed(struct weston_desktop_surface *dsurface,
				     void *user_data,
				     int32_t sx, int32_t sy)
{
	struct weston_desktop_xdg_surface *surface = user_data;
	struct weston_surface *wsurface =
		weston_desktop_surface_get_surface(surface->surface);
	bool reconfigure = false;

	if (surface->next_state.maximized || surface->next_state.fullscreen)
		reconfigure = surface->requested_size.width != wsurface->width ||
			      surface->requested_size.height != wsurface->height;

	if (reconfigure) {
		weston_desktop_xdg_surface_schedule_configure(surface);
	} else {
		surface->state = surface->next_state;
		if (surface->has_next_geometry) {
			surface->has_next_geometry = false;
			weston_desktop_surface_set_geometry(surface->surface,
							    surface->next_geometry);
		}

		weston_desktop_xdg_surface_ensure_added(surface);
		weston_desktop_api_committed(surface->desktop, surface->surface,
					     sx, sy);
	}
}

static void
weston_desktop_xdg_surface_ping(struct weston_desktop_surface *dsurface,
				uint32_t serial, void *user_data)
{
	struct weston_desktop_client *client =
		weston_desktop_surface_get_client(dsurface);

	xdg_shell_send_ping(weston_desktop_client_get_resource(client),
			    serial);
}

static void
weston_desktop_xdg_surface_close(struct weston_desktop_surface *dsurface,
				 void *user_data)
{
	struct weston_desktop_xdg_surface *surface = user_data;

	xdg_surface_send_close(surface->resource);
}

static bool
weston_desktop_xdg_surface_get_maximized(struct weston_desktop_surface *dsurface,
					 void *user_data)
{
	struct weston_desktop_xdg_surface *surface = user_data;

	return surface->state.maximized;
}

static bool
weston_desktop_xdg_surface_get_fullscreen(struct weston_desktop_surface *dsurface,
					  void *user_data)
{
	struct weston_desktop_xdg_surface *surface = user_data;

	return surface->state.fullscreen;
}

static bool
weston_desktop_xdg_surface_get_resizing(struct weston_desktop_surface *dsurface,
					void *user_data)
{
	struct weston_desktop_xdg_surface *surface = user_data;

	return surface->state.resizing;
}

static bool
weston_desktop_xdg_surface_get_activated(struct weston_desktop_surface *dsurface,
					 void *user_data)
{
	struct weston_desktop_xdg_surface *surface = user_data;

	return surface->state.activated;
}

static void
weston_desktop_xdg_surface_destroy(struct weston_desktop_surface *dsurface,
				   void *user_data)
{
	struct weston_desktop_xdg_surface *surface = user_data;

	if (surface->added)
		weston_desktop_api_surface_removed(surface->desktop,
						   surface->surface);

	if (surface->add_idle != NULL)
		wl_event_source_remove(surface->add_idle);

	if (surface->configure_idle != NULL)
		wl_event_source_remove(surface->configure_idle);

	free(surface);
}

static void
weston_desktop_xdg_surface_protocol_set_parent(struct wl_client *wl_client,
					       struct wl_resource *resource,
					       struct wl_resource *parent_resource)
{
	struct weston_desktop_surface *dsurface =
		wl_resource_get_user_data(resource);
	struct weston_desktop_xdg_surface *surface =
		weston_desktop_surface_get_implementation_data(dsurface);
	struct weston_desktop_surface *parent = NULL;

	if (parent_resource != NULL)
		parent = wl_resource_get_user_data(parent_resource);

	weston_desktop_xdg_surface_ensure_added(surface);
	weston_desktop_api_set_parent(surface->desktop, dsurface, parent);
}

static void
weston_desktop_xdg_surface_protocol_set_title(struct wl_client *wl_client,
					      struct wl_resource *resource,
					      const char *title)
{
	struct weston_desktop_surface *surface =
		wl_resource_get_user_data(resource);

	weston_desktop_surface_set_title(surface, title);
}

static void
weston_desktop_xdg_surface_protocol_set_app_id(struct wl_client *wl_client,
					       struct wl_resource *resource,
					       const char *app_id)
{
	struct weston_desktop_surface *surface =
		wl_resource_get_user_data(resource);

	weston_desktop_surface_set_app_id(surface, app_id);
}

static void
weston_desktop_xdg_surface_protocol_show_window_menu(struct wl_client *wl_client,
						     struct wl_resource *resource,
						     struct wl_resource *seat_resource,
						     uint32_t serial,
						     int32_t x, int32_t y)
{
	struct weston_desktop_surface *dsurface =
		wl_resource_get_user_data(resource);
	struct weston_seat *seat =
		wl_resource_get_user_data(seat_resource);
	struct weston_desktop_xdg_surface *surface =
		weston_desktop_surface_get_implementation_data(dsurface);

	weston_desktop_xdg_surface_ensure_added(surface);
	weston_desktop_api_show_window_menu(surface->desktop, dsurface, seat, x, y);
}

static void
weston_desktop_xdg_surface_protocol_move(struct wl_client *wl_client,
					 struct wl_resource *resource,
					 struct wl_resource *seat_resource,
					 uint32_t serial)
{
	struct weston_desktop_surface *dsurface =
		wl_resource_get_user_data(resource);
	struct weston_seat *seat =
		wl_resource_get_user_data(seat_resource);
	struct weston_desktop_xdg_surface *surface =
		weston_desktop_surface_get_implementation_data(dsurface);

	weston_desktop_xdg_surface_ensure_added(surface);
	weston_desktop_api_move(surface->desktop, dsurface, seat, serial);
}

static void
weston_desktop_xdg_surface_protocol_resize(struct wl_client *wl_client,
					   struct wl_resource *resource,
					   struct wl_resource *seat_resource,
					   uint32_t serial,
					   enum xdg_surface_resize_edge edges)
{
	struct weston_desktop_surface *dsurface =
		wl_resource_get_user_data(resource);
	struct weston_seat *seat =
		wl_resource_get_user_data(seat_resource);
	struct weston_desktop_xdg_surface *surface =
		weston_desktop_surface_get_implementation_data(dsurface);
	enum weston_desktop_surface_edge surf_edges =
		(enum weston_desktop_surface_edge) edges;

	weston_desktop_xdg_surface_ensure_added(surface);
	weston_desktop_api_resize(surface->desktop, dsurface, seat, serial, surf_edges);
}

static void
weston_desktop_xdg_surface_protocol_ack_configure(struct wl_client *wl_client,
						  struct wl_resource *resource,
						  uint32_t serial)
{
	struct weston_desktop_surface *dsurface =
		wl_resource_get_user_data(resource);
	struct weston_desktop_xdg_surface *surface =
		weston_desktop_surface_get_implementation_data(dsurface);

	if (surface->configure_serial != serial)
		return;

	surface->next_state = surface->requested_state;
}

static void
weston_desktop_xdg_surface_protocol_set_window_geometry(struct wl_client *wl_client,
							struct wl_resource *resource,
							int32_t x, int32_t y,
							int32_t width, int32_t height)
{
	struct weston_desktop_surface *dsurface =
		wl_resource_get_user_data(resource);
	struct weston_desktop_xdg_surface *surface =
		weston_desktop_surface_get_implementation_data(dsurface);

	surface->has_next_geometry = true;
	surface->next_geometry.x = x;
	surface->next_geometry.y = y;
	surface->next_geometry.width = width;
	surface->next_geometry.height = height;
}

static void
weston_desktop_xdg_surface_protocol_set_maximized(struct wl_client *wl_client,
						  struct wl_resource *resource)
{
	struct weston_desktop_surface *dsurface =
		wl_resource_get_user_data(resource);
	struct weston_desktop_xdg_surface *surface =
		weston_desktop_surface_get_implementation_data(dsurface);

	weston_desktop_xdg_surface_ensure_added(surface);
	weston_desktop_api_maximized_requested(surface->desktop, dsurface, true);
}

static void
weston_desktop_xdg_surface_protocol_unset_maximized(struct wl_client *wl_client,
						    struct wl_resource *resource)
{
	struct weston_desktop_surface *dsurface =
		wl_resource_get_user_data(resource);
	struct weston_desktop_xdg_surface *surface =
		weston_desktop_surface_get_implementation_data(dsurface);

	weston_desktop_xdg_surface_ensure_added(surface);
	weston_desktop_api_maximized_requested(surface->desktop, dsurface, false);
}

static void
weston_desktop_xdg_surface_protocol_set_fullscreen(struct wl_client *wl_client,
						   struct wl_resource *resource,
						   struct wl_resource *output_resource)
{
	struct weston_desktop_surface *dsurface =
		wl_resource_get_user_data(resource);
	struct weston_desktop_xdg_surface *surface =
		weston_desktop_surface_get_implementation_data(dsurface);
	struct weston_output *output = NULL;

	if (output_resource != NULL)
		output = wl_resource_get_user_data(output_resource);

	weston_desktop_xdg_surface_ensure_added(surface);
	weston_desktop_api_fullscreen_requested(surface->desktop, dsurface,
						true, output);
}

static void
weston_desktop_xdg_surface_protocol_unset_fullscreen(struct wl_client *wl_client,
					       struct wl_resource *resource)
{
	struct weston_desktop_surface *dsurface =
		wl_resource_get_user_data(resource);
	struct weston_desktop_xdg_surface *surface =
		weston_desktop_surface_get_implementation_data(dsurface);

	weston_desktop_xdg_surface_ensure_added(surface);
	weston_desktop_api_fullscreen_requested(surface->desktop, dsurface,
						false, NULL);
}

static void
weston_desktop_xdg_surface_protocol_set_minimized(struct wl_client *wl_client,
						  struct wl_resource *resource)
{
	struct weston_desktop_surface *dsurface =
		wl_resource_get_user_data(resource);
	struct weston_desktop_xdg_surface *surface =
		weston_desktop_surface_get_implementation_data(dsurface);

	weston_desktop_xdg_surface_ensure_added(surface);
	weston_desktop_api_minimized_requested(surface->desktop, dsurface);
}

static const struct xdg_surface_interface weston_desktop_xdg_surface_implementation = {
	.destroy             = weston_desktop_destroy_request,
	.set_parent          = weston_desktop_xdg_surface_protocol_set_parent,
	.set_title           = weston_desktop_xdg_surface_protocol_set_title,
	.set_app_id          = weston_desktop_xdg_surface_protocol_set_app_id,
	.show_window_menu    = weston_desktop_xdg_surface_protocol_show_window_menu,
	.move                = weston_desktop_xdg_surface_protocol_move,
	.resize              = weston_desktop_xdg_surface_protocol_resize,
	.ack_configure       = weston_desktop_xdg_surface_protocol_ack_configure,
	.set_window_geometry = weston_desktop_xdg_surface_protocol_set_window_geometry,
	.set_maximized       = weston_desktop_xdg_surface_protocol_set_maximized,
	.unset_maximized     = weston_desktop_xdg_surface_protocol_unset_maximized,
	.set_fullscreen      = weston_desktop_xdg_surface_protocol_set_fullscreen,
	.unset_fullscreen    = weston_desktop_xdg_surface_protocol_unset_fullscreen,
	.set_minimized       = weston_desktop_xdg_surface_protocol_set_minimized,
};

static const struct weston_desktop_surface_implementation weston_desktop_xdg_surface_internal_implementation = {
	.set_maximized = weston_desktop_xdg_surface_set_maximized,
	.set_fullscreen = weston_desktop_xdg_surface_set_fullscreen,
	.set_resizing = weston_desktop_xdg_surface_set_resizing,
	.set_activated = weston_desktop_xdg_surface_set_activated,
	.set_size = weston_desktop_xdg_surface_set_size,
	.committed = weston_desktop_xdg_surface_committed,
	.ping = weston_desktop_xdg_surface_ping,
	.close = weston_desktop_xdg_surface_close,

	.get_maximized = weston_desktop_xdg_surface_get_maximized,
	.get_fullscreen = weston_desktop_xdg_surface_get_fullscreen,
	.get_resizing = weston_desktop_xdg_surface_get_resizing,
	.get_activated = weston_desktop_xdg_surface_get_activated,

	.destroy = weston_desktop_xdg_surface_destroy,
};

static void
weston_desktop_xdg_popup_close(struct weston_desktop_surface *dsurface,
			       void *user_data)
{
	struct weston_desktop_xdg_popup *popup = user_data;

	xdg_popup_send_popup_done(popup->resource);
}

static void
weston_desktop_xdg_popup_destroy(struct weston_desktop_surface *dsurface,
				 void *user_data)
{
	struct weston_desktop_xdg_popup *popup = user_data;
	struct weston_desktop_surface *topmost;
	struct weston_desktop_client *client =
		weston_desktop_surface_get_client(popup->popup);

	if (!weston_desktop_surface_get_grab(popup->popup))
		goto end;

	topmost = weston_desktop_seat_popup_grab_get_topmost_surface(popup->seat);
	if (topmost != popup->popup) {
		struct wl_resource *client_resource =
			weston_desktop_client_get_resource(client);

		wl_resource_post_error(client_resource,
				       XDG_SHELL_ERROR_NOT_THE_TOPMOST_POPUP,
				       "xdg_popup was destroyed while it was not the topmost popup.");
	}

	weston_desktop_surface_popup_ungrab(popup->popup, popup->seat);

end:
	free(popup);
}

static const struct xdg_popup_interface weston_desktop_xdg_popup_implementation = {
	.destroy             = weston_desktop_destroy_request,
};

static const struct weston_desktop_surface_implementation weston_desktop_xdg_popup_internal_implementation = {
	.close = weston_desktop_xdg_popup_close,

	.destroy = weston_desktop_xdg_popup_destroy,
};

static void
weston_desktop_xdg_shell_protocol_use_unstable_version(struct wl_client *wl_client,
						       struct wl_resource *resource,
						       int32_t version)
{
	if (version > 1) {
		wl_resource_post_error(resource,
				       1, "xdg_shell version not supported");
		return;
	}
}

static void
weston_desktop_xdg_surface_add_idle_callback(void *user_data)
{
	struct weston_desktop_xdg_surface *surface = user_data;

	surface->add_idle = NULL;
	weston_desktop_xdg_surface_ensure_added(surface);
}

static void
weston_desktop_xdg_shell_protocol_get_xdg_surface(struct wl_client *wl_client,
						  struct wl_resource *resource,
						  uint32_t id,
						  struct wl_resource *surface_resource)
{
	struct weston_desktop_client *client =
		wl_resource_get_user_data(resource);
	struct weston_desktop *desktop =
		weston_desktop_client_get_desktop(client);
	struct weston_surface *wsurface =
		wl_resource_get_user_data(surface_resource);
	struct weston_desktop_xdg_surface *surface;
	struct wl_display *display = weston_desktop_get_display(desktop);
	struct wl_event_loop *loop = wl_display_get_event_loop(display);

	if (weston_surface_set_role(wsurface, "xdg_surface", resource, XDG_SHELL_ERROR_ROLE) < 0)
		return;

	surface = zalloc(sizeof(struct weston_desktop_xdg_surface));
	if (surface == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}

	surface->desktop = desktop;

	surface->surface =
		weston_desktop_surface_create(surface->desktop, client,
					      wsurface,
					      &weston_desktop_xdg_surface_internal_implementation,
					      surface);
	if (surface->surface == NULL) {
		free(surface);
		return;
	}

	surface->resource =
		weston_desktop_surface_add_resource(surface->surface,
						    &xdg_surface_interface,
						    &weston_desktop_xdg_surface_implementation,
						    id, NULL);
	if (surface->resource == NULL)
		return;

	surface->add_idle =
		wl_event_loop_add_idle(loop,
				       weston_desktop_xdg_surface_add_idle_callback,
				       surface);
}

static void
weston_desktop_xdg_shell_protocol_get_xdg_popup(struct wl_client *wl_client,
						struct wl_resource *resource,
						uint32_t id,
						struct wl_resource *surface_resource,
						struct wl_resource *parent_resource,
						struct wl_resource *seat_resource,
						uint32_t serial,
						int32_t x, int32_t y)
{
	struct weston_desktop_client *client =
		wl_resource_get_user_data(resource);
	struct weston_surface *wsurface =
		wl_resource_get_user_data(surface_resource);
	struct weston_surface *wparent =
		wl_resource_get_user_data(parent_resource);
	struct weston_seat *wseat = wl_resource_get_user_data(seat_resource);
	struct weston_desktop_seat *seat = weston_desktop_seat_from_seat(wseat);
	struct weston_desktop_surface *parent, *topmost;
	bool parent_is_popup, parent_is_xdg;
	struct weston_desktop_xdg_popup *popup;

	if (weston_surface_set_role(wsurface, "xdg_popup", resource, XDG_SHELL_ERROR_ROLE) < 0)
		return;

	if (!weston_surface_is_desktop_surface(wparent)) {
		wl_resource_post_error(resource,
				       XDG_SHELL_ERROR_INVALID_POPUP_PARENT,
				       "xdg_popup parent was invalid");
		return;
	}

	parent = weston_surface_get_desktop_surface(wparent);
	parent_is_xdg =
		weston_desktop_surface_has_implementation(parent,
							  &weston_desktop_xdg_surface_internal_implementation);
	parent_is_popup =
		weston_desktop_surface_has_implementation(parent,
							  &weston_desktop_xdg_popup_internal_implementation);

	if (!parent_is_xdg && !parent_is_popup) {
		wl_resource_post_error(resource,
				       XDG_SHELL_ERROR_INVALID_POPUP_PARENT,
				       "xdg_popup parent was invalid");
		return;
	}

	topmost = weston_desktop_seat_popup_grab_get_topmost_surface(seat);
	if ((topmost == NULL && parent_is_popup) ||
	    (topmost != NULL && topmost != parent)) {
		wl_resource_post_error(resource,
				       XDG_SHELL_ERROR_NOT_THE_TOPMOST_POPUP,
				       "xdg_popup was not created on the topmost popup");
		return;
	}

	popup = zalloc(sizeof(struct weston_desktop_xdg_popup));
	if (popup == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}

	popup->desktop = weston_desktop_client_get_desktop(client);
	popup->display = weston_desktop_get_display(popup->desktop);
	popup->seat = seat;

	popup->popup =
		weston_desktop_surface_create(popup->desktop, client, wsurface,
					      &weston_desktop_xdg_popup_internal_implementation,
					      popup);
	if (popup->popup == NULL) {
		free(popup);
		return;
	}

	popup->resource =
		weston_desktop_surface_add_resource(popup->popup,
						    &xdg_popup_interface,
						    &weston_desktop_xdg_popup_implementation,
						    id, NULL);
	if (popup->resource == NULL)
		return;

	weston_desktop_surface_set_relative_to(popup->popup, parent, x, y, false);
	weston_desktop_surface_popup_grab(popup->popup, popup->seat, serial);
}

static void
weston_desktop_xdg_shell_protocol_pong(struct wl_client *wl_client,
				       struct wl_resource *resource,
				       uint32_t serial)
{
	struct weston_desktop_client *client =
		wl_resource_get_user_data(resource);

	weston_desktop_client_pong(client, serial);
}

static const struct xdg_shell_interface weston_desktop_xdg_shell_implementation = {
	.destroy = weston_desktop_destroy_request,
	.use_unstable_version = weston_desktop_xdg_shell_protocol_use_unstable_version,
	.get_xdg_surface = weston_desktop_xdg_shell_protocol_get_xdg_surface,
	.get_xdg_popup = weston_desktop_xdg_shell_protocol_get_xdg_popup,
	.pong = weston_desktop_xdg_shell_protocol_pong,
};

static int
xdg_shell_unversioned_dispatch(const void *implementation,
			       void *_target, uint32_t opcode,
			       const struct wl_message *message,
			       union wl_argument *args)
{
	struct wl_resource *resource = _target;
	struct weston_desktop_client *client =
		wl_resource_get_user_data(resource);

	if (opcode != 1 /* XDG_SHELL_USE_UNSTABLE_VERSION */) {
		wl_resource_post_error(resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "must call use_unstable_version first");
		return 0;
	}

#define XDG_SERVER_VERSION 5

	if (args[0].i != XDG_SERVER_VERSION) {
		wl_resource_post_error(resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "incompatible version, server is %d " "client wants %d",
				       XDG_SERVER_VERSION, args[0].i);
		return 0;
	}

	wl_resource_set_implementation(resource,
				       &weston_desktop_xdg_shell_implementation,
				       client, implementation);

	return 1;
}

static void
weston_desktop_xdg_shell_bind(struct wl_client *client, void *data,
			      uint32_t version, uint32_t id)
{
	struct weston_desktop *desktop = data;

	weston_desktop_client_create(desktop, client,
				     xdg_shell_unversioned_dispatch,
				     &xdg_shell_interface, NULL, version, id);
}

struct wl_global *
weston_desktop_xdg_shell_v5_create(struct weston_desktop *desktop,
				   struct wl_display *display)
{
	return wl_global_create(display,
				&xdg_shell_interface,
				WD_XDG_SHELL_PROTOCOL_VERSION,
				desktop, weston_desktop_xdg_shell_bind);
}
