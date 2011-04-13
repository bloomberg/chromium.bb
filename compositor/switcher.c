/*
 * Copyright © 2011 Intel Corporation
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
#include <stdio.h>
#include <linux/input.h>

#include "compositor.h"

struct wlsc_switcher {
	struct wlsc_compositor *compositor;
	struct wlsc_surface *current;
	struct wl_listener listener;
};

static void
wlsc_switcher_next(struct wlsc_switcher *switcher)
{
	struct wl_list *l;

	wlsc_surface_damage(switcher->current);
	l = switcher->current->link.next;
	if (l == &switcher->compositor->surface_list)
		l = switcher->compositor->surface_list.next;
	switcher->current = container_of(l, struct wlsc_surface, link);
	wl_list_remove(&switcher->listener.link);
	wl_list_insert(switcher->current->surface.destroy_listener_list.prev,
		       &switcher->listener.link);
	switcher->compositor->overlay = switcher->current;
	wlsc_surface_damage(switcher->current);
}

static void
switcher_handle_surface_destroy(struct wl_listener *listener,
				struct wl_surface *surface, uint32_t time)
{
	struct wlsc_switcher *switcher =
		container_of(listener, struct wlsc_switcher, listener);

	wlsc_switcher_next(switcher);
}

static struct wlsc_switcher *
wlsc_switcher_create(struct wlsc_compositor *compositor)
{
	struct wlsc_switcher *switcher;

	switcher = malloc(sizeof *switcher);
	switcher->compositor = compositor;
	switcher->current = container_of(compositor->surface_list.next,
					 struct wlsc_surface, link);
	switcher->listener.func = switcher_handle_surface_destroy;
	wl_list_init(&switcher->listener.link);

	return switcher;
}

static void
wlsc_switcher_destroy(struct wlsc_switcher *switcher)
{
	wl_list_remove(&switcher->listener.link);
	free(switcher);
}

static void
switcher_next_binding(struct wl_input_device *device, uint32_t time,
		      uint32_t key, uint32_t button,
		      uint32_t state, void *data)
{
	struct wlsc_compositor *compositor = data;

	if (!state)
		return;
	if (wl_list_empty(&compositor->surface_list))
		return;
	if (compositor->switcher == NULL)
		compositor->switcher = wlsc_switcher_create(compositor);

	wlsc_switcher_next(compositor->switcher);
}

static void
switcher_terminate_binding(struct wl_input_device *device,
			   uint32_t time, uint32_t key, uint32_t button,
			   uint32_t state, void *data)
{
	struct wlsc_compositor *compositor = data;
	struct wlsc_input_device *wd = (struct wlsc_input_device *) device;

	if (compositor->switcher && !state) {
		wlsc_surface_activate(compositor->switcher->current, wd, time);
		wlsc_switcher_destroy(compositor->switcher);
		compositor->switcher = NULL;
		compositor->overlay = NULL;
	}
}

void
wlsc_switcher_init(struct wlsc_compositor *compositor)
{
	wlsc_compositor_add_binding(compositor,
				    KEY_TAB, 0, MODIFIER_SUPER,
				    switcher_next_binding, compositor);
	wlsc_compositor_add_binding(compositor,
				    KEY_LEFTMETA, 0, MODIFIER_SUPER,
				    switcher_terminate_binding, compositor);
	wlsc_compositor_add_binding(compositor,
				    KEY_RIGHTMETA, 0, MODIFIER_SUPER,
				    switcher_terminate_binding, compositor);
}
