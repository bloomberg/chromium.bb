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

#include "../config.h"

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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <xf86drm.h>
#include <sys/mman.h>

#define EGL_EGLEXT_PROTOTYPES 1
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifdef HAVE_CAIRO_GL
#include <cairo-gl.h>
#endif

#include <X11/extensions/XKBcommon.h>

#include <linux/input.h>
#include "wayland-util.h"
#include "wayland-client.h"
#include "wayland-glib.h"
#include "cairo-util.h"

#include "window.h"

struct display {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_shell *shell;
	struct wl_drm *drm;
	struct wl_shm *shm;
	struct wl_output *output;
	struct rectangle screen_allocation;
	int authenticated;
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
	cairo_surface_t **pointer_surfaces;

	display_drag_offer_handler_t drag_offer_handler;
};

struct window {
	struct display *display;
	struct wl_surface *surface;
	const char *title;
	struct rectangle allocation, saved_allocation, pending_allocation;
	int resize_edges;
	int redraw_scheduled;
	int minimum_width, minimum_height;
	int margin;
	int fullscreen;
	int decoration;
	struct input *grab_device;
	struct input *keyboard_device;
	uint32_t name;
	enum window_buffer_type buffer_type;

	EGLImageKHR *image;
	cairo_surface_t *cairo_surface, *pending_surface;

	window_resize_handler_t resize_handler;
	window_redraw_handler_t redraw_handler;
	window_key_handler_t key_handler;
	window_button_handler_t button_handler;
	window_keyboard_focus_handler_t keyboard_focus_handler;
	window_motion_handler_t motion_handler;

	void *user_data;
	struct wl_list link;
};

struct input {
	struct display *display;
	struct wl_input_device *input_device;
	struct window *pointer_focus;
	struct window *keyboard_focus;
	uint32_t current_pointer_image;
	uint32_t modifiers;
	int32_t x, y, sx, sy;
	struct wl_list link;
};

enum {
	POINTER_DEFAULT = 100,
	POINTER_UNSET
};

const char *option_xkb_layout = "us";
const char *option_xkb_variant = "";
const char *option_xkb_options = "";

static const GOptionEntry xkb_option_entries[] = {
	{ "xkb-layout", 0, 0, G_OPTION_ARG_STRING,
	  &option_xkb_layout, "XKB Layout" },
	{ "xkb-variant", 0, 0, G_OPTION_ARG_STRING,
	  &option_xkb_variant, "XKB Variant" },
	{ "xkb-options", 0, 0, G_OPTION_ARG_STRING,
	  &option_xkb_options, "XKB Options" },
	{ NULL }
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
	struct wl_buffer *buffer;
};

#define MULT(_d,c,a,t) \
	do { t = c * a + 0x7f; _d = ((t >> 8) + t) >> 8; } while (0)

#ifdef HAVE_CAIRO_GL

struct drm_surface_data {
	struct surface_data data;
	EGLImageKHR image;
	GLuint texture;
	struct display *display;
};

static void
drm_surface_data_destroy(void *p)
{
	struct drm_surface_data *data = p;
	struct display *d = data->display;

	cairo_device_acquire(d->device);
	glDeleteTextures(1, &data->texture);
	cairo_device_release(d->device);

	eglDestroyImageKHR(d->dpy, data->image);
	wl_buffer_destroy(data->data.buffer);
}

EGLImageKHR
display_get_image_for_drm_surface(struct display *display,
				  cairo_surface_t *surface)
{
	struct drm_surface_data *data;

	data = cairo_surface_get_user_data (surface, &surface_data_key);

	return data->image;
}

static cairo_surface_t *
display_create_drm_surface(struct display *display,
			   struct rectangle *rectangle)
{
	struct drm_surface_data *data;
	EGLDisplay dpy = display->dpy;
	cairo_surface_t *surface;
	struct wl_visual *visual;
	EGLint name, stride;

	EGLint image_attribs[] = {
		EGL_WIDTH,		0,
		EGL_HEIGHT,		0,
		EGL_DRM_BUFFER_FORMAT_MESA,	EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
		EGL_DRM_BUFFER_USE_MESA,	EGL_DRM_BUFFER_USE_SCANOUT_MESA,
		EGL_NONE
	};

	data = malloc(sizeof *data);
	if (data == NULL)
		return NULL;

	data->display = display;

	image_attribs[1] = rectangle->width;
	image_attribs[3] = rectangle->height;
	data->image = eglCreateDRMImageMESA(dpy, image_attribs);

	cairo_device_acquire(display->device);
	glGenTextures(1, &data->texture);
	glBindTexture(GL_TEXTURE_2D, data->texture);
	glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, data->image);
	cairo_device_release(display->device);

	eglExportDRMImageMESA(display->dpy, data->image, &name, NULL, &stride);

	visual = wl_display_get_premultiplied_argb_visual(display->display);
	data->data.buffer =
		wl_drm_create_buffer(display->drm, name, rectangle->width,
				     rectangle->height, stride, visual);

	surface = cairo_gl_surface_create_for_texture(display->device,
						      CAIRO_CONTENT_COLOR_ALPHA,
						      data->texture,
						      rectangle->width,
						      rectangle->height);

	cairo_surface_set_user_data (surface, &surface_data_key,
				     data, drm_surface_data_destroy);

	return surface;
}

static cairo_surface_t *
display_create_drm_surface_from_file(struct display *display,
				     const char *filename,
				     struct rectangle *rect)
{
	cairo_surface_t *surface;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	int stride, i;
	unsigned char *pixels, *p, *end;
	struct drm_surface_data *data;

	pixbuf = gdk_pixbuf_new_from_file_at_scale(filename,
						   rect->width, rect->height,
						   FALSE, &error);
	if (error != NULL)
		return NULL;

	if (!gdk_pixbuf_get_has_alpha(pixbuf) ||
	    gdk_pixbuf_get_n_channels(pixbuf) != 4) {
		gdk_pixbuf_unref(pixbuf);
		return NULL;
	}


	stride = gdk_pixbuf_get_rowstride(pixbuf);
	pixels = gdk_pixbuf_get_pixels(pixbuf);

	for (i = 0; i < rect->height; i++) {
		p = pixels + i * stride;
		end = p + rect->width * 4;
		while (p < end) {
			unsigned int t;

			MULT(p[0], p[0], p[3], t);
			MULT(p[1], p[1], p[3], t);
			MULT(p[2], p[2], p[3], t);
			p += 4;

		}
	}

	surface = display_create_drm_surface(display, rect);
	data = cairo_surface_get_user_data(surface, &surface_data_key);

	cairo_device_acquire(display->device);
	glBindTexture(GL_TEXTURE_2D, data->texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rect->width, rect->height,
			GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	cairo_device_release(display->device);

	gdk_pixbuf_unref(pixbuf);

	return surface;
}

#endif

struct wl_buffer *
display_get_buffer_for_surface(struct display *display,
			       cairo_surface_t *surface)
{
	struct surface_data *data;

	data = cairo_surface_get_user_data (surface, &surface_data_key);

	return data->buffer;
}

struct shm_surface_data {
	struct surface_data data;
	void *map;
	size_t length;
};

static void
shm_surface_data_destroy(void *p)
{
	struct shm_surface_data *data = p;

	wl_buffer_destroy(data->data.buffer);
	munmap(data->map, data->length);
}

static cairo_surface_t *
display_create_shm_surface(struct display *display,
			   struct rectangle *rectangle)
{
	struct shm_surface_data *data;
	cairo_surface_t *surface;
	struct wl_visual *visual;
	int stride, fd;
	char filename[] = "/tmp/wayland-shm-XXXXXX";

	data = malloc(sizeof *data);
	if (data == NULL)
		return NULL;

	stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32,
						rectangle->width);
	data->length = stride * rectangle->height;
	fd = mkstemp(filename);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %m", filename);
		return NULL;
	}
	if (ftruncate(fd, data->length) < 0) {
		fprintf(stderr, "ftruncate failed: %m");
		close(fd);
		return NULL;
	}

	data->map = mmap(NULL, data->length,
			 PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	unlink(filename);

	if (data->map == MAP_FAILED) {
		fprintf(stderr, "mmap failed: %m");
		close(fd);
		return NULL;
	}

	surface = cairo_image_surface_create_for_data (data->map,
						       CAIRO_FORMAT_ARGB32,
						       rectangle->width,
						       rectangle->height,
						       stride);

	cairo_surface_set_user_data (surface, &surface_data_key,
				     data, shm_surface_data_destroy);

	visual = wl_display_get_premultiplied_argb_visual(display->display);
	data->data.buffer = wl_shm_create_buffer(display->shm,
						 fd,
						 rectangle->width,
						 rectangle->height,
						 stride, visual);

	close(fd);

	return surface;
}

static cairo_surface_t *
display_create_shm_surface_from_file(struct display *display,
				     const char *filename,
				     struct rectangle *rect)
{
	cairo_surface_t *surface;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	int stride, i;
	unsigned char *pixels, *p, *end, *dest_data;
	int dest_stride;
	uint32_t *d;

	pixbuf = gdk_pixbuf_new_from_file_at_scale(filename,
						   rect->width, rect->height,
						   FALSE, &error);
	if (error != NULL)
		return NULL;

	if (!gdk_pixbuf_get_has_alpha(pixbuf) ||
	    gdk_pixbuf_get_n_channels(pixbuf) != 4) {
		gdk_pixbuf_unref(pixbuf);
		return NULL;
	}

	stride = gdk_pixbuf_get_rowstride(pixbuf);
	pixels = gdk_pixbuf_get_pixels(pixbuf);

	surface = display_create_shm_surface(display, rect);
	dest_data = cairo_image_surface_get_data (surface);
	dest_stride = cairo_image_surface_get_stride (surface);

	for (i = 0; i < rect->height; i++) {
		d = (uint32_t *) (dest_data + i * dest_stride);
		p = pixels + i * stride;
		end = p + rect->width * 4;
		while (p < end) {
			unsigned int t;
			unsigned char a, r, g, b;

			a = p[3];
			MULT(r, p[0], a, t);
			MULT(g, p[1], a, t);
			MULT(b, p[2], a, t);
			p += 4;
			*d++ = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}

	gdk_pixbuf_unref(pixbuf);

	return surface;
}

cairo_surface_t *
display_create_surface(struct display *display,
		       struct rectangle *rectangle)
{
#ifdef HAVE_CAIRO_GL
	return display_create_drm_surface(display, rectangle);
#else
	return display_create_shm_surface(display, rectangle);
#endif
}

static cairo_surface_t *
display_create_surface_from_file(struct display *display,
				 const char *filename,
				 struct rectangle *rectangle)
{
#ifdef HAVE_CAIRO_GL
	return display_create_drm_surface_from_file(display, filename, rectangle);
#else
	return display_create_shm_surface_from_file(display, filename, rectangle);
#endif
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
	{ DATADIR "/wayland/xterm.png",			15, 15 },
	{ DATADIR "/wayland/hand1.png",			18, 11 }
};

static void
create_pointer_surfaces(struct display *display)
{
	int i, count;
	const int width = 32, height = 32;
	struct rectangle rect;

	count = ARRAY_LENGTH(pointer_images);
	display->pointer_surfaces =
		malloc(count * sizeof *display->pointer_surfaces);
	rect.width = width;
	rect.height = height;
	for (i = 0; i < count; i++) {
		display->pointer_surfaces[i] =
			display_create_surface_from_file(display,
							 pointer_images[i].filename,
							 &rect);
	}

}

cairo_surface_t *
display_get_pointer_surface(struct display *display, int pointer,
			    int *width, int *height,
			    int *hotspot_x, int *hotspot_y)
{
	cairo_surface_t *surface;

	surface = display->pointer_surfaces[pointer];
	*width = cairo_gl_surface_get_width(surface);
	*height = cairo_gl_surface_get_height(surface);
	*hotspot_x = pointer_images[pointer].hotspot_x;
	*hotspot_y = pointer_images[pointer].hotspot_y;

	return cairo_surface_reference(surface);
}


static void
window_attach_surface(struct window *window);

static void
free_surface(void *data)
{
	struct window *window = data;

	cairo_surface_destroy(window->pending_surface);
	window->pending_surface = NULL;
	if (window->cairo_surface)
		window_attach_surface(window);
}

static void
window_attach_surface(struct window *window)
{
	struct display *display = window->display;
	struct wl_buffer *buffer;

	if (window->pending_surface != NULL)
		return;

	window->pending_surface = window->cairo_surface;
	window->cairo_surface = NULL;

	buffer = display_get_buffer_for_surface(display,
						window->pending_surface);
	wl_surface_attach(window->surface, buffer);

	wl_surface_map(window->surface,
		       window->allocation.x,
		       window->allocation.y,
		       window->allocation.width,
		       window->allocation.height);

	wl_display_sync_callback(display->display, free_surface, window);
}

void
window_flush(struct window *window)
{
       if (window->cairo_surface)
	       window_attach_surface(window);
}

void
window_set_surface(struct window *window, cairo_surface_t *surface)
{
	cairo_surface_reference(surface);

	if (window->cairo_surface != NULL)
		cairo_surface_destroy(window->cairo_surface);

	window->cairo_surface = surface;
}

void
window_create_surface(struct window *window)
{
	cairo_surface_t *surface;

	switch (window->buffer_type) {
#ifdef HAVE_CAIRO_GL
	case WINDOW_BUFFER_TYPE_DRM:
		surface = display_create_surface(window->display,
						 &window->allocation);
		break;
#endif
	case WINDOW_BUFFER_TYPE_SHM:
		surface = display_create_shm_surface(window->display,
						     &window->allocation);
		break;
        default:
		surface = NULL;
		break;
	}

	window_set_surface(window, surface);
	cairo_surface_destroy(surface);
}

static void
window_draw_decorations(struct window *window)
{
	cairo_t *cr;
	cairo_text_extents_t extents;
	cairo_pattern_t *outline, *bright, *dim;
	cairo_surface_t *frame;
	int width, height, shadow_dx = 3, shadow_dy = 3;

	window_create_surface(window);

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
	tile_mask(cr, window->display->shadow,
		  shadow_dx, shadow_dy, width, height,
		  window->margin + 10 - shadow_dx,
		  window->margin + 10 - shadow_dy);

	if (window->keyboard_device)
		frame = window->display->active_frame;
	else
		frame = window->display->inactive_frame;

	tile_source(cr, frame, 0, 0, width, height,
		    window->margin + 10, window->margin + 50);

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

	cairo_device_flush (window->display->device);
}

void
display_flush_cairo_device(struct display *display)
{
	cairo_device_flush (display->device);
}

void
window_draw(struct window *window)
{
	if (window->fullscreen || !window->decoration)
		window_create_surface(window);
	else
		window_draw_decorations(window);
}

cairo_surface_t *
window_get_surface(struct window *window)
{
	return cairo_surface_reference(window->cairo_surface);
}

enum window_location {
	WINDOW_INTERIOR = 0,
	WINDOW_RESIZING_TOP = 1,
	WINDOW_RESIZING_BOTTOM = 2,
	WINDOW_RESIZING_LEFT = 4,
	WINDOW_RESIZING_TOP_LEFT = 5,
	WINDOW_RESIZING_BOTTOM_LEFT = 6,
	WINDOW_RESIZING_RIGHT = 8,
	WINDOW_RESIZING_TOP_RIGHT = 9,
	WINDOW_RESIZING_BOTTOM_RIGHT = 10,
	WINDOW_RESIZING_MASK = 15,
	WINDOW_EXTERIOR = 16,
	WINDOW_TITLEBAR = 17,
	WINDOW_CLIENT_AREA = 18,
};

static int
get_pointer_location(struct window *window, int32_t x, int32_t y)
{
	int vlocation, hlocation, location;
	const int grip_size = 8;

	if (x < window->margin)
		hlocation = WINDOW_EXTERIOR;
	else if (window->margin <= x && x < window->margin + grip_size)
		hlocation = WINDOW_RESIZING_LEFT;
	else if (x < window->allocation.width - window->margin - grip_size)
		hlocation = WINDOW_INTERIOR;
	else if (x < window->allocation.width - window->margin)
		hlocation = WINDOW_RESIZING_RIGHT;
	else
		hlocation = WINDOW_EXTERIOR;

	if (y < window->margin)
		vlocation = WINDOW_EXTERIOR;
	else if (window->margin <= y && y < window->margin + grip_size)
		vlocation = WINDOW_RESIZING_TOP;
	else if (y < window->allocation.height - window->margin - grip_size)
		vlocation = WINDOW_INTERIOR;
	else if (y < window->allocation.height - window->margin)
		vlocation = WINDOW_RESIZING_BOTTOM;
	else
		vlocation = WINDOW_EXTERIOR;

	location = vlocation | hlocation;
	if (location & WINDOW_EXTERIOR)
		location = WINDOW_EXTERIOR;
	if (location == WINDOW_INTERIOR && y < window->margin + 50)
		location = WINDOW_TITLEBAR;
	else if (location == WINDOW_INTERIOR)
		location = WINDOW_CLIENT_AREA;

	return location;
}

static void
set_pointer_image(struct input *input, uint32_t time, int pointer)
{
	struct display *display = input->display;
	struct wl_buffer *buffer;
	cairo_surface_t *surface;
	int location;

	location = get_pointer_location(input->pointer_focus,
					input->sx, input->sy);
	switch (location) {
	case WINDOW_RESIZING_TOP:
		pointer = POINTER_TOP;
		break;
	case WINDOW_RESIZING_BOTTOM:
		pointer = POINTER_BOTTOM;
		break;
	case WINDOW_RESIZING_LEFT:
		pointer = POINTER_LEFT;
		break;
	case WINDOW_RESIZING_RIGHT:
		pointer = POINTER_RIGHT;
		break;
	case WINDOW_RESIZING_TOP_LEFT:
		pointer = POINTER_TOP_LEFT;
		break;
	case WINDOW_RESIZING_TOP_RIGHT:
		pointer = POINTER_TOP_RIGHT;
		break;
	case WINDOW_RESIZING_BOTTOM_LEFT:
		pointer = POINTER_BOTTOM_LEFT;
		break;
	case WINDOW_RESIZING_BOTTOM_RIGHT:
		pointer = POINTER_BOTTOM_RIGHT;
		break;
	case WINDOW_EXTERIOR:
	case WINDOW_TITLEBAR:
		if (input->current_pointer_image == POINTER_DEFAULT)
			return;

		wl_input_device_attach(input->input_device, time, NULL, 0, 0);
		input->current_pointer_image = POINTER_DEFAULT;
		return;
	default:
		break;
	}

	if (pointer == input->current_pointer_image)
		return;

	input->current_pointer_image = pointer;
	surface = display->pointer_surfaces[pointer];
	buffer = display_get_buffer_for_surface(display, surface);
	wl_input_device_attach(input->input_device, time, buffer,
			       pointer_images[pointer].hotspot_x,
			       pointer_images[pointer].hotspot_y);
}

static void
window_handle_motion(void *data, struct wl_input_device *input_device,
		     uint32_t time,
		     int32_t x, int32_t y, int32_t sx, int32_t sy)
{
	struct input *input = data;
	struct window *window = input->pointer_focus;
	int location, pointer = POINTER_LEFT_PTR;

	input->x = x;
	input->y = y;
	input->sx = sx;
	input->sy = sy;

	location = get_pointer_location(window, input->sx, input->sy);

	if (window->motion_handler)
		pointer = (*window->motion_handler)(window, input, time,
						    x, y, sx, sy,
						    window->user_data);

	set_pointer_image(input, time, pointer);
}

static void
window_handle_button(void *data,
		     struct wl_input_device *input_device,
		     uint32_t time, uint32_t button, uint32_t state)
{
	struct input *input = data;
	struct window *window = input->pointer_focus;
	int location;

	location = get_pointer_location(window, input->sx, input->sy);

	if (button == BTN_LEFT && state == 1) {
		switch (location) {
		case WINDOW_TITLEBAR:
			wl_shell_move(window->display->shell,
				      window->surface, input_device, time);
			break;
		case WINDOW_RESIZING_TOP:
		case WINDOW_RESIZING_BOTTOM:
		case WINDOW_RESIZING_LEFT:
		case WINDOW_RESIZING_RIGHT:
		case WINDOW_RESIZING_TOP_LEFT:
		case WINDOW_RESIZING_TOP_RIGHT:
		case WINDOW_RESIZING_BOTTOM_LEFT:
		case WINDOW_RESIZING_BOTTOM_RIGHT:
			wl_shell_resize(window->display->shell,
					window->surface, input_device, time,
					location);
			break;
		case WINDOW_CLIENT_AREA:
			if (window->button_handler)
				(*window->button_handler)(window,
							  input, time,
							  button, state,
							  window->user_data);
			break;
		}
	} else {
		if (window->button_handler)
			(*window->button_handler)(window,
						  input, time,
						  button, state,
						  window->user_data);
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
			    uint32_t time, struct wl_surface *surface,
			    int32_t x, int32_t y, int32_t sx, int32_t sy)
{
	struct input *input = data;
	struct window *window;
	int pointer;

	if (surface) {
		input->pointer_focus = wl_surface_get_user_data(surface);
		window = input->pointer_focus;

		pointer = POINTER_LEFT_PTR;
		if (window->motion_handler)
			pointer = (*window->motion_handler)(window,
							    input, time,
							    x, y, sx, sy,
							    window->user_data);

		set_pointer_image(input, time, pointer);
	} else {
		input->pointer_focus = NULL;
		input->current_pointer_image = POINTER_UNSET;
	}
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
input_get_position(struct input *input, int32_t *x, int32_t *y)
{
	*x = input->sx;
	*y = input->sy;
}

struct wl_input_device *
input_get_input_device(struct input *input)
{
	return input->input_device;
}

struct wl_drag *
window_create_drag(struct window *window)
{
	cairo_device_flush (window->display->device);

	return wl_shell_create_drag(window->display->shell);
}

void
window_activate_drag(struct wl_drag *drag, struct window *window,
		     struct input *input, uint32_t time)
{
	wl_drag_activate(drag, window->surface, input->input_device, time);
}

static void
handle_configure(void *data, struct wl_shell *shell,
		 uint32_t time, uint32_t edges,
		 struct wl_surface *surface,
		 int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct window *window = wl_surface_get_user_data(surface);

	window->resize_edges = edges;
	window->pending_allocation.x = x;
	window->pending_allocation.y = y;
	window->pending_allocation.width = width;
	window->pending_allocation.height = height;

	if (edges & WINDOW_TITLEBAR) {
		window->allocation.x = window->pending_allocation.x;
		window->allocation.y = window->pending_allocation.y;
	} else if (edges & WINDOW_RESIZING_MASK) {
		if (window->resize_handler)
			(*window->resize_handler)(window,
						  window->user_data);
		else if (window->redraw_handler)
			window_schedule_redraw(window);
	}
}

static const struct wl_shell_listener shell_listener = {
	handle_configure,
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

		if (window->resize_edges & WINDOW_RESIZING_LEFT)
			window->allocation.x +=
				window->allocation.width - width;
		if (window->resize_edges & WINDOW_RESIZING_TOP)
			window->allocation.y +=
				window->allocation.height - height;

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

	if (window->resize_edges)
		window->allocation = window->pending_allocation;

	window->redraw_handler(window, window->user_data);

	window->redraw_scheduled = 0;
	window->resize_edges = 0;

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

void *
window_get_user_data(struct window *window)
{
	return window->user_data;
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
window_set_button_handler(struct window *window,
			  window_button_handler_t handler)
{
	window->button_handler = handler;
}

void
window_set_motion_handler(struct window *window,
			  window_motion_handler_t handler)
{
	window->motion_handler = handler;
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

void
window_damage(struct window *window, int32_t x, int32_t y,
	      int32_t width, int32_t height)
{
	wl_surface_damage(window->surface, x, y, width, height);
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
	window->decoration = 1;

#ifdef HAVE_CAIRO_GL
	window->buffer_type = WINDOW_BUFFER_TYPE_DRM;
#else
	window->buffer_type = WINDOW_BUFFER_TYPE_SHM;
#endif

	wl_surface_set_user_data(window->surface, window);
	wl_list_insert(display->window_list.prev, &window->link);

	return window;
}

void
window_set_buffer_type(struct window *window, enum window_buffer_type type)
{
	window->buffer_type = type;
}

static void
drm_handle_device(void *data, struct wl_drm *drm, const char *device)
{
	struct display *d = data;

	d->device_name = strdup(device);
}

static void drm_handle_authenticated(void *data, struct wl_drm *drm)
{
	struct display *d = data;

	d->authenticated = 1;
}

static const struct wl_drm_listener drm_listener = {
	drm_handle_device,
	drm_handle_authenticated
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
display_add_input(struct display *d, uint32_t id)
{
	struct input *input;

	input = malloc(sizeof *input);
	if (input == NULL)
		return;

	memset(input, 0, sizeof *input);
	input->display = d;
	input->input_device = wl_input_device_create(d->display, id);
	input->pointer_focus = NULL;
	input->keyboard_focus = NULL;
	wl_list_insert(d->input_list.prev, &input->link);

	wl_input_device_add_listener(input->input_device,
				     &input_device_listener, input);
	wl_input_device_set_user_data(input->input_device, input);
}

static void
display_handle_global(struct wl_display *display, uint32_t id,
		      const char *interface, uint32_t version, void *data)
{
	struct display *d = data;
	struct wl_drag_offer *offer;

	if (strcmp(interface, "compositor") == 0) {
		d->compositor = wl_compositor_create(display, id);
	} else if (strcmp(interface, "output") == 0) {
		d->output = wl_output_create(display, id);
		wl_output_add_listener(d->output, &output_listener, d);
	} else if (strcmp(interface, "input_device") == 0) {
		display_add_input(d, id);
	} else if (strcmp(interface, "shell") == 0) {
		d->shell = wl_shell_create(display, id);
		wl_shell_add_listener(d->shell, &shell_listener, d);
	} else if (strcmp(interface, "drm") == 0) {
		d->drm = wl_drm_create(display, id);
		wl_drm_add_listener(d->drm, &drm_listener, d);
	} else if (strcmp(interface, "shm") == 0) {
		d->shm = wl_shm_create(display, id);
	} else if (strcmp(interface, "drag_offer") == 0) {
		if (d->drag_offer_handler) {
			offer = wl_drag_offer_create(display, id);
			d->drag_offer_handler(offer, d);
		}
	}
}

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
	names.layout = option_xkb_layout;
	names.variant = option_xkb_variant;
	names.options = option_xkb_options;

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
	GOptionGroup *xkb_option_group;
	GError *error;
	drm_magic_t magic;

	g_type_init();

	context = g_option_context_new(NULL);
	if (option_entries)
		g_option_context_add_main_entries(context, option_entries, "Wayland View");

	xkb_option_group = g_option_group_new("xkb",
					      "Keyboard options",
					      "Show all XKB options",
					      NULL, NULL);
	g_option_group_add_entries(xkb_option_group, xkb_option_entries);
	g_option_context_add_group (context, xkb_option_group);

	if (!g_option_context_parse(context, argc, argv, &error)) {
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		exit(EXIT_FAILURE);
	}


	d = malloc(sizeof *d);
	if (d == NULL)
		return NULL;

	d->display = wl_display_connect(NULL);
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

	if (drmGetMagic(fd, &magic)) {
		fprintf(stderr, "DRI2: failed to get drm magic");
		return NULL;
	}

	/* Wait for authenticated event */
	wl_drm_authenticate(d->drm, magic);
	wl_display_iterate(d->display, WL_DISPLAY_WRITABLE);
	while (!d->authenticated)
		wl_display_iterate(d->display, WL_DISPLAY_READABLE);

	d->dpy = eglGetDRMDisplayMESA(fd);
	if (!eglInitialize(d->dpy, &major, &minor)) {
		fprintf(stderr, "failed to initialize display\n");
		return NULL;
	}

	if (!eglBindAPI(EGL_OPENGL_API)) {
		fprintf(stderr, "failed to bind api EGL_OPENGL_API\n");
		return NULL;
	}

	d->ctx = eglCreateContext(d->dpy, NULL, EGL_NO_CONTEXT, NULL);
	if (d->ctx == NULL) {
		fprintf(stderr, "failed to create context\n");
		return NULL;
	}

	if (!eglMakeCurrent(d->dpy, NULL, NULL, d->ctx)) {
		fprintf(stderr, "faile to make context current\n");
		return NULL;
	}

#ifdef HAVE_CAIRO_GL
	d->device = cairo_egl_device_create(d->dpy, d->ctx);
	if (d->device == NULL) {
		fprintf(stderr, "failed to get cairo drm device\n");
		return NULL;
	}
#endif

	create_pointer_surfaces(d);

	display_render_frame(d);

	d->loop = g_main_loop_new(NULL, FALSE);
	d->source = wl_glib_source_new(d->display);
	g_source_attach(d->source, NULL);

	wl_list_init(&d->window_list);

	init_xkb(d);

	return d;
}

struct wl_display *
display_get_display(struct display *display)
{
	return display->display;
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

void
display_set_drag_offer_handler(struct display *display,
			       display_drag_offer_handler_t handler)
{
	display->drag_offer_handler = handler;
}
