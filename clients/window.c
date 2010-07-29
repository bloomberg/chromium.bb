/*
 * Copyright © 2008 Kristian Høgsberg
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <cairo.h>
#include <glib.h>
#include <glib-object.h>

#define EGL_EGLEXT_PROTOTYPES 1
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cairo-gl.h>

#include <X11/extensions/XKBcommon.h>

#include <linux/input.h>
#include "wayland-util.h"
#include "wayland-client.h"
#include "wayland-glib.h"
#include "../cairo-util.h"

#include "window.h"

struct display {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_output *output;
	struct rectangle screen_allocation;
	EGLDisplay dpy;
	EGLContext ctx;
	cairo_device_t *device;
	int fd;
	GMainLoop *loop;
	GSource *source;
	struct wl_list window_list;
	struct wl_list input_list;
	char *device_name;
	cairo_surface_t *active_frame, *inactive_frame, *shadow;
	struct xkb_desc *xkb;
};

struct window {
	struct display *display;
	struct wl_surface *surface;
	const char *title;
	struct rectangle allocation, saved_allocation, surface_allocation;
	int redraw_scheduled;
	int minimum_width, minimum_height;
	int margin;
	int drag_x, drag_y;
	int state;
	int fullscreen;
	int decoration;
	struct input *grab_device;
	struct input *keyboard_device;
	uint32_t name;

	EGLImageKHR *image;
	cairo_surface_t *cairo_surface, *pending_surface;

	window_resize_handler_t resize_handler;
	window_redraw_handler_t redraw_handler;
	window_key_handler_t key_handler;
	window_keyboard_focus_handler_t keyboard_focus_handler;
	window_acknowledge_handler_t acknowledge_handler;
	window_frame_handler_t frame_handler;

	void *user_data;
	struct wl_list link;
};

struct input {
	struct display *display;
	struct wl_input_device *input_device;
	struct window *pointer_focus;
	struct window *keyboard_focus;
	uint32_t modifiers;
	int32_t x, y, sx, sy;
	struct wl_list link;
};

static void
rounded_rect(cairo_t *cr, int x0, int y0, int x1, int y1, int radius)
{
	cairo_move_to(cr, x0, y0 + radius);
	cairo_arc(cr, x0 + radius, y0 + radius, radius, M_PI, 3 * M_PI / 2);
	cairo_line_to(cr, x1 - radius, y0);
	cairo_arc(cr, x1 - radius, y0 + radius, radius, 3 * M_PI / 2, 2 * M_PI);
	cairo_line_to(cr, x1, y1 - radius);
	cairo_arc(cr, x1 - radius, y1 - radius, radius, 0, M_PI / 2);
	cairo_line_to(cr, x0 + radius, y1);
	cairo_arc(cr, x0 + radius, y1 - radius, radius, M_PI / 2, M_PI);
	cairo_close_path(cr);
}

static const cairo_user_data_key_t surface_data_key;
struct surface_data {
	EGLImageKHR image;
	GLuint texture;
	EGLDisplay dpy;
};

static void
surface_data_destroy(void *p)
{
	struct surface_data *data = p;

	glDeleteTextures(1, &data->texture);
	eglDestroyImageKHR(data->dpy, data->image);
}

cairo_surface_t *
display_create_surface(struct display *display,
		       struct rectangle *rectangle)
{
	struct surface_data *data;
	EGLDisplay dpy = display->dpy;
	cairo_surface_t *surface;

	EGLint image_attribs[] = {
		EGL_WIDTH,		0,
		EGL_HEIGHT,		0,
		EGL_IMAGE_FORMAT_MESA,	EGL_IMAGE_FORMAT_ARGB8888_MESA,
		EGL_IMAGE_USE_MESA,	EGL_IMAGE_USE_SCANOUT_MESA,
		EGL_NONE
	};

	data = malloc(sizeof *data);
	image_attribs[1] = rectangle->width;
	image_attribs[3] = rectangle->height;
	data->image = eglCreateDRMImageMESA(dpy, image_attribs);
	glGenTextures(1, &data->texture);
	data->dpy = dpy;
	glBindTexture(GL_TEXTURE_2D, data->texture);
	glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, data->image);

	surface = cairo_gl_surface_create_for_texture(display->device,
						      CAIRO_CONTENT_COLOR_ALPHA,
						      data->texture,
						      rectangle->width,
						      rectangle->height);
	
	cairo_surface_set_user_data (surface, &surface_data_key,
				     data, surface_data_destroy);
	
	return surface;
}

static void
window_attach_surface(struct window *window)
{
	struct wl_visual *visual;
	struct surface_data *data;
	EGLint name, stride;

	if (window->pending_surface != NULL)
		return;

	window->pending_surface = window->cairo_surface;
	window->cairo_surface = NULL;

	data = cairo_surface_get_user_data (window->pending_surface,
					    &surface_data_key);
	eglExportDRMImageMESA(window->display->dpy,
			      data->image, &name, NULL, &stride);

	visual = wl_display_get_premultiplied_argb_visual(window->display->display);
	wl_surface_attach(window->surface,
			  name,
			  window->surface_allocation.width,
			  window->surface_allocation.height,
			  stride,
			  visual);

	wl_surface_map(window->surface,
		       window->surface_allocation.x - window->margin,
		       window->surface_allocation.y - window->margin,
		       window->surface_allocation.width,
		       window->surface_allocation.height);

	wl_compositor_commit(window->display->compositor, 0);
}

void
window_commit(struct window *window, uint32_t key)
{
	if (window->cairo_surface) {
		window_attach_surface(window);
	} else {
		wl_compositor_commit(window->display->compositor, key);
	}
}

static void
window_draw_decorations(struct window *window)
{
	cairo_t *cr;
	cairo_text_extents_t extents;
	cairo_pattern_t *outline, *bright, *dim;
	int width, height;

	window->cairo_surface =
		display_create_surface(window->display, &window->allocation);
	window->surface_allocation = window->allocation;
	width = window->allocation.width;
	height = window->allocation.height;

	outline = cairo_pattern_create_rgb(0.1, 0.1, 0.1);
	bright = cairo_pattern_create_rgb(0.8, 0.8, 0.8);
	dim = cairo_pattern_create_rgb(0.4, 0.4, 0.4);

	cr = cairo_create(window->cairo_surface);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);

	cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
	tile_mask(cr, window->display->shadow, 3, 3, width, height, 32);

	if (window->keyboard_device)
		tile_source(cr, window->display->active_frame,
			    0, 0, width, height, 96);
	else
		tile_source(cr, window->display->inactive_frame,
			    0, 0, width, height, 96);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_font_size(cr, 14);
	cairo_text_extents(cr, window->title, &extents);
	cairo_move_to(cr, (width - extents.width) / 2, 32 - extents.y_bearing);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_line_width (cr, 4);
	if (window->keyboard_device)
		cairo_set_source_rgb(cr, 0, 0, 0);
	else
		cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
	cairo_show_text(cr, window->title);

	cairo_destroy(cr);
}

static void
window_draw_fullscreen(struct window *window)
{
	window->cairo_surface =
		display_create_surface(window->display, &window->allocation);
	window->surface_allocation = window->allocation;
}

void
window_draw(struct window *window)
{
	if (window->cairo_surface != NULL)
		cairo_surface_destroy(window->cairo_surface);

	if (window->fullscreen || !window->decoration)
		window_draw_fullscreen(window);
	else
		window_draw_decorations(window);
}

cairo_surface_t *
window_get_surface(struct window *window)
{
	return window->cairo_surface;
}

enum window_state {
	WINDOW_MOVING = 0,
	WINDOW_RESIZING_TOP = 1,
	WINDOW_RESIZING_BOTTOM = 2,
	WINDOW_RESIZING_LEFT = 4,
	WINDOW_RESIZING_TOP_LEFT = 5,
	WINDOW_RESIZING_BOTTOM_LEFT = 6,
	WINDOW_RESIZING_RIGHT = 8,
	WINDOW_RESIZING_TOP_RIGHT = 9,
	WINDOW_RESIZING_BOTTOM_RIGHT = 10,
	WINDOW_RESIZING_MASK = 15,
	WINDOW_STABLE = 16,
};

static void
window_handle_motion(void *data, struct wl_input_device *input_device,
		     uint32_t time,
		     int32_t x, int32_t y, int32_t sx, int32_t sy)
{
	struct input *input = data;
	struct window *window = input->pointer_focus;

	input->x = x;
	input->y = y;
	input->sx = sx;
	input->sy = sy;

	switch (window->state) {
	case WINDOW_MOVING:
		if (window->fullscreen)
			break;
		if (window->grab_device != input)
			break;
		window->allocation.x = window->drag_x + x;
		window->allocation.y = window->drag_y + y;
		wl_surface_map(window->surface,
			       window->allocation.x - window->margin,
			       window->allocation.y - window->margin,
			       window->allocation.width,
			       window->allocation.height);
		wl_compositor_commit(window->display->compositor, 1);
		break;

	case WINDOW_RESIZING_TOP:
	case WINDOW_RESIZING_BOTTOM:
	case WINDOW_RESIZING_LEFT:
	case WINDOW_RESIZING_RIGHT:
	case WINDOW_RESIZING_TOP_LEFT:
	case WINDOW_RESIZING_TOP_RIGHT:
	case WINDOW_RESIZING_BOTTOM_LEFT:
	case WINDOW_RESIZING_BOTTOM_RIGHT:
		if (window->fullscreen)
			break;
		if (window->grab_device != input)
			break;
		if (window->state & WINDOW_RESIZING_LEFT) {
			window->allocation.x = x - window->drag_x + window->saved_allocation.x;
			window->allocation.width = window->drag_x - x + window->saved_allocation.width;
		}
		if (window->state & WINDOW_RESIZING_RIGHT)
			window->allocation.width = x - window->drag_x + window->saved_allocation.width;
		if (window->state & WINDOW_RESIZING_TOP) {
			window->allocation.y = y - window->drag_y + window->saved_allocation.y;
			window->allocation.height = window->drag_y - y + window->saved_allocation.height;
		}
		if (window->state & WINDOW_RESIZING_BOTTOM)
			window->allocation.height = y - window->drag_y + window->saved_allocation.height;

		if (window->resize_handler)
			(*window->resize_handler)(window,
						  window->user_data);
		else if (window->redraw_handler)
			window_schedule_redraw(window);
		break;
	}
}

static void window_handle_button(void *data, struct wl_input_device *input_device,
				 uint32_t time, uint32_t button, uint32_t state)
{
	struct input *input = data;
	struct window *window = input->pointer_focus;
	int grip_size = 8, vlocation, hlocation;

	if (window->margin <= input->sx && input->sx < window->margin + grip_size)
		hlocation = WINDOW_RESIZING_LEFT;
	else if (input->sx < window->allocation.width - window->margin - grip_size)
		hlocation = WINDOW_MOVING;
	else if (input->sx < window->allocation.width - window->margin)
		hlocation = WINDOW_RESIZING_RIGHT;
	else
		hlocation = WINDOW_STABLE;

	if (window->margin <= input->sy && input->sy < window->margin + grip_size)
		vlocation = WINDOW_RESIZING_TOP;
	else if (input->sy < window->allocation.height - window->margin - grip_size)
		vlocation = WINDOW_MOVING;
	else if (input->sy < window->allocation.height - window->margin)
		vlocation = WINDOW_RESIZING_BOTTOM;
	else
		vlocation = WINDOW_STABLE;

	if (button == BTN_LEFT && state == 1) {
		switch (hlocation | vlocation) {
		case WINDOW_MOVING:
			window->drag_x = window->allocation.x - input->x;
			window->drag_y = window->allocation.y - input->y;
			window->state = WINDOW_MOVING;
			window->grab_device = input;
			break;
		case WINDOW_RESIZING_TOP:
		case WINDOW_RESIZING_BOTTOM:
		case WINDOW_RESIZING_LEFT:
		case WINDOW_RESIZING_RIGHT:
		case WINDOW_RESIZING_TOP_LEFT:
		case WINDOW_RESIZING_TOP_RIGHT:
		case WINDOW_RESIZING_BOTTOM_LEFT:
		case WINDOW_RESIZING_BOTTOM_RIGHT:
			window->drag_x = input->x;
			window->drag_y = input->y;
			window->saved_allocation = window->allocation;
			window->state = hlocation | vlocation;
			window->grab_device = input;
			break;
		default:
			window->state = WINDOW_STABLE;
			break;
		}
	} else if (button == BTN_LEFT &&
		   state == 0 && window->grab_device == input) {
		window->state = WINDOW_STABLE;
	}
}

static void
window_handle_key(void *data, struct wl_input_device *input_device,
		  uint32_t time, uint32_t key, uint32_t state)
{
	struct input *input = data;
	struct window *window = input->keyboard_focus;
	struct display *d = window->display;
	uint32_t code, sym, level;

	code = key + d->xkb->min_key_code;
	if (window->keyboard_device != input)
		return;

	level = 0;
	if (input->modifiers & WINDOW_MODIFIER_SHIFT &&
	    XkbKeyGroupWidth(d->xkb, code, 0) > 1)
		level = 1;

	sym = XkbKeySymEntry(d->xkb, code, level, 0);

	if (state)
		input->modifiers |= d->xkb->map->modmap[code];
	else
		input->modifiers &= ~d->xkb->map->modmap[code];

	if (window->key_handler)
		(*window->key_handler)(window, key, sym, state,
				       input->modifiers, window->user_data);
}

static void
window_handle_pointer_focus(void *data,
			    struct wl_input_device *input_device,
			    uint32_t time, struct wl_surface *surface)
{
	struct input *input = data;

	if (surface)
		input->pointer_focus = wl_surface_get_user_data(surface);
	else
		input->pointer_focus = NULL;
}

static void
window_handle_keyboard_focus(void *data,
			     struct wl_input_device *input_device,
			     uint32_t time,
			     struct wl_surface *surface,
			     struct wl_array *keys)
{
	struct input *input = data;
	struct window *window = input->keyboard_focus;
	struct display *d = input->display;
	uint32_t *k, *end;

	window = input->keyboard_focus;
	if (window) {
		window->keyboard_device = NULL;
		if (window->keyboard_focus_handler)
			(*window->keyboard_focus_handler)(window, NULL,
							  window->user_data);
	}

	if (surface)
		input->keyboard_focus = wl_surface_get_user_data(surface);
	else
		input->keyboard_focus = NULL;

	end = keys->data + keys->size;
	for (k = keys->data; k < end; k++)
		input->modifiers |= d->xkb->map->modmap[*k];

	window = input->keyboard_focus;
	if (window) {
		window->keyboard_device = input;
		if (window->keyboard_focus_handler)
			(*window->keyboard_focus_handler)(window,
							  window->keyboard_device,
							  window->user_data);
	}
}

static const struct wl_input_device_listener input_device_listener = {
	window_handle_motion,
	window_handle_button,
	window_handle_key,
	window_handle_pointer_focus,
	window_handle_keyboard_focus,
};

void
window_get_child_rectangle(struct window *window,
			   struct rectangle *rectangle)
{
	if (window->fullscreen && !window->decoration) {
		*rectangle = window->allocation;
	} else {
		rectangle->x = window->margin + 10;
		rectangle->y = window->margin + 50;
		rectangle->width = window->allocation.width - 20 - window->margin * 2;
		rectangle->height = window->allocation.height - 60 - window->margin * 2;
	}
}

void
window_set_child_size(struct window *window,
		      struct rectangle *rectangle)
{
	int32_t width, height;

	if (!window->fullscreen) {
		width = rectangle->width + 20 + window->margin * 2;
		height = rectangle->height + 60 + window->margin * 2;
		if (window->state & WINDOW_RESIZING_LEFT)
			window->allocation.x -=
				width - window->allocation.width;
		if (window->state & WINDOW_RESIZING_TOP)
			window->allocation.y -=
				height - window->allocation.height;
		window->allocation.width = width;
		window->allocation.height = height;
	}
}

void
window_copy_image(struct window *window,
		  struct rectangle *rectangle, EGLImageKHR image)
{
	/* set image as read buffer, copy pixels or something... */
}

void
window_copy_surface(struct window *window,
		    struct rectangle *rectangle,
		    cairo_surface_t *surface)
{
	cairo_t *cr;

	cr = cairo_create (window->cairo_surface);

	cairo_set_source_surface (cr,
				  surface,
				  rectangle->x, rectangle->y);

	cairo_paint (cr);
	cairo_destroy (cr);
}

static gboolean
idle_redraw(void *data)
{
	struct window *window = data;

	window->redraw_handler(window, window->user_data);
	window->redraw_scheduled = 0;

	return FALSE;
}

void
window_schedule_redraw(struct window *window)
{
	if (!window->redraw_scheduled) {
		g_idle_add(idle_redraw, window);
		window->redraw_scheduled = 1;
	}
}

void
window_set_fullscreen(struct window *window, int fullscreen)
{
	window->fullscreen = fullscreen;
	if (window->fullscreen) {
		window->saved_allocation = window->allocation;
		window->allocation = window->display->screen_allocation;
	} else {
		window->allocation = window->saved_allocation;
	}
}

void
window_set_decoration(struct window *window, int decoration)
{
	window->decoration = decoration;
}

void
window_set_user_data(struct window *window, void *data)
{
	window->user_data = data;
}

void
window_set_resize_handler(struct window *window,
			  window_resize_handler_t handler)
{
	window->resize_handler = handler;
}

void
window_set_redraw_handler(struct window *window,
			  window_redraw_handler_t handler)
{
	window->redraw_handler = handler;
}

void
window_set_key_handler(struct window *window,
		       window_key_handler_t handler)
{
	window->key_handler = handler;
}

void
window_set_acknowledge_handler(struct window *window,
			       window_acknowledge_handler_t handler)
{
	window->acknowledge_handler = handler;
}

void
window_set_frame_handler(struct window *window,
			 window_frame_handler_t handler)
{
	window->frame_handler = handler;
}

void
window_set_keyboard_focus_handler(struct window *window,
				  window_keyboard_focus_handler_t handler)
{
	window->keyboard_focus_handler = handler;
}

void
window_move(struct window *window, int32_t x, int32_t y)
{
	window->allocation.x = x;
	window->allocation.y = y;

	wl_surface_map(window->surface,
		       window->allocation.x - window->margin,
		       window->allocation.y - window->margin,
		       window->allocation.width,
		       window->allocation.height);
}

struct window *
window_create(struct display *display, const char *title,
	      int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct window *window;

	window = malloc(sizeof *window);
	if (window == NULL)
		return NULL;

	memset(window, 0, sizeof *window);
	window->display = display;
	window->title = strdup(title);
	window->surface = wl_compositor_create_surface(display->compositor);
	window->allocation.x = x;
	window->allocation.y = y;
	window->allocation.width = width;
	window->allocation.height = height;
	window->saved_allocation = window->allocation;
	window->margin = 16;
	window->state = WINDOW_STABLE;
	window->decoration = 1;

	wl_surface_set_user_data(window->surface, window);
	wl_list_insert(display->window_list.prev, &window->link);

	return window;
}

static void
display_handle_device(void *data,
		      struct wl_compositor *compositor,
		      const char *device)
{
	struct display *d = data;

	d->device_name = strdup(device);
}

static void
display_handle_acknowledge(void *data,
			   struct wl_compositor *compositor,
			   uint32_t key, uint32_t frame)
{
	struct display *d = data;
	struct window *window;
		
	/* The acknowledge event means that the server processed our
	 * last commit request and we can now safely free the old
	 * window buffer if we resized and render the next frame into
	 * our back buffer.. */
	wl_list_for_each(window, &d->window_list, link) {
		cairo_surface_destroy(window->pending_surface);
		window->pending_surface = NULL;
		if (window->cairo_surface)
			window_attach_surface(window);
		if (window->acknowledge_handler)
			(*window->acknowledge_handler)(window, key, frame, window->user_data);
	}
}

static void
display_handle_frame(void *data,
		     struct wl_compositor *compositor,
		     uint32_t frame, uint32_t timestamp)
{
	struct display *d = data;
	struct window *window;

	wl_list_for_each(window, &d->window_list, link) {
		if (window->frame_handler)
			(*window->frame_handler)(window, frame,
						 timestamp, window->user_data);
	}
}

static const struct wl_compositor_listener compositor_listener = {
	display_handle_device,
	display_handle_acknowledge,
	display_handle_frame,
};

static void
display_handle_geometry(void *data,
			struct wl_output *output,
			int32_t width, int32_t height)
{
	struct display *display = data;

	display->screen_allocation.x = 0;
	display->screen_allocation.y = 0;
	display->screen_allocation.width = width;
	display->screen_allocation.height = height;
}

static const struct wl_output_listener output_listener = {
	display_handle_geometry,
};

static void
display_add_input(struct display *d, struct wl_object *object)
{
	struct input *input;

	input = malloc(sizeof *input);
	if (input == NULL)
		return;

	memset(input, 0, sizeof *input);
	input->display = d;
	input->input_device = (struct wl_input_device *) object;
	input->pointer_focus = NULL;
	input->keyboard_focus = NULL;
	wl_list_insert(d->input_list.prev, &input->link);

	wl_input_device_add_listener(input->input_device,
				     &input_device_listener, input);
}

static void
display_handle_global(struct wl_display *display,
		     struct wl_object *object, void *data)
{
	struct display *d = data;

	if (wl_object_implements(object, "compositor", 1)) { 
		d->compositor = (struct wl_compositor *) object;
		wl_compositor_add_listener(d->compositor, &compositor_listener, d);
	} else if (wl_object_implements(object, "output", 1)) {
		d->output = (struct wl_output *) object;
		wl_output_add_listener(d->output, &output_listener, d);
	} else if (wl_object_implements(object, "input_device", 1)) {
		display_add_input(d, object);
	}
}

static const char socket_name[] = "\0wayland";

static void
display_render_frame(struct display *d)
{
	int radius = 8;
	cairo_t *cr;

	d->shadow = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 128, 128);
	cr = cairo_create(d->shadow);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	rounded_rect(cr, 16, 16, 112, 112, radius);
	cairo_fill(cr);
	cairo_destroy(cr);
	blur_surface(d->shadow, 64);

	d->active_frame =
		cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 128, 128);
	cr = cairo_create(d->active_frame);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba(cr, 0.8, 0.8, 0.4, 1);
	rounded_rect(cr, 16, 16, 112, 112, radius);
	cairo_fill(cr);
	cairo_destroy(cr);

	d->inactive_frame =
		cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 128, 128);
	cr = cairo_create(d->inactive_frame);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba(cr, 0.6, 0.6, 0.6, 1);
	rounded_rect(cr, 16, 16, 112, 112, radius);
	cairo_fill(cr);
	cairo_destroy(cr);
}

static void
init_xkb(struct display *d)
{
	struct xkb_rule_names names;

	names.rules = "evdev";
	names.model = "pc105";
	names.layout = "us";
	names.variant = "";
	names.options = "";

	d->xkb = xkb_compile_keymap_from_rules(&names);
	if (!d->xkb) {
		fprintf(stderr, "Failed to compile keymap\n");
		exit(1);
	}
}

struct display *
display_create(int *argc, char **argv[], const GOptionEntry *option_entries)
{
	struct display *d;
	EGLint major, minor;
	int fd;
	GOptionContext *context;
	GError *error;

	g_type_init();

	context = g_option_context_new(NULL);
	if (option_entries) {
		g_option_context_add_main_entries(context, option_entries, "Wayland View");
		if (!g_option_context_parse(context, argc, argv, &error)) {
			fprintf(stderr, "option parsing failed: %s\n", error->message);
			exit(EXIT_FAILURE);
		}
	}

	d = malloc(sizeof *d);
	if (d == NULL)
		return NULL;

	d->display = wl_display_create(socket_name, sizeof socket_name);
	if (d->display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return NULL;
	}

	wl_list_init(&d->input_list);

	/* Set up listener so we'll catch all events. */
	wl_display_add_global_listener(d->display,
				       display_handle_global, d);

	/* Process connection events. */
	wl_display_iterate(d->display, WL_DISPLAY_READABLE);

	fd = open(d->device_name, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "drm open failed: %m\n");
		return NULL;
	}

	d->dpy = eglGetDRMDisplayMESA(fd);
	if (!eglInitialize(d->dpy, &major, &minor)) {
		fprintf(stderr, "failed to initialize display\n");
		return NULL;
	}

	eglBindAPI(EGL_OPENGL_API);

	d->ctx = eglCreateContext(d->dpy, NULL, EGL_NO_CONTEXT, NULL);
	if (d->ctx == NULL) {
		fprintf(stderr, "failed to create context\n");
		return NULL;
	}

	if (!eglMakeCurrent(d->dpy, NULL, NULL, d->ctx)) {
		fprintf(stderr, "faile to make context current\n");
		return NULL;
	}

	d->device = cairo_egl_device_create(d->dpy, d->ctx);
	if (d->device == NULL) {
		fprintf(stderr, "failed to get cairo drm device\n");
		return NULL;
	}

	display_render_frame(d);

	d->loop = g_main_loop_new(NULL, FALSE);
	d->source = wl_glib_source_new(d->display);
	g_source_attach(d->source, NULL);

	wl_list_init(&d->window_list);

	init_xkb(d);

	return d;
}

struct wl_compositor *
display_get_compositor(struct display *display)
{
	return display->compositor;
}

EGLDisplay
display_get_egl_display(struct display *d)
{
	return d->dpy;
}

void
display_run(struct display *d)
{
	g_main_loop_run(d->loop);
}
