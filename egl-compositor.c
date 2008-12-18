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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <sys/poll.h>
#include <png.h>
#include <math.h>
#include <linux/input.h>
#include <xf86drmMode.h>
#include <time.h>
#include <fnmatch.h>
#include <dirent.h>

#include <GL/gl.h>
#include <eagle.h>

#include "wayland.h"
#include "cairo-util.h"
#include "egl-compositor.h"

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

struct egl_input_device {
	struct wl_object base;
	int32_t x, y;
	struct egl_compositor *ec;
	struct egl_surface *pointer_surface;
	struct wl_list link;

	int grab;
	struct egl_surface *grab_surface;
	struct egl_surface *focus_surface;
};

struct egl_compositor {
	struct wl_compositor base;
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	EGLConfig config;
	struct wl_display *wl_display;
	int gem_fd;
	int width, height;
	struct egl_surface *background;
	struct egl_surface *overlay;
	double overlay_y, overlay_target, overlay_previous;

	struct wl_list input_device_list;
	struct wl_list surface_list;

	/* Repaint state. */
	struct wl_event_source *timer_source;
	int repaint_needed;
	int repaint_on_timeout;
	struct timespec previous_swap;
	uint32_t current_frame;
};

struct egl_surface {
	struct wl_surface base;
	struct egl_compositor *compositor;
	GLuint texture;
	struct wl_map map;
	EGLSurface surface;
	int width, height;
	struct wl_list link;
};

struct screenshooter {
	struct wl_object base;
	struct egl_compositor *ec;
};

struct screenshooter_interface {
	void (*shoot)(struct wl_client *client, struct screenshooter *shooter);
};

static void
screenshooter_shoot(struct wl_client *client, struct screenshooter *shooter)
{
	struct egl_compositor *ec = shooter->ec;
	GLuint stride;
	static const char filename[]  = "wayland-screenshot.png";
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	void *data;

	data = eglReadBuffer(ec->display, ec->surface, GL_FRONT_LEFT, &stride);
	pixbuf = gdk_pixbuf_new_from_data(data, GDK_COLORSPACE_RGB, TRUE,
					  8, ec->width, ec->height, stride,
					  NULL, NULL);
	gdk_pixbuf_save(pixbuf, filename, "png", &error, NULL);
}

static const struct wl_method screenshooter_methods[] = {
	{ "shoot", "", NULL }
};

static const struct wl_interface screenshooter_interface = {
	"screenshooter", 1,
	ARRAY_LENGTH(screenshooter_methods),
	screenshooter_methods,
};

struct screenshooter_interface screenshooter_implementation = {
	screenshooter_shoot
};

static struct screenshooter *
screenshooter_create(struct egl_compositor *ec)
{
	struct screenshooter *shooter;

	shooter = malloc(sizeof *shooter);
	if (shooter == NULL)
		return NULL;

	shooter->base.interface = &screenshooter_interface;
	shooter->base.implementation = (void(**)(void)) &screenshooter_implementation;
	shooter->ec = ec;

	return shooter;
};

static struct egl_surface *
egl_surface_create_from_cairo_surface(cairo_surface_t *surface,
				      int x, int y, int width, int height)
{
	struct egl_surface *es;
	int stride;
	void *data;

	stride = cairo_image_surface_get_stride(surface);
	data = cairo_image_surface_get_data(surface);

	es = malloc(sizeof *es);
	if (es == NULL)
		return NULL;

	glGenTextures(1, &es->texture);
	glBindTexture(GL_TEXTURE_2D, es->texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
		     GL_BGRA, GL_UNSIGNED_BYTE, data);

	es->map.x = x;
	es->map.y = y;
	es->map.width = width;
	es->map.height = height;
	es->surface = EGL_NO_SURFACE;

	return es;
}

static void
egl_surface_destroy(struct egl_surface *es, struct egl_compositor *ec)
{
	glDeleteTextures(1, &es->texture);
	if (es->surface != EGL_NO_SURFACE)
		eglDestroySurface(ec->display, es->surface);
	free(es);
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

static struct egl_surface *
pointer_create(int x, int y, int width, int height)
{
	struct egl_surface *es;
	const int hotspot_x = 16, hotspot_y = 16;
	cairo_surface_t *surface;
	cairo_t *cr;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
					     width, height);

	cr = cairo_create(surface);
	pointer_path(cr, hotspot_x + 5, hotspot_y + 4);
	cairo_set_line_width (cr, 2);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_stroke_preserve(cr);
	cairo_fill(cr);
	blur_surface(surface, width);

	pointer_path(cr, hotspot_x, hotspot_y);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill(cr);
	cairo_destroy(cr);

	es = egl_surface_create_from_cairo_surface(surface,
						   x - hotspot_x,
						   y - hotspot_y,
						   width, height);
	
	cairo_surface_destroy(surface);

	return es;
}

static struct egl_surface *
background_create(const char *filename, int width, int height)
{
	struct egl_surface *background;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	int pixbuf_width, pixbuf_height;
	void *data;

	background = malloc(sizeof *background);
	if (background == NULL)
		return NULL;
	
	g_type_init();

	pixbuf = gdk_pixbuf_new_from_file(filename, &error);
	if (error != NULL) {
		free(background);
		return NULL;
	}

	pixbuf_width = gdk_pixbuf_get_width(pixbuf);
	pixbuf_height = gdk_pixbuf_get_height(pixbuf);
	data = gdk_pixbuf_get_pixels(pixbuf);

	glGenTextures(1, &background->texture);
	glBindTexture(GL_TEXTURE_2D, background->texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, pixbuf_width, pixbuf_height, 0,
		     GL_BGR, GL_UNSIGNED_BYTE, data);

	background->map.x = 0;
	background->map.y = 0;
	background->map.width = width;
	background->map.height = height;
	background->surface = EGL_NO_SURFACE;

	return background;
}

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

static void
draw_button(cairo_t *cr, int x, int y, int width, int height, const char *text)
{
	cairo_pattern_t *gradient;
	cairo_text_extents_t extents;
	double bright = 0.15, dim = 0.02;
	int radius = 10;

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_line_width (cr, 2);
	rounded_rect(cr, x, y, x + width, y + height, radius);
	cairo_set_source_rgb(cr, dim, dim, dim);
	cairo_stroke(cr);
	rounded_rect(cr, x + 2, y + 2, x + width, y + height, radius);
	cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
	cairo_stroke(cr);

	rounded_rect(cr, x + 1, y + 1, x + width - 1, y + height - 1, radius - 1);
	cairo_set_source_rgb(cr, bright, bright, bright);
	cairo_stroke(cr);
	rounded_rect(cr, x + 3, y + 3, x + width - 1, y + height - 1, radius - 1);
	cairo_set_source_rgb(cr, dim, dim, dim);
	cairo_stroke(cr);

	rounded_rect(cr, x + 1, y + 1, x + width - 1, y + height - 1, radius - 1);
	gradient = cairo_pattern_create_linear (0, y, 0, y + height);
	cairo_pattern_add_color_stop_rgb(gradient, 0, 0.15, 0.15, 0.15);
	cairo_pattern_add_color_stop_rgb(gradient, 0.5, 0.08, 0.08, 0.08);
	cairo_pattern_add_color_stop_rgb(gradient, 0.5, 0.07, 0.07, 0.07);
	cairo_pattern_add_color_stop_rgb(gradient, 1, 0.1, 0.1, 0.1);
	cairo_set_source(cr, gradient);
	cairo_fill(cr);

	cairo_set_font_size(cr, 16);
	cairo_text_extents(cr, text, &extents);
	cairo_move_to(cr, x + (width - extents.width) / 2, y + (height - extents.height) / 2 - extents.y_bearing);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_line_width (cr, 4);
	cairo_text_path(cr, text);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill(cr);
}

static struct egl_surface *
overlay_create(int x, int y, int width, int height)
{
	struct egl_surface *es;
	cairo_surface_t *surface;
	cairo_t *cr;
	int total_width, button_x, button_y;
	const int button_width = 150;
	const int button_height = 40;
	const int spacing = 50;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
					     width, height);

	cr = cairo_create(surface);
	cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.8);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);

	total_width = button_width * 2 + spacing;
	button_x = (width - total_width) / 2;
	button_y = height - button_height - 20;
	draw_button(cr, button_x, button_y, button_width, button_height, "Previous");
	button_x += button_width + spacing;
	draw_button(cr, button_x, button_y, button_width, button_height, "Next");

	cairo_destroy(cr);

	es = egl_surface_create_from_cairo_surface(surface, x, y, width, height);

	cairo_surface_destroy(surface);

	return es;	
}

static void
draw_surface(struct egl_surface *es)
{
	GLint vertices[12];
	GLint tex_coords[12] = { 0, 0,  0, 1,  1, 0,  1, 1 };
	GLuint indices[4] = { 0, 1, 2, 3 };

	vertices[0] = es->map.x;
	vertices[1] = es->map.y;
	vertices[2] = 0;

	vertices[3] = es->map.x;
	vertices[4] = es->map.y + es->map.height;
	vertices[5] = 0;

	vertices[6] = es->map.x + es->map.width;
	vertices[7] = es->map.y;
	vertices[8] = 0;

	vertices[9] = es->map.x + es->map.width;
	vertices[10] = es->map.y + es->map.height;
	vertices[11] = 0;

	glBindTexture(GL_TEXTURE_2D, es->texture);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	/* Assume pre-multiplied alpha for now, this probably
	 * needs to be a wayland visual type of thing. */
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(3, GL_INT, 0, vertices);
	glTexCoordPointer(2, GL_INT, 0, tex_coords);
	glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, indices);
}

static void
schedule_repaint(struct egl_compositor *ec);

static void
animate_overlay(struct egl_compositor *ec)
{
	double force, y;
	int32_t top, bottom;
#if 1
	double bounce = 0.0;
	double friction = 1.0;
	double spring = 0.2;
#else
	double bounce = 0.2;
	double friction = 0.04;
	double spring = 0.09;
#endif

	y = ec->overlay_y;
	force = (ec->overlay_target - ec->overlay_y) * spring +
		(ec->overlay_previous - y) * friction;
	
	ec->overlay_y = y + (y - ec->overlay_previous) + force;
	ec->overlay_previous = y;

	top = ec->height - ec->overlay->map.height;
	bottom = ec->height;
	if (ec->overlay_y >= bottom) {
		ec->overlay_y = bottom;
		ec->overlay_previous = bottom;
	}

	if (ec->overlay_y <= top) {
		ec->overlay_y = top + bounce * (top - ec->overlay_y);
		ec->overlay_previous =
			top + bounce * (top - ec->overlay_previous);
	}

	ec->overlay->map.y = ec->overlay_y + 0.5;

	if (fabs(y - ec->overlay_target) > 0.2 ||
	    fabs(ec->overlay_y - ec->overlay_target) > 0.2)
		schedule_repaint(ec);
}

static void
repaint(void *data)
{
	struct egl_compositor *ec = data;
	struct egl_surface *es;
	struct egl_input_device *eid;
	struct timespec ts;
	uint32_t msecs;

	if (!ec->repaint_needed) {
		ec->repaint_on_timeout = 0;
		return;
	}

	if (ec->background)
		draw_surface(ec->background);
	else
		glClear(GL_COLOR_BUFFER_BIT);

	es = container_of(ec->surface_list.next,
			  struct egl_surface, link);
	while (&es->link != &ec->surface_list) {
		draw_surface(es);

		es = container_of(es->link.next,
				   struct egl_surface, link);
	}

	draw_surface(ec->overlay);

	eid = container_of(ec->input_device_list.next,
			   struct egl_input_device, link);
	while (&eid->link != &ec->input_device_list) {
		draw_surface(eid->pointer_surface);

		eid = container_of(eid->link.next,
				   struct egl_input_device, link);
	}

	eglSwapBuffers(ec->display, ec->surface);
	ec->repaint_needed = 0;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	msecs = ts.tv_sec * 1000 + ts.tv_nsec / (1000 * 1000);
	wl_display_post_frame(ec->wl_display, &ec->base,
			      ec->current_frame, msecs);
	ec->current_frame++;

	wl_event_source_timer_update(ec->timer_source, 10);
	ec->repaint_on_timeout = 1;

	animate_overlay(ec);
}

static void
schedule_repaint(struct egl_compositor *ec)
{
	struct wl_event_loop *loop;

	ec->repaint_needed = 1;
	if (!ec->repaint_on_timeout) {
		loop = wl_display_get_event_loop(ec->wl_display);
		wl_event_loop_add_idle(loop, repaint, ec);
	}
}
				   
static void
surface_destroy(struct wl_client *client,
		struct wl_surface *surface)
{
	struct egl_surface *es = (struct egl_surface *) surface;
	struct egl_compositor *ec = es->compositor;

	wl_list_remove(&es->link);
	egl_surface_destroy(es, ec);

	schedule_repaint(ec);
}

static void
surface_attach(struct wl_client *client,
	       struct wl_surface *surface, uint32_t name, 
	       uint32_t width, uint32_t height, uint32_t stride)
{
	struct egl_surface *es = (struct egl_surface *) surface;
	struct egl_compositor *ec = es->compositor;

	if (es->surface != EGL_NO_SURFACE)
		eglDestroySurface(ec->display, es->surface);

	es->width = width;
	es->height = height;
	es->surface = eglCreateSurfaceForName(ec->display, ec->config,
					      name, width, height, stride, NULL);

	glBindTexture(GL_TEXTURE_2D, es->texture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	eglBindTexImage(ec->display, es->surface, GL_TEXTURE_2D);
}

static void
surface_map(struct wl_client *client,
	    struct wl_surface *surface,
	    int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct egl_surface *es = (struct egl_surface *) surface;

	es->map.x = x;
	es->map.y = y;
	es->map.width = width;
	es->map.height = height;
}

static void
surface_copy(struct wl_client *client,
	     struct wl_surface *surface,
	     int32_t dst_x, int32_t dst_y,
	     uint32_t name, uint32_t stride,
	     int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct egl_surface *es = (struct egl_surface *) surface;
	struct egl_compositor *ec = es->compositor;
	EGLSurface src;

	/* FIXME: glCopyPixels should work, but then we'll have to
	 * call eglMakeCurrent to set up the src and dest surfaces
	 * first.  This seems cheaper, but maybe there's a better way
	 * to accomplish this. */

	src = eglCreateSurfaceForName(ec->display, ec->config,
				      name, x + width, y + height, stride, NULL);

	eglCopyNativeBuffers(ec->display, es->surface, GL_FRONT_LEFT, dst_x, dst_y,
			     src, GL_FRONT_LEFT, x, y, width, height);
	eglDestroySurface(ec->display, src);
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
	surface_copy,
	surface_damage
};

static void
compositor_create_surface(struct wl_client *client,
			  struct wl_compositor *compositor, uint32_t id)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;
	struct egl_surface *es;

	es = malloc(sizeof *es);
	if (es == NULL)
		/* FIXME: Send OOM event. */
		return;

	es->compositor = ec;
	es->surface = EGL_NO_SURFACE;
	wl_list_insert(ec->surface_list.prev, &es->link);
	glGenTextures(1, &es->texture);
	wl_client_add_surface(client, &es->base,
			      &surface_interface, id);
}

static void
compositor_commit(struct wl_client *client,
		  struct wl_compositor *compositor, uint32_t key)
{
	struct egl_compositor *ec = (struct egl_compositor *) compositor;

	schedule_repaint(ec);
	wl_client_send_acknowledge(client, compositor, key, ec->current_frame);
}

const static struct wl_compositor_interface compositor_interface = {
	compositor_create_surface,
	compositor_commit
};

static struct egl_surface *
pick_surface(struct egl_input_device *device)
{
	struct egl_compositor *ec = device->ec;
	struct egl_surface *es;

	if (device->grab > 0)
		return device->grab_surface;

	es = container_of(ec->surface_list.prev,
			  struct egl_surface, link);
	while (&es->link != &ec->surface_list) {
		if (es->map.x <= device->x &&
		    device->x < es->map.x + es->map.width &&
		    es->map.y <= device->y &&
		    device->y < es->map.y + es->map.height)
			return es;

		es = container_of(es->link.prev,
				  struct egl_surface, link);
	}

	return NULL;
}

void
notify_motion(struct egl_input_device *device, int x, int y)
{
	struct egl_surface *es;
	const int hotspot_x = 16, hotspot_y = 16;
	int32_t sx, sy;

	es = pick_surface(device);

	if (es) {
		sx = (x - es->map.x) * es->width / es->map.width;
		sy = (y - es->map.y) * es->height / es->map.height;
		wl_surface_post_event(&es->base, &device->base,
				      WL_INPUT_MOTION, x, y, sx, sy);
	}

	device->x = x;
	device->y = y;
	device->pointer_surface->map.x = x - hotspot_x;
	device->pointer_surface->map.y = y - hotspot_y;

	schedule_repaint(device->ec);
}

void
notify_button(struct egl_input_device *device,
	      int32_t button, int32_t state)
{
	struct egl_surface *es;
	int32_t sx, sy;

	es = pick_surface(device);
	if (es) {
		wl_list_remove(&es->link);
		wl_list_insert(device->ec->surface_list.prev, &es->link);

		if (state) {
			/* FIXME: We need callbacks when the surfaces
			 * we reference here go away. */
			device->grab++;
			device->grab_surface = es;
			device->focus_surface = es;
		} else {
			device->grab--;
		}

		sx = (device->x - es->map.x) * es->width / es->map.width;
		sy = (device->y - es->map.y) * es->height / es->map.height;

		/* FIXME: Swallow click on raise? */
		wl_surface_post_event(&es->base, &device->base,
				      WL_INPUT_BUTTON, button, state,
				      device->x, device->y, sx, sy);

		schedule_repaint(device->ec);
	}
}

void
notify_key(struct egl_input_device *device,
	   uint32_t key, uint32_t state)
{
	struct egl_compositor *ec = device->ec;

	if (key == KEY_ESC && state == 1) {
		if (ec->overlay_target == ec->height)
			ec->overlay_target -= 200;
		else
			ec->overlay_target += 200;
		schedule_repaint(ec);
	} else if (!wl_list_empty(&ec->surface_list)) {
		if (device->focus_surface != NULL)
			wl_surface_post_event(&device->focus_surface->base,
					      &device->base, 
					      WL_INPUT_KEY, key, state);
	}
}

struct evdev_input_device *
evdev_input_device_create(struct egl_input_device *device,
			  struct wl_display *display, const char *path);

static void
create_input_device(struct egl_compositor *ec, const char *glob)
{
	struct egl_input_device *device;
	struct dirent *de;
	char path[PATH_MAX];
	const char *by_path_dir = "/dev/input/by-path";
	DIR *dir;

	device = malloc(sizeof *device);
	if (device == NULL)
		return;

	memset(device, 0, sizeof *device);
	device->base.interface = wl_input_device_get_interface();
	wl_display_add_object(ec->wl_display, &device->base);
	device->x = 100;
	device->y = 100;
	device->pointer_surface = pointer_create(device->x, device->y, 64, 64);
	device->ec = ec;

	dir = opendir(by_path_dir);
	if (dir == NULL) {
		fprintf(stderr, "couldn't read dir %s\n", by_path_dir);
		return;
	}

	while (de = readdir(dir), de != NULL) {
		if (fnmatch(glob, de->d_name, 0))
			continue;

		snprintf(path, sizeof path, "%s/%s", by_path_dir, de->d_name);
		evdev_input_device_create(device, ec->wl_display, path);
	}
	closedir(dir);

	wl_list_insert(ec->input_device_list.prev, &device->link);
}

void
egl_device_get_position(struct egl_input_device *device, int32_t *x, int32_t *y)
{
	*x = device->x;
	*y = device->y;
}

static uint32_t
create_frontbuffer(int fd, int *width, int *height, int *stride)
{
	drmModeConnector *connector;
	drmModeRes *resources;
	drmModeEncoder *encoder;
	struct drm_mode_modeinfo *mode;
	struct drm_i915_gem_create create;
	struct drm_gem_flink flink;
	unsigned int fb_id;
	int i, ret;

	resources = drmModeGetResources(fd);
	if (!resources) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return 0;
	}

	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(fd, resources->connectors[i]);
		if (connector == NULL)
			continue;

		if (connector->connection == DRM_MODE_CONNECTED &&
		    connector->count_modes > 0)
			break;

		drmModeFreeConnector(connector);
	}

	if (i == resources->count_connectors) {
		fprintf(stderr, "No currently active connector found.\n");
		return -1;
	}

	mode = &connector->modes[0];

	for (i = 0; i < resources->count_encoders; i++) {
		encoder = drmModeGetEncoder(fd, resources->encoders[i]);

		if (encoder == NULL)
			continue;

		if (encoder->encoder_id == connector->encoder_id)
			break;

		drmModeFreeEncoder(encoder);
	}

	/* Mode size at 32 bpp */
	create.size = mode->hdisplay * mode->vdisplay * 4;
	if (ioctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create) != 0) {
		fprintf(stderr, "gem create failed: %m\n");
		return 0;
	}

	ret = drmModeAddFB(fd, mode->hdisplay, mode->vdisplay,
			   32, 32, mode->hdisplay * 4, create.handle, &fb_id);
	if (ret) {
		fprintf(stderr, "failed to add fb: %m\n");
		return 0;
	}

	ret = drmModeSetCrtc(fd, encoder->crtc_id, fb_id, 0, 0,
			     &connector->connector_id, 1, mode);
	if (ret) {
		fprintf(stderr, "failed to set mode: %m\n");
		return 0;
	}

	flink.handle = create.handle;
	if (ioctl(fd, DRM_IOCTL_GEM_FLINK, &flink) != 0) {
		fprintf(stderr, "gem flink failed: %m\n");
		return 0;
	}

	*width = mode->hdisplay;
	*height = mode->vdisplay;
	*stride = mode->hdisplay * 4;

	return flink.name;
}

static int
pick_config(struct egl_compositor *ec)
{
	EGLConfig configs[100];
	EGLint value, count;
	int i;

	if (!eglGetConfigs(ec->display, configs, ARRAY_LENGTH(configs), &count)) {
		fprintf(stderr, "failed to get configs\n");
		return -1;
	}

	ec->config = EGL_NO_CONFIG;
	for (i = 0; i < count; i++) {
		eglGetConfigAttrib(ec->display,
				   configs[i],
				   EGL_DEPTH_SIZE,
				   &value);
		if (value > 0) {
			fprintf(stderr, "config %d has depth size %d\n", i, value);
			continue;
		}

		eglGetConfigAttrib(ec->display,
				   configs[i],
				   EGL_STENCIL_SIZE,
				   &value);
		if (value > 0) {
			fprintf(stderr, "config %d has stencil size %d\n", i, value);
			continue;
		}

		eglGetConfigAttrib(ec->display,
				   configs[i],
				   EGL_CONFIG_CAVEAT,
				   &value);
		if (value != EGL_NONE) {
			fprintf(stderr, "config %d has caveat %d\n", i, value);
			continue;
		}

		ec->config = configs[i];
		break;
	}

	if (ec->config == EGL_NO_CONFIG) {
		fprintf(stderr, "found no config without depth or stencil buffers\n");
		return -1;
	}

	return 0;
}

static const char gem_device[] = "/dev/dri/card0";

static const char *macbook_air_default_input_device[] = {
	"pci-0000:00:1d.2-usb-0:2:1*event*",
	NULL
};

static const char *option_background = "background.jpg";
static const char **option_input_devices = macbook_air_default_input_device;

static const GOptionEntry option_entries[] = {
	{ "background", 'b', 0, G_OPTION_ARG_STRING,
	  &option_background, "Background image" },
	{ "input-device", 'i', 0, G_OPTION_ARG_STRING_ARRAY, 
	  &option_input_devices, "Input device glob" },
	{ NULL }
};

static struct egl_compositor *
egl_compositor_create(struct wl_display *display)
{
	EGLint major, minor;
	struct egl_compositor *ec;
	struct screenshooter *shooter;
	uint32_t fb_name;
	int i, stride;
	struct wl_event_loop *loop;
	const static EGLint attribs[] =
		{ EGL_RENDER_BUFFER, EGL_BACK_BUFFER, EGL_NONE };

	ec = malloc(sizeof *ec);
	if (ec == NULL)
		return NULL;

	ec->wl_display = display;

	ec->display = eglCreateDisplayNative(gem_device, "i965");
	if (ec->display == NULL) {
		fprintf(stderr, "failed to create display\n");
		return NULL;
	}

	if (!eglInitialize(ec->display, &major, &minor)) {
		fprintf(stderr, "failed to initialize display\n");
		return NULL;
	}

	if (pick_config(ec))
		return NULL;
 
	fb_name = create_frontbuffer(eglGetDisplayFD(ec->display),
				     &ec->width, &ec->height, &stride);
	ec->surface = eglCreateSurfaceForName(ec->display, ec->config,
					      fb_name, ec->width, ec->height, stride, attribs);
	if (ec->surface == NULL) {
		fprintf(stderr, "failed to create surface\n");
		return NULL;
	}

	ec->context = eglCreateContext(ec->display, ec->config, NULL, NULL);
	if (ec->context == NULL) {
		fprintf(stderr, "failed to create context\n");
		return NULL;
	}

	if (!eglMakeCurrent(ec->display, ec->surface, ec->surface, ec->context)) {
		fprintf(stderr, "failed to make context current\n");
		return NULL;
	}

	glViewport(0, 0, ec->width, ec->height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, ec->width, ec->height, 0, 0, 1000.0);
	glMatrixMode(GL_MODELVIEW);
	glClearColor(0, 0, 0.2, 1);

	wl_display_set_compositor(display, &ec->base, &compositor_interface); 

	wl_list_init(&ec->input_device_list);
	for (i = 0; option_input_devices[i]; i++)
		create_input_device(ec, option_input_devices[i]);

	wl_list_init(&ec->surface_list);
	ec->background = background_create(option_background,
					   ec->width, ec->height);
	ec->overlay = overlay_create(0, ec->height, ec->width, 200);
	ec->overlay_y = ec->height;
	ec->overlay_target = ec->height;
	ec->overlay_previous = ec->height;

	ec->gem_fd = open(gem_device, O_RDWR);
	if (ec->gem_fd < 0) {
		fprintf(stderr, "failed to open drm device\n");
		return NULL;
	}

	shooter = screenshooter_create(ec);
	wl_display_add_object(display, &shooter->base);
	wl_display_add_global(display, &shooter->base);

	loop = wl_display_get_event_loop(ec->wl_display);
	ec->timer_source = wl_event_loop_add_timer(loop, repaint, ec);
	ec->repaint_needed = 0;
	ec->repaint_on_timeout = 0;

	schedule_repaint(ec);

	return ec;
}

/* The plan here is to generate a random anonymous socket name and
 * advertise that through a service on the session dbus.
 */
static const char socket_name[] = "\0wayland";

int main(int argc, char *argv[])
{
	struct wl_display *display;
	struct egl_compositor *ec;
	GError *error = NULL;
	GOptionContext *context;

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, option_entries, "Wayland");
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		exit(EXIT_FAILURE);
	}

	display = wl_display_create();

	ec = egl_compositor_create(display);
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
