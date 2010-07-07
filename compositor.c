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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <math.h>
#include <time.h>
#include <linux/input.h>

#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "wayland.h"
#include "wayland-protocol.h"
#include "cairo-util.h"
#include "compositor.h"

const char *option_background = "background.jpg";
int option_connector = 0;

static const GOptionEntry option_entries[] = {
	{ "background", 'b', 0, G_OPTION_ARG_STRING,
	  &option_background, "Background image" },
	{ "connector", 'c', 0, G_OPTION_ARG_INT,
	  &option_connector, "KMS connector" },
	{ NULL }
};

				   
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
wlsc_matrix_rotate(struct wlsc_matrix *matrix,
		   GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	GLfloat c = cos(angle);
	GLfloat s = sin(angle);
	struct wlsc_matrix rotate = {
		{ x * x * (1 - c) + c,     y * x * (1 - c) + z * s, x * z * (1 - c) - y * s, 0,
		  x * y * (1 - c) - z * s, y * y * (1 - c) + c,     y * z * (1 - c) + x * s, 0, 
		  x * z * (1 - c) + y * s, y * z * (1 - c) - x * s, z * z * (1 - c) + c,     0,
		  0, 0, 0, 1 }
	};

	wlsc_matrix_multiply(matrix, &rotate);
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

static void
wlsc_surface_init(struct wlsc_surface *surface,
		  struct wlsc_compositor *compositor, struct wl_visual *visual,
		  int32_t x, int32_t y, int32_t width, int32_t height)
{
	glGenTextures(1, &surface->texture);
	surface->compositor = compositor;
	surface->visual = visual;
	wlsc_matrix_init(&surface->matrix);
	wlsc_matrix_scale(&surface->matrix, width, height, 1);
	wlsc_matrix_translate(&surface->matrix, x, y, 0);

	wlsc_matrix_init(&surface->matrix_inv);
	wlsc_matrix_translate(&surface->matrix_inv, -x, -y, 0);
	wlsc_matrix_scale(&surface->matrix_inv, 1.0 / width, 1.0 / height, 1);
}

static struct wlsc_surface *
wlsc_surface_create_from_cairo_surface(struct wlsc_compositor *ec,
				      cairo_surface_t *surface,
				      int x, int y, int width, int height)
{
	struct wlsc_surface *es;
	int stride;
	void *data;

	stride = cairo_image_surface_get_stride(surface);
	data = cairo_image_surface_get_data(surface);

	es = malloc(sizeof *es);
	if (es == NULL)
		return NULL;

	wlsc_surface_init(es, ec, &ec->premultiplied_argb_visual,
			  x, y, width, height);
	glBindTexture(GL_TEXTURE_2D, es->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
		     GL_RGBA, GL_UNSIGNED_BYTE, data);

	return es;
}

static void
wlsc_surface_destroy(struct wlsc_surface *surface,
		     struct wlsc_compositor *compositor)
{
	struct wlsc_listener *l;

	wl_list_remove(&surface->link);
	glDeleteTextures(1, &surface->texture);
	wl_client_remove_surface(surface->base.client, &surface->base);

	wl_list_for_each(l, &compositor->surface_destroy_listener_list, link)
		l->func(l, surface);

	free(surface);
}

static void
pointer_path(cairo_t *cr, int x, int y)
{
	const int end = 3, tx = 4, ty = 12, dx = 5, dy = 10;
	const int width = 16, height = 16;

	cairo_move_to(cr, x, y);
	cairo_line_to(cr, x + tx, y + ty);
	cairo_line_to(cr, x + dx, y + dy);
	cairo_line_to(cr, x + width - end, y + height);
	cairo_line_to(cr, x + width, y + height - end);
	cairo_line_to(cr, x + dy, y + dx);
	cairo_line_to(cr, x + ty, y + tx);
	cairo_close_path(cr);
}

static struct wlsc_surface *
pointer_create(struct wlsc_compositor *ec, int x, int y, int width, int height)
{
	struct wlsc_surface *es;
	const int hotspot_x = 16, hotspot_y = 16;
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
					     width, height);

	cr = cairo_create(surface);
	pointer_path(cr, hotspot_x + 5, hotspot_y + 4);
	cairo_set_line_width(cr, 2);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_stroke_preserve(cr);
	cairo_fill(cr);
	blur_surface(surface, width);

	pointer_path(cr, hotspot_x, hotspot_y);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill(cr);
	cairo_destroy(cr);

	es = wlsc_surface_create_from_cairo_surface(ec,
						   surface,
						   x - hotspot_x,
						   y - hotspot_y,
						   width, height);
	
	cairo_surface_destroy(surface);

	return es;
}

static struct wlsc_surface *
background_create(struct wlsc_output *output, const char *filename)
{
	struct wlsc_surface *background;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	void *data;
	GLenum format;

	background = malloc(sizeof *background);
	if (background == NULL)
		return NULL;
	
	g_type_init();

	pixbuf = gdk_pixbuf_new_from_file_at_scale(filename,
						   output->width,
						   output->height,
						   FALSE, &error);
	if (error != NULL) {
		free(background);
		return NULL;
	}

	data = gdk_pixbuf_get_pixels(pixbuf);

	wlsc_surface_init(background, output->compositor,
			  &output->compositor->rgb_visual,
			  output->x, output->y, output->width, output->height);

	glBindTexture(GL_TEXTURE_2D, background->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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
	glEnable(GL_TEXTURE_2D);
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
	wl_list_insert(compositor->surface_list.prev, &surface->link);
}

static void
wlsc_surface_lower(struct wlsc_surface *surface)
{
	struct wlsc_compositor *compositor = surface->compositor;

	wl_list_remove(&surface->link);
	wl_list_insert(&compositor->surface_list, &surface->link);
}

void
wlsc_compositor_finish_frame(struct wlsc_compositor *compositor, int msecs)
{
	wl_display_post_frame(compositor->wl_display,
			      &compositor->base,
			      compositor->current_frame, msecs);

	wl_event_source_timer_update(compositor->timer_source, 5);
	compositor->current_frame++;
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

	wl_list_for_each(es, &ec->surface_list, link)
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
	struct wlsc_surface *es = (struct wlsc_surface *) surface;
	struct wlsc_compositor *ec = es->compositor;

	wlsc_surface_destroy(es, ec);

	wlsc_compositor_schedule_repaint(ec);
}

static void
surface_attach(struct wl_client *client,
	       struct wl_surface *surface, uint32_t name, 
	       uint32_t width, uint32_t height, uint32_t stride,
	       struct wl_object *visual)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;
	struct wlsc_compositor *ec = es->compositor;
	EGLint attribs[] = {
		EGL_WIDTH,		0,
		EGL_HEIGHT,		0,
		EGL_IMAGE_STRIDE_MESA,	0,
		EGL_IMAGE_FORMAT_MESA,	EGL_IMAGE_FORMAT_ARGB8888_MESA,
		EGL_NONE
	};

	es->width = width;
	es->height = height;

	if (visual == &ec->argb_visual.base)
		es->visual = &ec->argb_visual;
	else if (visual == &ec->premultiplied_argb_visual.base)
		es->visual = &ec->premultiplied_argb_visual;
	else if (visual == &ec->rgb_visual.base)
		es->visual = &ec->rgb_visual;
	else
		/* FIXME: Smack client with an exception event */;

	glBindTexture(GL_TEXTURE_2D, es->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (es->image)
		eglDestroyImageKHR(ec->display, es->image);

	attribs[1] = width;
	attribs[3] = height;
	attribs[5] = stride / 4;

	es->image = eglCreateImageKHR(ec->display, ec->context,
				      EGL_DRM_IMAGE_MESA,
				      (EGLClientBuffer) name, attribs);
	glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, es->image);
	
}

static void
surface_map(struct wl_client *client,
	    struct wl_surface *surface,
	    int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wlsc_surface *es = (struct wlsc_surface *) surface;

	wlsc_matrix_init(&es->matrix);
	wlsc_matrix_scale(&es->matrix, width, height, 1);
	wlsc_matrix_translate(&es->matrix, x, y, 0);

	wlsc_matrix_init(&es->matrix_inv);
	wlsc_matrix_translate(&es->matrix_inv, -x, -y, 0);
	wlsc_matrix_scale(&es->matrix_inv, 1.0 / width, 1.0 / height, 1);

}

static void
surface_damage(struct wl_client *client,
	       struct wl_surface *surface,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	/* FIXME: This need to take a damage region, of course. */
}

const static struct wl_surface_interface surface_interface = {
	surface_destroy,
	surface_attach,
	surface_map,
	surface_damage
};

static void
compositor_create_surface(struct wl_client *client,
			  struct wl_compositor *compositor, uint32_t id)
{
	struct wlsc_compositor *ec = (struct wlsc_compositor *) compositor;
	struct wlsc_surface *surface;

	surface = malloc(sizeof *surface);
	if (surface == NULL)
		/* FIXME: Send OOM event. */
		return;

	wlsc_surface_init(surface, ec, NULL, 0, 0, 0, 0);

	wl_list_insert(ec->surface_list.prev, &surface->link);
	wl_client_add_surface(client, &surface->base,
			      &surface_interface, id);
}

static void
compositor_commit(struct wl_client *client,
		  struct wl_compositor *compositor, uint32_t key)
{
	struct wlsc_compositor *ec = (struct wlsc_compositor *) compositor;

	wlsc_compositor_schedule_repaint(ec);
	wl_client_send_acknowledge(client, compositor, key, ec->current_frame);
}

const static struct wl_compositor_interface compositor_interface = {
	compositor_create_surface,
	compositor_commit
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
				     struct wlsc_surface *surface)
{
	if (device->keyboard_focus == surface)
		return;

	if (device->keyboard_focus &&
	    (!surface || device->keyboard_focus->base.client != surface->base.client))
		wl_surface_post_event(&device->keyboard_focus->base,
				      &device->base,
				      WL_INPUT_KEYBOARD_FOCUS, NULL, &device->keys);

	if (surface)
		wl_surface_post_event(&surface->base,
				      &device->base,
				      WL_INPUT_KEYBOARD_FOCUS,
				      &surface->base, &device->keys);

	device->keyboard_focus = surface;
}

static void
wlsc_input_device_set_pointer_focus(struct wlsc_input_device *device,
				    struct wlsc_surface *surface)
{
	if (device->pointer_focus == surface)
		return;

	if (device->pointer_focus &&
	    (!surface || device->pointer_focus->base.client != surface->base.client))
		wl_surface_post_event(&device->pointer_focus->base,
				      &device->base,
				      WL_INPUT_POINTER_FOCUS, NULL);
	if (surface)
		wl_surface_post_event(&surface->base,
				      &device->base,
				      WL_INPUT_POINTER_FOCUS, &surface->base);

	device->pointer_focus = surface;
}

static struct wlsc_surface *
pick_surface(struct wlsc_input_device *device, int32_t *sx, int32_t *sy)
{
	struct wlsc_compositor *ec = device->ec;
	struct wlsc_surface *es;

	if (device->grab > 0) {
		wlsc_surface_transform(device->grab_surface,
				       device->x, device->y, sx, sy);
		return device->grab_surface;
	}

	wl_list_for_each(es, &ec->surface_list, link) {
		wlsc_surface_transform(es, device->x, device->y, sx, sy);
		if (0 <= *sx && *sx < es->width &&
		    0 <= *sy && *sy < es->height)
			return es;
	}

	return NULL;
}

void
notify_motion(struct wlsc_input_device *device, int x, int y)
{
	struct wlsc_surface *es;
	struct wlsc_compositor *ec = device->ec;
	struct wlsc_output *output;
	const int hotspot_x = 16, hotspot_y = 16;
	int32_t sx, sy;

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
	es = pick_surface(device, &sx, &sy);

	wlsc_input_device_set_pointer_focus(device, es);
		
	if (es)
		wl_surface_post_event(&es->base, &device->base,
				      WL_INPUT_MOTION, x, y, sx, sy);

	wlsc_matrix_init(&device->sprite->matrix);
	wlsc_matrix_scale(&device->sprite->matrix, 64, 64, 1);
	wlsc_matrix_translate(&device->sprite->matrix,
			      x - hotspot_x, y - hotspot_y, 0);

	wlsc_compositor_schedule_repaint(device->ec);
}

void
notify_button(struct wlsc_input_device *device,
	      int32_t button, int32_t state)
{
	struct wlsc_surface *surface;
	struct wlsc_compositor *compositor = device->ec;
	int32_t sx, sy;

	surface = pick_surface(device, &sx, &sy);
	if (surface) {
		if (state) {
			wlsc_surface_raise(surface);
			device->grab++;
			device->grab_surface = surface;
			wlsc_input_device_set_keyboard_focus(device,
							     surface);
		} else {
			device->grab--;
		}

		/* FIXME: Swallow click on raise? */
		wl_surface_post_event(&surface->base, &device->base,
				      WL_INPUT_BUTTON, button, state,
				      device->x, device->y, sx, sy);

		wlsc_compositor_schedule_repaint(compositor);
	}
}

void
notify_key(struct wlsc_input_device *device,
	   uint32_t key, uint32_t state)
{
	struct wlsc_compositor *compositor = device->ec;
	uint32_t *k, *end;
	uint32_t modifier;

	switch (key | compositor->modifier_state) {
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

	default:
		modifier = 0;
		break;
	}

	if (state)
		compositor->modifier_state |= modifier;
	else
		compositor->modifier_state &= ~modifier;

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
		wl_surface_post_event(&device->keyboard_focus->base,
				      &device->base, 
				      WL_INPUT_KEY, key, state);
}

static void
handle_surface_destroy(struct wlsc_listener *listener,
		       struct wlsc_surface *surface)
{
	struct wlsc_input_device *device =
		container_of(listener, struct wlsc_input_device, listener);
	struct wlsc_surface *focus;
	int32_t sx, sy;

	if (device->grab_surface == surface) {
		device->grab_surface = NULL;
		device->grab = 0;
	}
	if (device->keyboard_focus == surface)
		wlsc_input_device_set_keyboard_focus(device, NULL);
	if (device->pointer_focus == surface) {
		focus = pick_surface(device, &sx, &sy);
		wlsc_input_device_set_pointer_focus(device, focus);
		fprintf(stderr, "lost pointer focus surface, reverting to %p\n", focus);
	}
}

void
wlsc_input_device_init(struct wlsc_input_device *device,
		       struct wlsc_compositor *ec)
{
	device->base.interface = &wl_input_device_interface;
	device->base.implementation = NULL;
	wl_display_add_object(ec->wl_display, &device->base);
	wl_display_add_global(ec->wl_display, &device->base, NULL);
	device->x = 100;
	device->y = 100;
	device->ec = ec;
	device->sprite = pointer_create(ec, device->x, device->y, 64, 64);

	device->listener.func = handle_surface_destroy;
	wl_list_insert(ec->surface_destroy_listener_list.prev,
		       &device->listener.link);
	wl_list_insert(ec->input_device_list.prev, &device->link);
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

	wl_display_set_compositor(display, &ec->base, &compositor_interface); 

	add_visuals(ec);

	wl_list_init(&ec->surface_list);
	wl_list_init(&ec->input_device_list);
	wl_list_init(&ec->output_list);
	wl_list_init(&ec->surface_destroy_listener_list);

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

/* The plan here is to generate a random anonymous socket name and
 * advertise that through a service on the session dbus.
 */
static const char socket_name[] = "\0wayland";

int main(int argc, char *argv[])
{
	struct wl_display *display;
	struct wlsc_compositor *ec;
	GError *error = NULL;
	GOptionContext *context;

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, option_entries, "Wayland");
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		exit(EXIT_FAILURE);
	}

	display = wl_display_create();

	if (getenv("DISPLAY"))
		ec = x11_compositor_create(display);
	else
		ec = drm_compositor_create(display);

	if (ec == NULL) {
		fprintf(stderr, "failed to create compositor\n");
		exit(EXIT_FAILURE);
	}
		
	if (wl_display_add_socket(display, socket_name, sizeof socket_name)) {
		fprintf(stderr, "failed to add socket: %m\n");
		exit(EXIT_FAILURE);
	}

	wl_display_run(display);

	return 0;
}
