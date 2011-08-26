/*
 * Copyright © 2010 Intel Corporation
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>

#include "wayland-server.h"
#include "compositor.h"

struct wl_shell {
	struct wlsc_shell shell;
};

struct wlsc_move_grab {
	struct wl_grab grab;
	struct wlsc_surface *surface;
	int32_t dx, dy;
};

static void
move_grab_motion(struct wl_grab *grab,
		   uint32_t time, int32_t x, int32_t y)
{
	struct wlsc_move_grab *move = (struct wlsc_move_grab *) grab;
	struct wlsc_surface *es = move->surface;

	wlsc_surface_configure(es, x + move->dx, y + move->dy,
			       es->width, es->height);
}

static void
move_grab_button(struct wl_grab *grab,
		 uint32_t time, int32_t button, int32_t state)
{
}

static void
move_grab_end(struct wl_grab *grab, uint32_t time)
{
	struct wlsc_surface *es;
	struct wl_input_device *device = grab->input_device;
	int32_t sx, sy;

	es = pick_surface(grab->input_device, &sx, &sy);
	wl_input_device_set_pointer_focus(device,
					  &es->surface, time,
					  device->x, device->y, sx, sy);
	free(grab);
}

static const struct wl_grab_interface move_grab_interface = {
	move_grab_motion,
	move_grab_button,
	move_grab_end
};

static int
wlsc_surface_move(struct wlsc_surface *es,
		  struct wlsc_input_device *wd, uint32_t time)
{
	struct wlsc_move_grab *move;

	/* FIXME: Reject if fullscreen */

	move = malloc(sizeof *move);
	if (!move)
		return -1;

	move->grab.interface = &move_grab_interface;
	move->dx = es->x - wd->input_device.grab_x;
	move->dy = es->y - wd->input_device.grab_y;
	move->surface = es;

	if (wl_input_device_update_grab(&wd->input_device,
					&move->grab, &es->surface, time) < 0)
		return 0;

	wlsc_input_device_set_pointer_image(wd, WLSC_POINTER_DRAGGING);
	wl_input_device_set_pointer_focus(&wd->input_device,
					  NULL, time, 0, 0, 0, 0);

	return 0;
}

static void
shell_move(struct wl_client *client, struct wl_resource *resource,
	   struct wl_resource *surface_resource,
	   struct wl_resource *input_resource, uint32_t time)
{
	struct wlsc_input_device *wd = input_resource->data;
	struct wlsc_surface *es = surface_resource->data;

	if (wlsc_surface_move(es, wd, time) < 0)
		wl_client_post_no_memory(client);
}

struct wlsc_resize_grab {
	struct wl_grab grab;
	uint32_t edges;
	int32_t dx, dy, width, height;
	struct wlsc_surface *surface;
	struct wl_resource *resource;
};

static void
resize_grab_motion(struct wl_grab *grab,
		   uint32_t time, int32_t x, int32_t y)
{
	struct wlsc_resize_grab *resize = (struct wlsc_resize_grab *) grab;
	struct wl_input_device *device = grab->input_device;
	struct wl_surface *surface = &resize->surface->surface;
	int32_t width, height;

	if (resize->edges & WL_SHELL_RESIZE_LEFT) {
		width = device->grab_x - x + resize->width;
	} else if (resize->edges & WL_SHELL_RESIZE_RIGHT) {
		width = x - device->grab_x + resize->width;
	} else {
		width = resize->width;
	}

	if (resize->edges & WL_SHELL_RESIZE_TOP) {
		height = device->grab_y - y + resize->height;
	} else if (resize->edges & WL_SHELL_RESIZE_BOTTOM) {
		height = y - device->grab_y + resize->height;
	} else {
		height = resize->height;
	}

	wl_resource_post_event(resize->resource,
			       WL_SHELL_CONFIGURE, time, resize->edges,
			       surface, width, height);
}

static void
resize_grab_button(struct wl_grab *grab,
		   uint32_t time, int32_t button, int32_t state)
{
}

static void
resize_grab_end(struct wl_grab *grab, uint32_t time)
{
	struct wlsc_surface *es;
	struct wl_input_device *device = grab->input_device;
	int32_t sx, sy;

	es = pick_surface(grab->input_device, &sx, &sy);
	wl_input_device_set_pointer_focus(device,
					  &es->surface, time,
					  device->x, device->y, sx, sy);
	free(grab);
}

static const struct wl_grab_interface resize_grab_interface = {
	resize_grab_motion,
	resize_grab_button,
	resize_grab_end
};

static int
wlsc_surface_resize(struct wlsc_surface *es,
		    struct wlsc_input_device *wd,
		    uint32_t time, uint32_t edges,
		    struct wl_resource *resource)
{
	struct wlsc_resize_grab *resize;
	enum wlsc_pointer_type pointer = WLSC_POINTER_LEFT_PTR;

	/* FIXME: Reject if fullscreen */

	resize = malloc(sizeof *resize);
	if (!resize)
		return -1;

	resize->grab.interface = &resize_grab_interface;
	resize->edges = edges;
	resize->dx = es->x - wd->input_device.grab_x;
	resize->dy = es->y - wd->input_device.grab_y;
	resize->width = es->width;
	resize->height = es->height;
	resize->surface = es;
	resize->resource = resource;

	if (edges == 0 || edges > 15 ||
	    (edges & 3) == 3 || (edges & 12) == 12)
		return 0;

	switch (edges) {
	case WL_SHELL_RESIZE_TOP:
		pointer = WLSC_POINTER_TOP;
		break;
	case WL_SHELL_RESIZE_BOTTOM:
		pointer = WLSC_POINTER_BOTTOM;
		break;
	case WL_SHELL_RESIZE_LEFT:
		pointer = WLSC_POINTER_LEFT;
		break;
	case WL_SHELL_RESIZE_TOP_LEFT:
		pointer = WLSC_POINTER_TOP_LEFT;
		break;
	case WL_SHELL_RESIZE_BOTTOM_LEFT:
		pointer = WLSC_POINTER_BOTTOM_LEFT;
		break;
	case WL_SHELL_RESIZE_RIGHT:
		pointer = WLSC_POINTER_RIGHT;
		break;
	case WL_SHELL_RESIZE_TOP_RIGHT:
		pointer = WLSC_POINTER_TOP_RIGHT;
		break;
	case WL_SHELL_RESIZE_BOTTOM_RIGHT:
		pointer = WLSC_POINTER_BOTTOM_RIGHT;
		break;
	}

	if (wl_input_device_update_grab(&wd->input_device,
					&resize->grab, &es->surface, time) < 0)
		return 0;

	wlsc_input_device_set_pointer_image(wd, pointer);
	wl_input_device_set_pointer_focus(&wd->input_device,
					  NULL, time, 0, 0, 0, 0);

	return 0;
}

static void
shell_resize(struct wl_client *client, struct wl_resource *resource,
	     struct wl_resource *surface_resource,
	     struct wl_resource *input_resource, uint32_t time, uint32_t edges)
{
	struct wlsc_input_device *wd = input_resource->data;
	struct wlsc_surface *es = surface_resource->data;

	/* FIXME: Reject if fullscreen */

	if (wlsc_surface_resize(es, wd, time, edges, resource) < 0)
		wl_client_post_no_memory(client);
}

static void
shell_set_toplevel(struct wl_client *client,
		   struct wl_resource *resource,
		   struct wl_resource *surface_resource)

{
	struct wlsc_surface *es = surface_resource->data;
	struct wlsc_compositor *ec = es->compositor;

	if (es->map_type == WLSC_SURFACE_MAP_FULLSCREEN) {
		es->x = es->saved_x;
		es->y = es->saved_y;
	} else if (es->map_type == WLSC_SURFACE_MAP_UNMAPPED) {
		es->x = 10 + random() % 400;
		es->y = 10 + random() % 400;
		/* assign to first output */
		es->output = container_of(ec->output_list.next,
					  struct wlsc_output, link);
	}

	wlsc_surface_damage(es);
	es->map_type = WLSC_SURFACE_MAP_TOPLEVEL;
	es->fullscreen_output = NULL;
}

static void
shell_set_transient(struct wl_client *client,
		    struct wl_resource *resource,
		    struct wl_resource *surface_resource,
		    struct wl_resource *parent_resource,
		    int x, int y, uint32_t flags)
{
	struct wlsc_surface *es = surface_resource->data;
	struct wlsc_surface *pes = parent_resource->data;

	/* assign to parents output  */
	es->output = pes->output;
 
	es->x = pes->x + x;
	es->y = pes->y + y;

	wlsc_surface_damage(es);
	es->map_type = WLSC_SURFACE_MAP_TRANSIENT;
}

static void
shell_set_fullscreen(struct wl_client *client,
		     struct wl_resource *resource,
		     struct wl_resource *surface_resource)

{
	struct wlsc_surface *es = surface_resource->data;
	struct wlsc_output *output;

	/* FIXME: Fullscreen on first output */
	/* FIXME: Handle output going away */
	output = container_of(es->compositor->output_list.next,
			      struct wlsc_output, link);
	es->output = output;

	es->saved_x = es->x;
	es->saved_y = es->y;
	es->x = (output->current->width - es->width) / 2;
	es->y = (output->current->height - es->height) / 2;
	es->fullscreen_output = output;
	wlsc_surface_damage(es);
	es->map_type = WLSC_SURFACE_MAP_FULLSCREEN;
}

static void
destroy_drag(struct wl_resource *resource)
{
	struct wl_drag *drag =
		container_of(resource, struct wl_drag, resource);

	wl_list_remove(&drag->drag_focus_listener.link);
	if (drag->grab.input_device)
		wl_input_device_end_grab(drag->grab.input_device,
					 wlsc_compositor_get_time());

	free(drag);
}


static void
wl_drag_set_pointer_focus(struct wl_drag *drag,
			  struct wl_surface *surface, uint32_t time,
			  int32_t x, int32_t y, int32_t sx, int32_t sy)
{
	char **p, **end;

	if (drag->drag_focus == surface)
		return;

	if (drag->drag_focus &&
	    (!surface ||
	     drag->drag_focus->resource.client != surface->resource.client))
		wl_resource_post_event(&drag->drag_offer.resource,
				      WL_DRAG_OFFER_POINTER_FOCUS,
				      time, NULL, 0, 0, 0, 0);

	if (surface &&
	    (!drag->drag_focus ||
	     drag->drag_focus->resource.client != surface->resource.client)) {
		
		drag->drag_offer.resource.client = surface->resource.client;
		end = drag->types.data + drag->types.size;
		for (p = drag->types.data; p < end; p++)
			wl_resource_post_event(&drag->drag_offer.resource,
					       WL_DRAG_OFFER_OFFER, *p);
	}

	if (surface) {
		wl_resource_post_event(&drag->drag_offer.resource,
				       WL_DRAG_OFFER_POINTER_FOCUS,
				       time, surface,
				       x, y, sx, sy);

	}

	drag->drag_focus = surface;
	drag->pointer_focus_time = time;
	drag->target = NULL;

	wl_list_remove(&drag->drag_focus_listener.link);
	if (surface)
		wl_list_insert(surface->resource.destroy_listener_list.prev,
			       &drag->drag_focus_listener.link);
}

static void
drag_offer_accept(struct wl_client *client, struct wl_resource *resource,
		  uint32_t time, const char *type)
{
	struct wl_drag_offer *offer = resource->data;
	struct wl_drag *drag = container_of(offer, struct wl_drag, drag_offer);
	char **p, **end;

	/* If the client responds to drag pointer_focus or motion
	 * events after the pointer has left the surface, we just
	 * discard the accept requests.  The drag source just won't
	 * get the corresponding 'target' events and eventually the
	 * next surface/root will start sending events. */
	if (time < drag->pointer_focus_time)
		return;

	drag->target = client;
	drag->type = NULL;
	end = drag->types.data + drag->types.size;
	for (p = drag->types.data; p < end; p++)
		if (type && strcmp(*p, type) == 0)
			drag->type = *p;

	wl_resource_post_event(&drag->resource, WL_DRAG_TARGET, drag->type);
}

static void
drag_offer_receive(struct wl_client *client,
		   struct wl_resource *resource, int fd)
{
	struct wl_drag_offer *offer = resource->data;
	struct wl_drag *drag = container_of(offer, struct wl_drag, drag_offer);

	wl_resource_post_event(&drag->resource, WL_DRAG_FINISH, fd);
	close(fd);
}

static void
drag_offer_reject(struct wl_client *client, struct wl_resource *resource)
{
	struct wl_drag_offer *offer = resource->data;
	struct wl_drag *drag = container_of(offer, struct wl_drag, drag_offer);

	wl_resource_post_event(&drag->resource, WL_DRAG_REJECT);
}

static const struct wl_drag_offer_interface drag_offer_interface = {
	drag_offer_accept,
	drag_offer_receive,
	drag_offer_reject
};

static void
drag_offer(struct wl_client *client,
	   struct wl_resource *resource, const char *type)
{
	struct wl_drag *drag = resource->data;
	char **p;

	p = wl_array_add(&drag->types, sizeof *p);
	if (p)
		*p = strdup(type);
	if (!p || !*p)
		wl_client_post_no_memory(client);
}

static void
drag_grab_motion(struct wl_grab *grab,
		   uint32_t time, int32_t x, int32_t y)
{
	struct wl_drag *drag = container_of(grab, struct wl_drag, grab);
	struct wlsc_surface *es;
	int32_t sx, sy;

	es = pick_surface(grab->input_device, &sx, &sy);
	wl_drag_set_pointer_focus(drag, &es->surface, time, x, y, sx, sy);
	if (es)
		wl_resource_post_event(&drag->drag_offer.resource,
				       WL_DRAG_OFFER_MOTION,
				       time, x, y, sx, sy);
}

static void
drag_grab_button(struct wl_grab *grab,
		 uint32_t time, int32_t button, int32_t state)
{
}

static void
drag_grab_end(struct wl_grab *grab, uint32_t time)
{
	struct wl_drag *drag = container_of(grab, struct wl_drag, grab);
	struct wlsc_surface *es;
	struct wl_input_device *device = grab->input_device;
	int32_t sx, sy;

	if (drag->target)
		wl_resource_post_event(&drag->drag_offer.resource,
				       WL_DRAG_OFFER_DROP);

	wl_drag_set_pointer_focus(drag, NULL, time, 0, 0, 0, 0);

	es = pick_surface(grab->input_device, &sx, &sy);
	wl_input_device_set_pointer_focus(device,
					  &es->surface, time,
					  device->x, device->y, sx, sy);
}

static const struct wl_grab_interface drag_grab_interface = {
	drag_grab_motion,
	drag_grab_button,
	drag_grab_end
};

static void
drag_activate(struct wl_client *client,
	      struct wl_resource *resource,
	      struct wl_resource *surface_resource,
	      struct wl_resource *device_resource, uint32_t time)
{
	struct wl_drag *drag = resource->data;
	struct wl_surface *surface = surface_resource->data;
	struct wl_input_device *device = device_resource->data;
	struct wl_display *display = wl_client_get_display (client);
	struct wlsc_surface *target;
	int32_t sx, sy;

	if (wl_input_device_update_grab(device,
					&drag->grab, surface, time) < 0)
		return;

	drag->grab.interface = &drag_grab_interface;

	drag->source = surface;

	drag->drag_offer.resource.object.interface = &wl_drag_offer_interface;
	drag->drag_offer.resource.object.implementation =
		(void (**)(void)) &drag_offer_interface;

	wl_display_add_global(display, &wl_drag_offer_interface, drag, NULL);

	target = pick_surface(device, &sx, &sy);
	wl_input_device_set_pointer_focus(device, NULL, time, 0, 0, 0, 0);
	wl_drag_set_pointer_focus(drag, &target->surface, time,
				  device->x, device->y, sx, sy);
}

static void
drag_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource, wlsc_compositor_get_time());
}

static const struct wl_drag_interface drag_interface = {
	drag_offer,
	drag_activate,
	drag_destroy,
};

static void
drag_handle_surface_destroy(struct wl_listener *listener,
			    struct wl_resource *resource, uint32_t time)
{
	struct wl_drag *drag =
		container_of(listener, struct wl_drag, drag_focus_listener);
	struct wl_surface *surface = (struct wl_surface *) resource;

	if (drag->drag_focus == surface)
		wl_drag_set_pointer_focus(drag, NULL, time, 0, 0, 0, 0);
}

static void
shell_create_drag(struct wl_client *client,
		  struct wl_resource *resource, uint32_t id)
{
	struct wl_drag *drag;

	drag = malloc(sizeof *drag);
	if (drag == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	memset(drag, 0, sizeof *drag);
	drag->resource.object.id = id;
	drag->resource.object.interface = &wl_drag_interface;
	drag->resource.object.implementation =
		(void (**)(void)) &drag_interface;

	drag->resource.destroy = destroy_drag;

	drag->drag_focus_listener.func = drag_handle_surface_destroy;
	wl_list_init(&drag->drag_focus_listener.link);

	wl_client_add_resource(client, &drag->resource);
}

static void
wlsc_selection_set_focus(struct wlsc_shell *shell,
			 struct wl_selection *selection,
			 struct wl_surface *surface, uint32_t time)
{
	char **p, **end;

	if (selection->selection_focus == surface)
		return;

	if (selection->selection_focus != NULL)
		wl_resource_post_event(&selection->selection_offer.resource,
				     WL_SELECTION_OFFER_KEYBOARD_FOCUS,
				     NULL);

	if (surface) {

		selection->selection_offer.resource.client = surface->resource.client;
		end = selection->types.data + selection->types.size;
		for (p = selection->types.data; p < end; p++)
			wl_resource_post_event(&selection->selection_offer.resource,
					       WL_SELECTION_OFFER_OFFER, *p);

		wl_list_remove(&selection->selection_focus_listener.link);
		wl_list_insert(surface->resource.destroy_listener_list.prev,
			       &selection->selection_focus_listener.link);

		wl_resource_post_event(&selection->selection_offer.resource,
				       WL_SELECTION_OFFER_KEYBOARD_FOCUS,
				       selection->input_device);
	}

	selection->selection_focus = surface;

	wl_list_remove(&selection->selection_focus_listener.link);
	if (surface)
		wl_list_insert(surface->resource.destroy_listener_list.prev,
			       &selection->selection_focus_listener.link);
}

static void
selection_offer_receive(struct wl_client *client,
			struct wl_resource *resource,
			const char *mime_type, int fd)
{
	struct wl_selection_offer *offer = resource->data;
	struct wl_selection *selection =
		container_of(offer, struct wl_selection, selection_offer);

	wl_resource_post_event(&selection->resource,
			       WL_SELECTION_SEND, mime_type, fd);
	close(fd);
}

static const struct wl_selection_offer_interface selection_offer_interface = {
	selection_offer_receive
};

static void
selection_offer(struct wl_client *client,
		struct wl_resource *resource, const char *type)
{
	struct wl_selection *selection = resource->data;
	char **p;

	p = wl_array_add(&selection->types, sizeof *p);
	if (p)
		*p = strdup(type);
	if (!p || !*p)
		wl_client_post_no_memory(client);
}

static void
selection_activate(struct wl_client *client,
		   struct wl_resource *resource,
		   struct wl_resource *input_resource, uint32_t time)
{
	struct wl_selection *selection = resource->data;
	struct wlsc_input_device *wd = input_resource->data;
	struct wl_display *display = wl_client_get_display (client);
	struct wlsc_compositor *compositor =
		(struct wlsc_compositor *) wd->input_device.compositor;

	selection->input_device = &wd->input_device;

	selection->selection_offer.resource.object.interface =
		&wl_selection_offer_interface;
	selection->selection_offer.resource.object.implementation =
		(void (**)(void)) &selection_offer_interface;

	wl_display_add_global(display,
			      &wl_selection_offer_interface, selection, NULL);

	if (wd->selection) {
		wl_resource_post_event(&wd->selection->resource,
				       WL_SELECTION_CANCELLED);
	}
	wd->selection = selection;

	wlsc_selection_set_focus(compositor->shell, selection,
				 wd->input_device.keyboard_focus, time);
}

static void
selection_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource, wlsc_compositor_get_time());
}

static const struct wl_selection_interface selection_interface = {
	selection_offer,
	selection_activate,
	selection_destroy
};

static void
destroy_selection(struct wl_resource *resource)
{
	struct wl_selection *selection =
		container_of(resource, struct wl_selection, resource);
	struct wlsc_input_device *wd =
		(struct wlsc_input_device *) selection->input_device;
	struct wlsc_compositor *compositor =
		(struct wlsc_compositor *) wd->input_device.compositor;

	if (wd && wd->selection == selection) {
		wd->selection = NULL;
		wlsc_selection_set_focus(compositor->shell, 
					 selection, NULL,
					 wlsc_compositor_get_time());
	}

	wl_list_remove(&selection->selection_focus_listener.link);
	free(selection);
}

static void
selection_handle_surface_destroy(struct wl_listener *listener,
				 struct wl_resource *resource, uint32_t time)
{
}

static void
shell_create_selection(struct wl_client *client,
		       struct wl_resource *resource, uint32_t id)
{
	struct wl_selection *selection;

	selection = malloc(sizeof *selection);
	if (selection == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	memset(selection, 0, sizeof *selection);
	selection->resource.object.id = id;
	selection->resource.object.interface = &wl_selection_interface;
	selection->resource.object.implementation =
		(void (**)(void)) &selection_interface;

	selection->client = client;
	selection->resource.destroy = destroy_selection;
	selection->selection_focus = NULL;

	selection->selection_focus_listener.func =
		selection_handle_surface_destroy;
	wl_list_init(&selection->selection_focus_listener.link);

	wl_client_add_resource(client, &selection->resource);
}

const static struct wl_shell_interface shell_interface = {
	shell_move,
	shell_resize,
	shell_create_drag,
	shell_create_selection,
	shell_set_toplevel,
	shell_set_transient,
	shell_set_fullscreen
};

static void
move_binding(struct wl_input_device *device, uint32_t time,
	     uint32_t key, uint32_t button, uint32_t state, void *data)
{
	struct wlsc_surface *surface =
		(struct wlsc_surface *) device->pointer_focus;

	if (surface == NULL)
		return;

	wlsc_surface_move(surface, (struct wlsc_input_device *) device, time);
}

static void
resize_binding(struct wl_input_device *device, uint32_t time,
	       uint32_t key, uint32_t button, uint32_t state, void *data)
{
	struct wlsc_surface *surface =
		(struct wlsc_surface *) device->pointer_focus;
	struct wl_resource *resource;
	uint32_t edges = 0;
	int32_t x, y;

	if (surface == NULL)
		return;

	x = device->grab_x - surface->x;
	y = device->grab_y - surface->y;

	if (x < surface->width / 3)
		edges |= WL_SHELL_RESIZE_LEFT;
	else if (x < 2 * surface->width / 3)
		edges |= 0;
	else
		edges |= WL_SHELL_RESIZE_RIGHT;

	if (y < surface->height / 3)
		edges |= WL_SHELL_RESIZE_TOP;
	else if (y < 2 * surface->height / 3)
		edges |= 0;
	else
		edges |= WL_SHELL_RESIZE_BOTTOM;

	resource = /* Find shell resource for surface client */ 0;

	/* ... or use wl_shell_surface */

	wlsc_surface_resize(surface, (struct wlsc_input_device *) device,
			    time, edges, resource);
}

static void
lock(struct wlsc_shell *shell)
{
}

static void
attach(struct wlsc_shell *shell, struct wlsc_surface *es)
{
	if (es->map_type == WLSC_SURFACE_MAP_FULLSCREEN) {
		es->x = (es->fullscreen_output->current->width - es->width) / 2;
		es->y = (es->fullscreen_output->current->height - es->height) / 2;
	}
}

static void
bind_shell(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_shell *shell = data;

	wl_client_add_object(client, &wl_shell_interface,
			     &shell_interface, id, shell);
}

int
shell_init(struct wlsc_compositor *ec);

WL_EXPORT int
shell_init(struct wlsc_compositor *ec)
{
	struct wl_shell *shell;

	shell = malloc(sizeof *shell);
	if (shell == NULL)
		return -1;

	shell->shell.lock = lock;
	shell->shell.attach = attach;
	shell->shell.set_selection_focus = wlsc_selection_set_focus;

	if (wl_display_add_global(ec->wl_display, &wl_shell_interface,
				  shell, bind_shell) == NULL)
		return -1;

	wlsc_compositor_add_binding(ec, 0, BTN_LEFT, MODIFIER_SUPER,
				    move_binding, shell);
	wlsc_compositor_add_binding(ec, 0, BTN_MIDDLE, MODIFIER_SUPER,
				    resize_binding, shell);

	ec->shell = &shell->shell;

	return 0;
}
