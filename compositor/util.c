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

#include <stdlib.h>
#include <stdio.h>

#include "compositor.h"

struct wlsc_zoom {
	struct wlsc_surface *surface;
	struct wlsc_animation animation;
	struct wlsc_spring spring;
	struct wlsc_transform transform;
	struct wl_listener listener;
	void (*done)(struct wlsc_zoom *zoom, void *data);
	void *data;
};

static void
wlsc_zoom_destroy(struct wlsc_zoom *zoom)
{
	wl_list_remove(&zoom->animation.link);
	wl_list_remove(&zoom->listener.link);
	zoom->surface->transform = NULL;
	if (zoom->done)
		zoom->done(zoom, zoom->data);
	free(zoom);
}

static void
handle_zoom_surface_destroy(struct wl_listener *listener,
			    struct wl_resource *resource, uint32_t time)
{
	struct wlsc_zoom *zoom =
		container_of(listener, struct wlsc_zoom, listener);

	wlsc_zoom_destroy(zoom);
}

static void
wlsc_zoom_frame(struct wlsc_animation *animation,
		struct wlsc_output *output, uint32_t msecs)
{
	struct wlsc_zoom *zoom =
		container_of(animation, struct wlsc_zoom, animation);
	struct wlsc_surface *es = zoom->surface;
	GLfloat scale;

	wlsc_spring_update(&zoom->spring, msecs);

	if (wlsc_spring_done(&zoom->spring))
		wlsc_zoom_destroy(zoom);

	scale = zoom->spring.current;
	wlsc_matrix_init(&zoom->transform.matrix);
	wlsc_matrix_translate(&zoom->transform.matrix,
			      -(es->x + es->width / 2.0),
			      -(es->y + es->height / 2.0), 0);
	wlsc_matrix_scale(&zoom->transform.matrix, scale, scale, scale);
	wlsc_matrix_translate(&zoom->transform.matrix,
			      es->x + es->width / 2.0,
			      es->y + es->height / 2.0, 0);

	scale = 1.0 / zoom->spring.current;
	wlsc_matrix_init(&zoom->transform.inverse);
	wlsc_matrix_scale(&zoom->transform.inverse, scale, scale, scale);

	wlsc_compositor_damage_all(es->compositor);
}

WL_EXPORT struct wlsc_zoom *
wlsc_zoom_run(struct wlsc_surface *surface, GLfloat start, GLfloat stop,
	      wlsc_zoom_done_func_t done, void *data)
{
	struct wlsc_zoom *zoom;

	zoom = malloc(sizeof *zoom);
	if (!zoom)
		return NULL;

	zoom->surface = surface;
	zoom->done = done;
	zoom->data = data;
	surface->transform = &zoom->transform;
	wlsc_spring_init(&zoom->spring, 200.0, start, stop);
	zoom->spring.timestamp = wlsc_compositor_get_time();
	zoom->animation.frame = wlsc_zoom_frame;
	wlsc_zoom_frame(&zoom->animation, NULL, zoom->spring.timestamp);

	zoom->listener.func = handle_zoom_surface_destroy;
	wl_list_insert(surface->surface.resource.destroy_listener_list.prev,
		       &zoom->listener.link);

	wl_list_insert(surface->compositor->animation_list.prev,
		       &zoom->animation.link);

	return zoom;
}
