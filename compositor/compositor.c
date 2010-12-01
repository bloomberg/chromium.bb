/*
 * Copyright © 2008 Kristian Høgsberg
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <math.h>
#include <time.h>
#include <linux/input.h>

#include "wayland-server-protocol.h"
#include "compositor.h"

/* The plan here is to generate a random anonymous socket name and
 * advertise that through a service on the session dbus.
 */
static const char *option_socket_name = "wayland";

static const char *option_background = "background.jpg";
static const char *option_geometry = "1024x640";
static int option_connector = 0;

static const GOptionEntry option_entries[] = {
	{ "background", 'b', 0, G_OPTION_ARG_STRING,
	  &option_background, "Background image" },
	{ "connector", 'c', 0, G_OPTION_ARG_INT,
	  &option_connector, "KMS connector" },
	{ "geometry", 'g', 0, G_OPTION_ARG_STRING,
	  &option_geometry, "Geometry" },
	{ "socket", 's', 0, G_OPTION_ARG_STRING,
	  &option_socket_name, "Socket Name" },
	{ NULL }
};

static void
wlsc_input_device_set_pointer_focus(struct wlsc_input_device *device,
				    struct wlsc_surface *surface,
				    uint32_t time,
				    int32_t x, int32_t y,
				    int32_t sx, int32_t sy);

static void
wlsc_matrix_init(struct wlsc_matrix *matrix)
{
	static const struct wlsc_matrix identity = {
		{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 }
	};

	memcpy(matrix, &identity, sizeof identity);
}

static void
wlsc_matrix_multiply(struct wlsc_matrix *m, const struct wlsc_matrix *n)
{
	struct wlsc_matrix tmp;
	const GLfloat *row, *column;
	div_t d;
	int i, j;

	for (i = 0; i < 16; i++) {
		tmp.d[i] = 0;
		d = div(i, 4);
		row = m->d + d.quot * 4;
		column = n->d + d.rem;
		for (j = 0; j < 4; j++)
			tmp.d[i] += row[j] * column[j * 4];
	}
	memcpy(m, &tmp, sizeof tmp);
}

static void
wlsc_matrix_translate(struct wlsc_matrix *matrix, GLfloat x, GLfloat y, GLfloat z)
{
	struct wlsc_matrix translate = {	
		{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  x, y, z, 1 }
	};

	wlsc_matrix_multiply(matrix, &translate);
}

static void
wlsc_matrix_scale(struct wlsc_matrix *matrix, GLfloat x, GLfloat y, GLfloat z)
{
	struct wlsc_matrix scale = {
		{ x, 0, 0, 0,  0, y, 0, 0,  0, 0, z, 0,  0, 0, 0, 1 }
	};

	wlsc_matrix_multiply(matrix, &scale);
}

static void
wlsc_matrix_transform(struct wlsc_matrix *matrix, struct wlsc_vector *v)
{
	int i, j;
	struct wlsc_vector t;

	for (i = 0; i < 4; i++) {
		t.f[i] = 0;
		for (j = 0; j < 4; j++)
			t.f[i] += v->f[j] * matrix->d[i + j * 4];
	}
	
	*v = t;
}

static struct wlsc_surface *
wlsc_surface_create(struct wlsc_compositor *compositor,
		    struct wl_visual *visual,
		    int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wlsc_surface *surface;

	surface = malloc(sizeof *surface);
	if (surface == NULL)
		return NULL;

	glGenTextures(1, &surface->texture);
	glBindTexture(GL_TEXTURE_2D, surface->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	surface->compositor = compositor;
	surface->visual = visual;
	surface->x = x;
	surface->y = y;
	surface->width = width;
	surface->height = height;
	wlsc_matrix_init(&surface->matrix);
	wlsc_matrix_scale(&surface->matrix, width, height, 1);
	wlsc_matrix_translate(&surface->matrix, x, y, 0);

	wlsc_matrix_init(&surface->matrix_inv);
	wlsc_matrix_translate(&surface->matrix_inv, -x, -y, 0);
	wlsc_matrix_scale(&surface->matrix_inv, 1.0 / width, 1.0 / height, 1);

	return surface;
}

static void
destroy_surface(struct wl_resource *resource, struct wl_client *client)
{
	struct wlsc_surface *surface =
		container_of(resource, struct wlsc_surface, base.base);
	struct wlsc_compositor *compositor = surface->compositor;
	struct wlsc_listener *l;

	wl_list_remove(&surface->link);
	glDeleteTextures(1, &surface->texture);

	wl_list_for_each(l, &compositor->surface_destroy_listener_list, link)
		l->func(l, surface);

	free(surface);

	wlsc_compositor_schedule_repaint(compositor);
}

static int
texture_from_png(const char *filename, int width, int height)
{
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	void *data;
	GLenum format;

	pixbuf = gdk_pixbuf_new_from_file_at_scale(filename,
						   width, height,
						   FALSE, &error);
	if (error != NULL)
		return -1;

	data = gdk_pixbuf_get_pixels(pixbuf);

	if (gdk_pixbuf_get_has_alpha(pixbuf))
		format = GL_RGBA;
	else
		format = GL_RGB;

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
			format, GL_UNSIGNED_BYTE, data);

	gdk_pixbuf_unref(pixbuf);

	return 0;
}

static const struct {
	const char *filename;
	int hotspot_x, hotspot_y;
} pointer_images[] = {
	{ DATADIR "/wayland/bottom_left_corner.png",	 6, 30 },
	{ DATADIR "/wayland/bottom_right_corner.png",	28, 28 },
	{ DATADIR "/wayland/bottom_side.png",		16, 20 },
	{ DATADIR "/wayland/grabbing.png",		20, 17 },
	{ DATADIR "/wayland/left_ptr.png",		10,  5 },
	{ DATADIR "/wayland/left_side.png",		10, 20 },
	{ DATADIR "/wayland/right_side.png",		30, 19 },
	{ DATADIR "/wayland/top_left_corner.png",	 8,  8 },
	{ DATADIR "/wayland/top_right_corner.png",	26,  8 },
	{ DATADIR "/wayland/top_side.png",		18,  8 },
	{ DATADIR "/wayland/xterm.png",			15, 15 }
};

static void
create_pointer_images(struct wlsc_compositor *ec)
{
	int i, count;
	GLuint texture;
	const int width = 32, height = 32;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	count = ARRAY_LENGTH(pointer_images);
	ec->pointer_buffers = malloc(count * sizeof *ec->pointer_buffers);
	for (i = 0; i < count; i++) {
		ec->pointer_buffers[i] =
			wlsc_drm_buffer_create(ec, width, height,
					       &ec->argb_visual);
		glEGLImageTargetTexture2DOES(GL_TEXTURE_2D,
					     ec->pointer_buffers[i]->image);
		texture_from_png(pointer_images[i].filename, width, height);
	}
}

static struct wlsc_surface *
background_create(struct wlsc_output *output, const char *filename)
{
	struct wlsc_surface *background;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	void *data;
	GLenum format;

	background = wlsc_surface_create(output->compositor,
					 &output->compositor->rgb_visual,
					 output->x, output->y,
					 output->width, output->height);
	if (background == NULL)
		return NULL;

	pixbuf = gdk_pixbuf_new_from_file_at_scale(filename,
						   output->width,
						   output->height,
						   FALSE, &error);
	if (error != NULL) {
		free(background);
		return NULL;
	}

	data = gdk_pixbuf_get_pixels(pixbuf);

	if (gdk_pixbuf_get_has_alpha(pixbuf))
		format = GL_RGBA;
	else
		format = GL_RGB;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
		     output->width, output->height, 0,
		     format, GL_UNSIGNED_BYTE, data);

	return background;
}

static void
wlsc_surface_draw(struct wlsc_surface *es, struct wlsc_output *output)
{
	struct wlsc_compositor *ec = es->compositor;
	struct wlsc_matrix tmp;

	tmp = es->matrix;
	wlsc_matrix_multiply(&tmp, &output->matrix);
	glUniformMatrix4fv(ec->proj_uniform, 1, GL_FALSE, tmp.d);
	glUniform1i(ec->tex_uniform, 0);

	if (es->visual == &ec->argb_visual) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
	} else if (es->visual == &ec->premultiplied_argb_visual) {
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
	} else {
		glDisable(GL_BLEND);
	}

	glBindTexture(GL_TEXTURE_2D, es->texture);
	glBindBuffer(GL_ARRAY_BUFFER, ec->vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
			      5 * sizeof(GLfloat), NULL);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
			      5 * sizeof(GLfloat), (GLfloat *) 0 + 3);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void
wlsc_surface_raise(struct wlsc_surface *surface)
{
	struct wlsc_compositor *compositor = surface->compositor;

	wl_list_remove(&surface->link);
	wl_list_insert(&compositor->surface_list, &surface->link);
}

static void
wlsc_surface_update_matrix(struct wlsc_surface *es)
{
	wlsc_matrix_init(&es->matrix);
	wlsc_matrix_scale(&es->matrix, es->width, es->height, 1);
	wlsc_matrix_translate(&es->matrix, es->x, es->y, 0);

	wlsc_matrix_init(&es->matrix_inv);
	wlsc_matrix_translate(&es->matrix_inv, -es->x, -es->y, 0);
	wlsc_matrix_scale(&es->matrix_inv,
			  1.0 / es->width, 1.0 / es->height, 1);
}

void
wlsc_compositor_finish_frame(struct wlsc_compositor *compositor, int msecs)
{
	wl_display_post_frame(compositor->wl_display, msecs);
	wl_event_source_timer_update(compositor->timer_source, 5);
}

static void
wlsc_output_repaint(struct wlsc_output *output)
{
	struct wlsc_compositor *ec = output->compositor;
	struct wlsc_surface *es;
	struct wlsc_input_device *eid;

	glViewport(0, 0, output->width, output->height);

	if (output->background)
		wlsc_surface_draw(output->background, output);
	else
		glClear(GL_COLOR_BUFFER_BIT);

	wl_list_for_each_reverse(es, &ec->surface_list, link)
		wlsc_surface_draw(es, output);

	if (ec->focus)
		wl_list_for_each(eid, &ec->input_device_list, link)
			wlsc_surface_draw(eid->sprite, output);
}

static void
repaint(void *data)
{
	struct wlsc_compositor *ec = data;
	struct wlsc_output *output;

	if (!ec->repaint_needed) {
		ec->repaint_on_timeout = 0;
		return;
	}

	wl_list_for_each(output, &ec->output_list, link)
		wlsc_output_repaint(output);

	ec->present(ec);

	ec->repaint_needed = 0;
}

void
wlsc_compositor_schedule_repaint(struct wlsc_compositor *compositor)
{
	compositor->repaint_needed = 1;
	if (compositor->repaint_on_timeout)
		return;

	wl_event_source_timer_update(compositor->timer_source, 1);
	compositor->repaint_on_timeout = 1;
}

static void
surface_destroy(struct wl_client *client,
		struct wl_surface *surface)
{
	wl_resource_destroy(&surface->base, client);
}

static void
surface_attach(struct wl_client *client,
	       struct wl_surface *surface, struct wl_buffer *buffer)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;

	buffer->attach(buffer, surface);
	es->buffer = buffer;
}

static void
surface_map(struct wl_client *client,
	    struct wl_surface *surface,
	    int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;

	es->x = x;
	es->y = y;
	es->width = width;
	es->height = height;

	wlsc_surface_update_matrix(es);
	wlsc_compositor_schedule_repaint(es->compositor);
}

static void
surface_damage(struct wl_client *client,
	       struct wl_surface *surface,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;

	es->buffer->damage(es->buffer, surface, x, y, width, height);
	wlsc_compositor_schedule_repaint(es->compositor);
}

const static struct wl_surface_interface surface_interface = {
	surface_destroy,
	surface_attach,
	surface_map,
	surface_damage
};

static void
wlsc_input_device_attach(struct wlsc_input_device *device,
			 struct wl_buffer *buffer, int x, int y)
{
	struct wlsc_compositor *ec = device->ec;

	buffer->attach(buffer, &device->sprite->base);
	device->hotspot_x = x;
	device->hotspot_y = y;

	device->sprite->x = device->x - device->hotspot_x;
	device->sprite->y = device->y - device->hotspot_y;
	device->sprite->width = buffer->width;
	device->sprite->height = buffer->height;
	wlsc_surface_update_matrix(device->sprite);

	wlsc_compositor_schedule_repaint(ec);
}


static void
wlsc_input_device_set_pointer_image(struct wlsc_input_device *device,
				    enum wlsc_pointer_type type)
{
	struct wlsc_compositor *compositor = device->ec;

	wlsc_input_device_attach(device,
				 &compositor->pointer_buffers[type]->base,
				 pointer_images[type].hotspot_x,
				 pointer_images[type].hotspot_y);
}

static void
wlsc_input_device_start_grab(struct wlsc_input_device *device,
			     uint32_t time,
			     enum wlsc_grab_type grab)
{
	device->grab = grab;
	device->grab_surface = device->pointer_focus;
	device->grab_dx = device->pointer_focus->x - device->grab_x;
	device->grab_dy = device->pointer_focus->y - device->grab_y;
	device->grab_width = device->pointer_focus->width;
	device->grab_height = device->pointer_focus->height;

	wlsc_input_device_set_pointer_focus(device,
					    (struct wlsc_surface *) &wl_grab_surface,
					    time, 0, 0, 0, 0);
}

static void
shell_move(struct wl_client *client, struct wl_shell *shell,
	   struct wl_surface *surface,
	   struct wl_input_device *device, uint32_t time)
{
	struct wlsc_input_device *wd = (struct wlsc_input_device *) device;

	if (wd->grab != WLSC_DEVICE_GRAB_MOTION ||
	    wd->grab_time != time ||
	    &wd->pointer_focus->base != surface)
		return;

	wlsc_input_device_start_grab(wd, time, WLSC_DEVICE_GRAB_MOVE);
	wlsc_input_device_set_pointer_image(wd, WLSC_POINTER_DRAGGING);
}

static void
shell_resize(struct wl_client *client, struct wl_shell *shell,
	     struct wl_surface *surface,
	     struct wl_input_device *device, uint32_t time, uint32_t edges)
{
	struct wlsc_input_device *wd = (struct wlsc_input_device *) device;
	enum wlsc_pointer_type pointer = WLSC_POINTER_LEFT_PTR;

	if (edges == 0 || edges > 15 ||
	    (edges & 3) == 3 || (edges & 12) == 12)
		return;

	if (wd->grab != WLSC_DEVICE_GRAB_MOTION ||
	    wd->grab_time != time ||
	    &wd->pointer_focus->base != surface)
		return;

	switch (edges) {
	case WLSC_DEVICE_GRAB_RESIZE_TOP:
		pointer = WLSC_POINTER_TOP;
		break;
	case WLSC_DEVICE_GRAB_RESIZE_BOTTOM:
		pointer = WLSC_POINTER_BOTTOM;
		break;
	case WLSC_DEVICE_GRAB_RESIZE_LEFT:
		pointer = WLSC_POINTER_LEFT;
		break;
	case WLSC_DEVICE_GRAB_RESIZE_TOP_LEFT:
		pointer = WLSC_POINTER_TOP_LEFT;
		break;
	case WLSC_DEVICE_GRAB_RESIZE_BOTTOM_LEFT:
		pointer = WLSC_POINTER_BOTTOM_LEFT;
		break;
	case WLSC_DEVICE_GRAB_RESIZE_RIGHT:
		pointer = WLSC_POINTER_RIGHT;
		break;
	case WLSC_DEVICE_GRAB_RESIZE_TOP_RIGHT:
		pointer = WLSC_POINTER_TOP_RIGHT;
		break;
	case WLSC_DEVICE_GRAB_RESIZE_BOTTOM_RIGHT:
		pointer = WLSC_POINTER_BOTTOM_RIGHT;
		break;
	}

	wlsc_input_device_start_grab(wd, time, edges);
	wlsc_input_device_set_pointer_image(wd, pointer);
}

static uint32_t
get_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

struct wlsc_drag {
	struct wl_drag drag;
	struct wlsc_listener listener;
};

static void
wl_drag_set_pointer_focus(struct wl_drag *drag,
			  struct wlsc_surface *surface, uint32_t time,
			  int32_t x, int32_t y, int32_t sx, int32_t sy);

static void
destroy_drag(struct wl_resource *resource, struct wl_client *client)
{
	struct wlsc_drag *drag =
		container_of(resource, struct wlsc_drag, drag.resource);

	wl_list_remove(&drag->listener.link);

	free(drag);
}

const static struct wl_drag_interface drag_interface;

static void
drag_handle_surface_destroy(struct wlsc_listener *listener,
			    struct wlsc_surface *surface)
{
	struct wlsc_drag *drag =
		container_of(listener, struct wlsc_drag, listener);
	uint32_t time = get_time();

	if (drag->drag.pointer_focus == &surface->base)
		wl_drag_set_pointer_focus(&drag->drag, NULL, time, 0, 0, 0, 0);
}

static void
shell_create_drag(struct wl_client *client,
		  struct wl_shell *shell, uint32_t id)
{
	struct wlsc_drag *drag;
	struct wlsc_compositor *ec =
		container_of(shell, struct wlsc_compositor, shell);

	drag = malloc(sizeof *drag);
	if (drag == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	memset(drag, 0, sizeof *drag);
	drag->drag.resource.base.id = id;
	drag->drag.resource.base.interface = &wl_drag_interface;
	drag->drag.resource.base.implementation =
		(void (**)(void)) &drag_interface;

	drag->drag.resource.destroy = destroy_drag;

	drag->listener.func = drag_handle_surface_destroy;
	wl_list_insert(ec->surface_destroy_listener_list.prev,
		       &drag->listener.link);

	wl_client_add_resource(client, &drag->drag.resource);
}

const static struct wl_shell_interface shell_interface = {
	shell_move,
	shell_resize,
	shell_create_drag
};

static void
compositor_create_surface(struct wl_client *client,
			  struct wl_compositor *compositor, uint32_t id)
{
	struct wlsc_compositor *ec = (struct wlsc_compositor *) compositor;
	struct wlsc_surface *surface;

	surface = wlsc_surface_create(ec, NULL, 0, 0, 0, 0);
	if (surface == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_list_insert(ec->surface_list.prev, &surface->link);
	surface->base.base.destroy = destroy_surface;

	surface->base.base.base.id = id;
	surface->base.base.base.interface = &wl_surface_interface;
	surface->base.base.base.implementation =
		(void (**)(void)) &surface_interface;
	surface->base.client = client;

	wl_client_add_resource(client, &surface->base.base);
}

const static struct wl_compositor_interface compositor_interface = {
	compositor_create_surface,
};

static void
wlsc_surface_transform(struct wlsc_surface *surface,
		       int32_t x, int32_t y, int32_t *sx, int32_t *sy)
{
	struct wlsc_vector v = { { x, y, 0, 1 } };
	
	wlsc_matrix_transform(&surface->matrix_inv, &v);
	*sx = v.f[0] * surface->width;
	*sy = v.f[1] * surface->height;
}

static void
wlsc_input_device_set_keyboard_focus(struct wlsc_input_device *device,
				     struct wlsc_surface *surface,
				     uint32_t time)
{
	if (device->keyboard_focus == surface)
		return;

	if (device->keyboard_focus &&
	    (!surface || device->keyboard_focus->base.client != surface->base.client))
		wl_client_post_event(device->keyboard_focus->base.client,
				     &device->base.base,
				     WL_INPUT_DEVICE_KEYBOARD_FOCUS,
				     time, NULL, &device->keys);

	if (surface)
		wl_client_post_event(surface->base.client,
				     &device->base.base,
				     WL_INPUT_DEVICE_KEYBOARD_FOCUS,
				     time, &surface->base, &device->keys);

	device->keyboard_focus = surface;
}

static void
wlsc_input_device_set_pointer_focus(struct wlsc_input_device *device,
				    struct wlsc_surface *surface,
				    uint32_t time,
				    int32_t x, int32_t y,
				    int32_t sx, int32_t sy)
{
	if (device->pointer_focus == surface)
		return;

	if (device->pointer_focus &&
	    (!surface || device->pointer_focus->base.client != surface->base.client))
		wl_client_post_event(device->pointer_focus->base.client,
				     &device->base.base,
				     WL_INPUT_DEVICE_POINTER_FOCUS,
				     time, NULL, 0, 0, 0, 0);
	if (surface)
		wl_client_post_event(surface->base.client,
				     &device->base.base,
				     WL_INPUT_DEVICE_POINTER_FOCUS,
				     time, &surface->base,
				     x, y, sx, sy);

	if (!surface)
		wlsc_input_device_set_pointer_image(device,
						    WLSC_POINTER_LEFT_PTR);

	device->pointer_focus = surface;
	device->pointer_focus_time = time;
}

static struct wlsc_surface *
pick_surface(struct wlsc_input_device *device, int32_t *sx, int32_t *sy)
{
	struct wlsc_compositor *ec = device->ec;
	struct wlsc_surface *es;

	wl_list_for_each(es, &ec->surface_list, link) {
		wlsc_surface_transform(es, device->x, device->y, sx, sy);
		if (0 <= *sx && *sx < es->width &&
		    0 <= *sy && *sy < es->height)
			return es;
	}

	return NULL;
}

void
notify_motion(struct wlsc_input_device *device, uint32_t time, int x, int y)
{
	struct wlsc_surface *es;
	struct wlsc_compositor *ec = device->ec;
	struct wlsc_output *output;
	int32_t sx, sy, width, height;

	/* FIXME: We need some multi head love here. */
	output = container_of(ec->output_list.next, struct wlsc_output, link);
	if (x < output->x)
		x = 0;
	if (y < output->y)
		y = 0;
	if (x >= output->x + output->width)
		x = output->x + output->width - 1;
	if (y >= output->y + output->height)
		y = output->y + output->height - 1;

	device->x = x;
	device->y = y;

	switch (device->grab) {
	case WLSC_DEVICE_GRAB_NONE:
		es = pick_surface(device, &sx, &sy);
		wlsc_input_device_set_pointer_focus(device, es,
						    time, x, y, sx, sy);
		if (es)
			wl_client_post_event(es->base.client,
					     &device->base.base,
					     WL_INPUT_DEVICE_MOTION,
					     time, x, y, sx, sy);
		break;

	case WLSC_DEVICE_GRAB_MOTION:
		es = device->pointer_focus;
		wlsc_surface_transform(es, x, y, &sx, &sy);
		wl_client_post_event(es->base.client,
				     &device->base.base,
				     WL_INPUT_DEVICE_MOTION,
				     time, x, y, sx, sy);
		break;

	case WLSC_DEVICE_GRAB_MOVE:
		es = device->grab_surface;
		es->x = x + device->grab_dx;
		es->y = y + device->grab_dy;;
		wl_client_post_event(es->base.client,
				     &ec->shell.base,
				     WL_SHELL_CONFIGURE,
				     time, device->grab,
				     &es->base, es->x, es->y,
				     es->width, es->height);

		wlsc_surface_update_matrix(es);

		break;

	case WLSC_DEVICE_GRAB_RESIZE_TOP:
	case WLSC_DEVICE_GRAB_RESIZE_BOTTOM:
	case WLSC_DEVICE_GRAB_RESIZE_LEFT:
	case WLSC_DEVICE_GRAB_RESIZE_TOP_LEFT:
	case WLSC_DEVICE_GRAB_RESIZE_BOTTOM_LEFT:
	case WLSC_DEVICE_GRAB_RESIZE_RIGHT:
	case WLSC_DEVICE_GRAB_RESIZE_TOP_RIGHT:
	case WLSC_DEVICE_GRAB_RESIZE_BOTTOM_RIGHT:
	case WLSC_DEVICE_GRAB_RESIZE_MASK:
		es = device->grab_surface;

		if (device->grab & WLSC_DEVICE_GRAB_RESIZE_LEFT) {
			sx = x + device->grab_dx;
			width = device->grab_x - x + device->grab_width;
		} else if (device->grab & WLSC_DEVICE_GRAB_RESIZE_RIGHT) {
			sx = device->grab_x + device->grab_dx;
			width = x - device->grab_x + device->grab_width;
		} else {
			sx = device->grab_x + device->grab_dx;
			width = device->grab_width;
		}

		if (device->grab & WLSC_DEVICE_GRAB_RESIZE_TOP) {
			sy = y + device->grab_dy;
			height = device->grab_y - y + device->grab_height;
		} else if (device->grab & WLSC_DEVICE_GRAB_RESIZE_BOTTOM) {
			sy = device->grab_y + device->grab_dy;
			height = y - device->grab_y + device->grab_height;
		} else {
			sy = device->grab_y + device->grab_dy;
			height = device->grab_height;
		}

		wl_client_post_event(es->base.client,
				     &ec->shell.base,
				     WL_SHELL_CONFIGURE, time, device->grab,
				     &es->base, sx, sy, width, height);
		break;

	case WLSC_DEVICE_GRAB_DRAG:
		es = pick_surface(device, &sx, &sy);
		wl_drag_set_pointer_focus(device->drag,
					  es, time, x, y, sx, sy);
		if (es)
			wl_client_post_event(es->base.client,
					     &device->drag->drag_offer.base,
					     WL_DRAG_OFFER_MOTION,
					     time, x, y, sx, sy);
		break;

	}

	device->sprite->x = device->x - device->hotspot_x;
	device->sprite->y = device->y - device->hotspot_y;
	wlsc_surface_update_matrix(device->sprite);

	wlsc_compositor_schedule_repaint(device->ec);
}

static void
wlsc_input_device_end_grab(struct wlsc_input_device *device, uint32_t time)
{
	struct wl_drag *drag = device->drag;
	struct wlsc_surface *es;
	int32_t sx, sy;

	switch (device->grab) {
	case WLSC_DEVICE_GRAB_DRAG:
		if (drag->target)
			wl_client_post_event(drag->target,
					     &drag->drag_offer.base,
					     WL_DRAG_OFFER_DROP);
		wl_drag_set_pointer_focus(drag, NULL, time, 0, 0, 0, 0);
		device->drag = NULL;
		break;
	default:
		break;
	}

	device->grab = WLSC_DEVICE_GRAB_NONE;
	es = pick_surface(device, &sx, &sy);
	wlsc_input_device_set_pointer_focus(device, es, time,
					    device->x, device->y, sx, sy);
}

void
notify_button(struct wlsc_input_device *device,
	      uint32_t time, int32_t button, int32_t state)
{
	struct wlsc_surface *surface;
	struct wlsc_compositor *compositor = device->ec;

	surface = device->pointer_focus;
	if (surface) {
		if (state && device->grab == WLSC_DEVICE_GRAB_NONE) {
			wlsc_surface_raise(surface);
			device->grab = WLSC_DEVICE_GRAB_MOTION;
			device->grab_button = button;
			device->grab_time = time;
			device->grab_x = device->x;
			device->grab_y = device->y;
			wlsc_input_device_set_keyboard_focus(device,
							     surface, time);
		}

		if (state && button == BTN_LEFT &&
		    device->grab == WLSC_DEVICE_GRAB_MOTION &&
		    (device->modifier_state & MODIFIER_SUPER))
			shell_move(NULL, (struct wl_shell *) &compositor->shell,
				   &surface->base, &device->base, time);
		else if (state && button == BTN_MIDDLE &&
			   device->grab == WLSC_DEVICE_GRAB_MOTION &&
			   (device->modifier_state & MODIFIER_SUPER))
			shell_resize(NULL, (struct wl_shell *) &compositor->shell,

				     &surface->base, &device->base, time,
				     WLSC_DEVICE_GRAB_RESIZE_BOTTOM_RIGHT);
		else if (device->grab == WLSC_DEVICE_GRAB_NONE ||
			 device->grab == WLSC_DEVICE_GRAB_MOTION)
			wl_client_post_event(surface->base.client,
					     &device->base.base,
					     WL_INPUT_DEVICE_BUTTON,
					     time, button, state);

		if (!state &&
		    device->grab != WLSC_DEVICE_GRAB_NONE &&
		    device->grab_button == button) {
			wlsc_input_device_end_grab(device, time);
		}

		wlsc_compositor_schedule_repaint(compositor);
	}
}

void
notify_key(struct wlsc_input_device *device,
	   uint32_t time, uint32_t key, uint32_t state)
{
	uint32_t *k, *end;
	uint32_t modifier;

	switch (key | device->modifier_state) {
	case KEY_BACKSPACE | MODIFIER_CTRL | MODIFIER_ALT:
		kill(0, SIGTERM);
		return;
	}

	switch (key) {
	case KEY_LEFTCTRL:
	case KEY_RIGHTCTRL:
		modifier = MODIFIER_CTRL;
		break;

	case KEY_LEFTALT:
	case KEY_RIGHTALT:
		modifier = MODIFIER_ALT;
		break;

	case KEY_LEFTMETA:
	case KEY_RIGHTMETA:
		modifier = MODIFIER_SUPER;
		break;

	default:
		modifier = 0;
		break;
	}

	if (state)
		device->modifier_state |= modifier;
	else
		device->modifier_state &= ~modifier;

	end = device->keys.data + device->keys.size;
	for (k = device->keys.data; k < end; k++) {
		if (*k == key)
			*k = *--end;
	}
	device->keys.size = (void *) end - device->keys.data;
	if (state) {
		k = wl_array_add(&device->keys, sizeof *k);
		*k = key;
	}

	if (device->keyboard_focus != NULL)
		wl_client_post_event(device->keyboard_focus->base.client,
				     &device->base.base,
				     WL_INPUT_DEVICE_KEY, time, key, state);
}

static void
input_device_attach(struct wl_client *client,
		    struct wl_input_device *device_base,
		    uint32_t time,
		    struct wl_buffer *buffer, int32_t x, int32_t y)
{
	struct wlsc_input_device *device =
		(struct wlsc_input_device *) device_base;

	if (time < device->pointer_focus_time)
		return;
	if (device->pointer_focus == NULL)
		return;

	if (device->pointer_focus->base.client != client &&
	    !(&device->pointer_focus->base == &wl_grab_surface &&
	      device->grab_surface->base.client == client))
		return;

	if (buffer == NULL) {
		wlsc_input_device_set_pointer_image(device,
						    WLSC_POINTER_LEFT_PTR);
		return;
	}

	wlsc_input_device_attach(device, buffer, x, y);
}

const static struct wl_input_device_interface input_device_interface = {
	input_device_attach,
};

static void
handle_surface_destroy(struct wlsc_listener *listener,
		       struct wlsc_surface *surface)
{
	struct wlsc_input_device *device =
		container_of(listener, struct wlsc_input_device, listener);
	uint32_t time = get_time();

	if (device->keyboard_focus == surface)
		wlsc_input_device_set_keyboard_focus(device, NULL, time);
	if (device->pointer_focus == surface)
		wlsc_input_device_set_pointer_focus(device, NULL, time,
						    0, 0, 0, 0);
	if (device->pointer_focus == surface ||
	    (&device->pointer_focus->base == &wl_grab_surface &&
	     device->grab_surface == surface))
		wlsc_input_device_end_grab(device, time);
}

static void
wl_drag_set_pointer_focus(struct wl_drag *drag,
			  struct wlsc_surface *surface, uint32_t time,
			  int32_t x, int32_t y, int32_t sx, int32_t sy)
{
	char **p, **end;

	if (drag->pointer_focus == &surface->base)
		return;

	if (drag->pointer_focus &&
	    (!surface || drag->pointer_focus->client != surface->base.client))
		wl_client_post_event(drag->pointer_focus->client,
				      &drag->drag_offer.base,
				      WL_DRAG_OFFER_POINTER_FOCUS,
				      time, NULL, 0, 0, 0, 0);

	if (surface &&
	    (!drag->pointer_focus ||
	     drag->pointer_focus->client != surface->base.client)) {
		wl_client_post_global(surface->base.client,
				      &drag->drag_offer.base);

		end = drag->types.data + drag->types.size;
		for (p = drag->types.data; p < end; p++)
			wl_client_post_event(surface->base.client,
					      &drag->drag_offer.base,
					      WL_DRAG_OFFER_OFFER, *p);
	}

	if (surface) {
		wl_client_post_event(surface->base.client,
				     &drag->drag_offer.base,
				     WL_DRAG_OFFER_POINTER_FOCUS,
				     time, &surface->base,
				     x, y, sx, sy);

	}

	drag->pointer_focus = &surface->base;
	drag->pointer_focus_time = time;
	drag->target = NULL;
}

static void
drag_offer_accept(struct wl_client *client,
		  struct wl_drag_offer *offer, uint32_t time, const char *type)
{
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

	wl_client_post_event(drag->source->client, &drag->resource.base,
			     WL_DRAG_TARGET, drag->type);
}

static void
drag_offer_receive(struct wl_client *client,
		   struct wl_drag_offer *offer, int fd)
{
	struct wl_drag *drag = container_of(offer, struct wl_drag, drag_offer);

	wl_client_post_event(drag->source->client, &drag->resource.base,
			     WL_DRAG_FINISH, fd);
	close(fd);
}

static void
drag_offer_reject(struct wl_client *client, struct wl_drag_offer *offer)
{
	struct wl_drag *drag = container_of(offer, struct wl_drag, drag_offer);

	wl_client_post_event(drag->source->client, &drag->resource.base,
			     WL_DRAG_REJECT);
}

static const struct wl_drag_offer_interface drag_offer_interface = {
	drag_offer_accept,
	drag_offer_receive,
	drag_offer_reject
};

static void
drag_offer(struct wl_client *client, struct wl_drag *drag, const char *type)
{
	char **p;

	p = wl_array_add(&drag->types, sizeof *p);
	if (p)
		*p = strdup(type);
	if (!p || !*p)
		wl_client_post_no_memory(client);
}

static void
drag_activate(struct wl_client *client,
	      struct wl_drag *drag,
	      struct wl_surface *surface,
	      struct wl_input_device *input_device, uint32_t time)
{
	struct wl_display *display = wl_client_get_display (client);
	struct wlsc_input_device *device =
		(struct wlsc_input_device *) input_device;
	struct wlsc_surface *target;
	int32_t sx, sy;

	if (device->grab != WLSC_DEVICE_GRAB_MOTION ||
	    &device->pointer_focus->base != surface ||
	    device->grab_time != time)
		return;

	drag->source = surface;
	drag->input_device = input_device;

	drag->drag_offer.base.interface = &wl_drag_offer_interface;
	drag->drag_offer.base.implementation =
		(void (**)(void)) &drag_offer_interface;

	wl_display_add_object(display, &drag->drag_offer.base);

	wlsc_input_device_start_grab(device, time,
				     WLSC_DEVICE_GRAB_DRAG);

	device->drag = drag;
	target = pick_surface(device, &sx, &sy);
	wl_drag_set_pointer_focus(device->drag, target, time,
				  device->x, device->y, sx, sy);
}

static void
drag_cancel(struct wl_client *client, struct wl_drag *drag)
{
	struct wlsc_input_device *device =
		(struct wlsc_input_device *) drag->input_device;

	if (drag->source == NULL || drag->source->client != client)
		return;

	wlsc_input_device_end_grab(device, get_time());
	device->drag = NULL;
}

static const struct wl_drag_interface drag_interface = {
	drag_offer,
	drag_activate,
	drag_cancel,
};

void
wlsc_input_device_init(struct wlsc_input_device *device,
		       struct wlsc_compositor *ec)
{
	device->base.base.interface = &wl_input_device_interface;
	device->base.base.implementation =
		(void (**)(void)) &input_device_interface;
	wl_display_add_object(ec->wl_display, &device->base.base);
	wl_display_add_global(ec->wl_display, &device->base.base, NULL);

	device->x = 100;
	device->y = 100;
	device->ec = ec;
	device->sprite = wlsc_surface_create(ec, &ec->argb_visual,
					     device->x, device->y, 32, 32);
	device->hotspot_x = 16;
	device->hotspot_y = 16;

	device->listener.func = handle_surface_destroy;
	wl_list_insert(ec->surface_destroy_listener_list.prev,
		       &device->listener.link);
	wl_list_insert(ec->input_device_list.prev, &device->link);

	wlsc_input_device_set_pointer_image(device, WLSC_POINTER_LEFT_PTR);
}

static void
wlsc_output_post_geometry(struct wl_client *client, struct wl_object *global)
{
	struct wlsc_output *output =
		container_of(global, struct wlsc_output, base);

	wl_client_post_event(client, global,
			     WL_OUTPUT_GEOMETRY,
			     output->width, output->height);
}

static const char vertex_shader[] =
	"uniform mat4 proj;\n"
	"attribute vec4 position;\n"
	"attribute vec2 texcoord;\n"
	"varying vec2 v_texcoord;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = proj * position;\n"
	"   v_texcoord = texcoord;\n"
	"}\n";

static const char fragment_shader[] =
	/* "precision mediump float;\n" */
	"varying vec2 v_texcoord;\n"
	"uniform sampler2D tex;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor = texture2D(tex, v_texcoord)\n;"
	"}\n";

static int
init_shaders(struct wlsc_compositor *ec)
{
	GLuint v, f, program;
	const char *p;
	char msg[512];
	GLfloat vertices[4 * 5];
	GLint status;

	p = vertex_shader;
	v = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(v, 1, &p, NULL);
	glCompileShader(v);
	glGetShaderiv(v, GL_COMPILE_STATUS, &status);
	if (!status) {
		glGetShaderInfoLog(v, sizeof msg, NULL, msg);
		fprintf(stderr, "vertex shader info: %s\n", msg);
		return -1;
	}

	p = fragment_shader;
	f = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(f, 1, &p, NULL);
	glCompileShader(f);
	glGetShaderiv(f, GL_COMPILE_STATUS, &status);
	if (!status) {
		glGetShaderInfoLog(f, sizeof msg, NULL, msg);
		fprintf(stderr, "fragment shader info: %s\n", msg);
		return -1;
	}

	program = glCreateProgram();
	glAttachShader(program, v);
	glAttachShader(program, f);
	glBindAttribLocation(program, 0, "position");
	glBindAttribLocation(program, 1, "texcoord");

	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (!status) {
		glGetProgramInfoLog(program, sizeof msg, NULL, msg);
		fprintf(stderr, "link info: %s\n", msg);
		return -1;
	}

	glUseProgram(program);
	ec->proj_uniform = glGetUniformLocation(program, "proj");
	ec->tex_uniform = glGetUniformLocation(program, "tex");

	vertices[ 0] = 0.0;
	vertices[ 1] = 0.0;
	vertices[ 2] = 0.0;
	vertices[ 3] = 0.0;
	vertices[ 4] = 0.0;

	vertices[ 5] = 0.0;
	vertices[ 6] = 1.0;
	vertices[ 7] = 0.0;
	vertices[ 8] = 0.0;
	vertices[ 9] = 1.0;

	vertices[10] = 1.0;
	vertices[11] = 0.0;
	vertices[12] = 0.0;
	vertices[13] = 1.0;
	vertices[14] = 0.0;

	vertices[15] = 1.0;
	vertices[16] = 1.0;
	vertices[17] = 0.0;
	vertices[18] = 1.0;
	vertices[19] = 1.0;

	glGenBuffers(1, &ec->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, ec->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);

	return 0;
}

static const struct wl_interface visual_interface = {
	"visual", 1,
};

static void
add_visuals(struct wlsc_compositor *ec)
{
	ec->argb_visual.base.interface = &visual_interface;
	ec->argb_visual.base.implementation = NULL;
	wl_display_add_object(ec->wl_display, &ec->argb_visual.base);
	wl_display_add_global(ec->wl_display, &ec->argb_visual.base, NULL);

	ec->premultiplied_argb_visual.base.interface = &visual_interface;
	ec->premultiplied_argb_visual.base.implementation = NULL;
	wl_display_add_object(ec->wl_display,
			      &ec->premultiplied_argb_visual.base);
	wl_display_add_global(ec->wl_display,
			      &ec->premultiplied_argb_visual.base, NULL);

	ec->rgb_visual.base.interface = &visual_interface;
	ec->rgb_visual.base.implementation = NULL;
	wl_display_add_object(ec->wl_display, &ec->rgb_visual.base);
	wl_display_add_global(ec->wl_display, &ec->rgb_visual.base, NULL);
}

void
wlsc_output_init(struct wlsc_output *output, struct wlsc_compositor *c,
		 int x, int y, int width, int height)
{
	output->compositor = c;
	output->x = x;
	output->y = y;
	output->width = width;
	output->height = height;

	output->background =
		background_create(output, option_background);

	wlsc_matrix_init(&output->matrix);
	wlsc_matrix_translate(&output->matrix,
			      -output->x - output->width / 2.0,
			      -output->y - output->height / 2.0, 0);
	wlsc_matrix_scale(&output->matrix,
			  2.0 / output->width, 2.0 / output->height, 1);

	output->base.interface = &wl_output_interface;
	wl_display_add_object(c->wl_display, &output->base);
	wl_display_add_global(c->wl_display, &output->base,
			      wlsc_output_post_geometry);
}

int
wlsc_compositor_init(struct wlsc_compositor *ec, struct wl_display *display)
{
	struct wl_event_loop *loop;

	ec->wl_display = display;

	ec->base.base.interface = &wl_compositor_interface;
	ec->base.base.implementation =
		(void (**)(void)) &compositor_interface;

	wl_display_add_object(display, &ec->base.base);
	if (wl_display_add_global(display, &ec->base.base, NULL))
		return -1;

	wlsc_shm_init(ec);

	ec->shell.base.interface = &wl_shell_interface;
	ec->shell.base.implementation = (void (**)(void)) &shell_interface;
	wl_display_add_object(display, &ec->shell.base);
	if (wl_display_add_global(display, &ec->shell.base, NULL))
		return -1;

	add_visuals(ec);

	wl_list_init(&ec->surface_list);
	wl_list_init(&ec->input_device_list);
	wl_list_init(&ec->output_list);
	wl_list_init(&ec->surface_destroy_listener_list);

	create_pointer_images(ec);

	screenshooter_create(ec);

	glGenFramebuffers(1, &ec->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, ec->fbo);
	glActiveTexture(GL_TEXTURE0);
	if (init_shaders(ec) < 0)
		return -1;

	loop = wl_display_get_event_loop(ec->wl_display);
	ec->timer_source = wl_event_loop_add_timer(loop, repaint, ec);
	wlsc_compositor_schedule_repaint(ec);

	return 0;
}


int main(int argc, char *argv[])
{
	struct wl_display *display;
	struct wlsc_compositor *ec;
	GError *error = NULL;
	GOptionContext *context;
	int width, height;
	char *socket_name;
	int socket_name_size;

	g_type_init(); /* GdkPixbuf needs this, it seems. */

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, option_entries, "Wayland");
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		exit(EXIT_FAILURE);
	}
	if (sscanf(option_geometry, "%dx%d", &width, &height) != 2) {
		fprintf(stderr, "invalid geometry option: %s \n",
			option_geometry);
		exit(EXIT_FAILURE);
	}

	display = wl_display_create();

	if (getenv("WAYLAND_DISPLAY"))
		ec = wayland_compositor_create(display, width, height);
	else if (getenv("DISPLAY"))
		ec = x11_compositor_create(display, width, height);
	else
		ec = drm_compositor_create(display, option_connector);

	if (ec == NULL) {
		fprintf(stderr, "failed to create compositor\n");
		exit(EXIT_FAILURE);
	}

	socket_name_size = 1 + asprintf(&socket_name, "%c%s", '\0',
					option_socket_name);

	if (wl_display_add_socket(display, socket_name, socket_name_size)) {
		fprintf(stderr, "failed to add socket: %m\n");
		exit(EXIT_FAILURE);
	}
	free(socket_name);

	wl_display_run(display);

	return 0;
}
