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

#define _GNU_SOURCE

#include "../config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include <cairo.h>
#include <glib.h>
#include <sys/mman.h>
#include <sys/epoll.h>

#include <pixman.h>

#include <wayland-egl.h>

#ifdef USE_CAIRO_GLESV2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include <GL/gl.h>
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifdef HAVE_CAIRO_EGL
#include <cairo-gl.h>
#endif

#include <xkbcommon/xkbcommon.h>
#include <X11/X.h>

#include <linux/input.h>
#include <wayland-client.h>
#include "cairo-util.h"

#include "window.h"

struct display {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_shell *shell;
	struct wl_shm *shm;
	struct wl_data_device_manager *data_device_manager;
	EGLDisplay dpy;
	EGLConfig argb_config;
	EGLContext argb_ctx;
	cairo_device_t *argb_device;
	uint32_t serial;

	int display_fd;
	uint32_t mask;
	struct task display_task;

	int epoll_fd;
	struct wl_list deferred_list;

	int running;

	struct wl_list window_list;
	struct wl_list input_list;
	struct wl_list output_list;
	cairo_surface_t *active_frame, *inactive_frame, *shadow;
	int frame_radius;
	struct xkb_desc *xkb;
	cairo_surface_t **pointer_surfaces;

	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_target_texture_2d;
	PFNEGLCREATEIMAGEKHRPROC create_image;
	PFNEGLDESTROYIMAGEKHRPROC destroy_image;

	display_output_handler_t output_configure_handler;

	void *user_data;
};

enum {
	TYPE_TOPLEVEL,
	TYPE_FULLSCREEN,
	TYPE_MAXIMIZED,
	TYPE_TRANSIENT,
	TYPE_MENU,
	TYPE_CUSTOM
};
       
struct window {
	struct display *display;
	struct window *parent;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	struct wl_region *input_region;
	struct wl_region *opaque_region;
	char *title;
	struct rectangle allocation, saved_allocation, server_allocation;
	struct rectangle pending_allocation;
	int x, y;
	int resize_edges;
	int redraw_scheduled;
	int redraw_needed;
	struct task redraw_task;
	int resize_needed;
	int type;
	int transparent;
	struct input *keyboard_device;
	enum window_buffer_type buffer_type;

	cairo_surface_t *cairo_surface;

	struct wl_shm_pool *pool;
	size_t pool_size;
	void *pool_data;

	window_key_handler_t key_handler;
	window_keyboard_focus_handler_t keyboard_focus_handler;
	window_data_handler_t data_handler;
	window_drop_handler_t drop_handler;
	window_close_handler_t close_handler;

	struct frame *frame;
	struct widget *widget;

	void *user_data;
	struct wl_list link;
};

struct widget {
	struct window *window;
	struct wl_list child_list;
	struct wl_list link;
	struct rectangle allocation;
	widget_resize_handler_t resize_handler;
	widget_redraw_handler_t redraw_handler;
	widget_enter_handler_t enter_handler;
	widget_leave_handler_t leave_handler;
	widget_motion_handler_t motion_handler;
	widget_button_handler_t button_handler;
	void *user_data;
	int opaque;
};

struct input {
	struct display *display;
	struct wl_input_device *input_device;
	struct window *pointer_focus;
	struct window *keyboard_focus;
	int current_pointer_image;
	uint32_t modifiers;
	uint32_t pointer_enter_serial;
	int32_t sx, sy;
	struct wl_list link;

	struct widget *focus_widget;
	struct widget *grab;
	uint32_t grab_button;

	struct wl_data_device *data_device;
	struct data_offer *drag_offer;
	struct data_offer *selection_offer;
};

struct output {
	struct display *display;
	struct wl_output *output;
	struct rectangle allocation;
	struct wl_list link;

	display_output_handler_t destroy_handler;
	void *user_data;
};

struct frame {
	struct widget *widget;
	struct widget *child;
	int margin;
	int width;
	int titlebar_height;
};

struct menu {
	struct window *window;
	struct widget *widget;
	struct input *input;
	const char **entries;
	uint32_t time;
	int current;
	int count;
	menu_func_t func;
};

enum {
	POINTER_DEFAULT = 100,
	POINTER_UNSET
};

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

const char *option_xkb_layout = "us";
const char *option_xkb_variant = "";
const char *option_xkb_options = "";

static const struct weston_option xkb_options[] = {
	{ WESTON_OPTION_STRING, "xkb-layout", 0, &option_xkb_layout },
	{ WESTON_OPTION_STRING, "xkb-variant", 0, &option_xkb_variant },
	{ WESTON_OPTION_STRING, "xkb-options", 0, &option_xkb_options },
};

static const cairo_user_data_key_t surface_data_key;
struct surface_data {
	struct wl_buffer *buffer;
};

#define MULT(_d,c,a,t) \
	do { t = c * a + 0x7f; _d = ((t >> 8) + t) >> 8; } while (0)

#ifdef HAVE_CAIRO_EGL

struct egl_window_surface_data {
	struct display *display;
	struct wl_surface *surface;
	struct wl_egl_window *window;
	EGLSurface surf;
};

static void
egl_window_surface_data_destroy(void *p)
{
	struct egl_window_surface_data *data = p;
	struct display *d = data->display;

	eglDestroySurface(d->dpy, data->surf);
	wl_egl_window_destroy(data->window);
	data->surface = NULL;

	free(p);
}

static cairo_surface_t *
display_create_egl_window_surface(struct display *display,
				  struct wl_surface *surface,
				  uint32_t flags,
				  struct rectangle *rectangle)
{
	cairo_surface_t *cairo_surface;
	struct egl_window_surface_data *data;
	EGLConfig config;
	cairo_device_t *device;

	data = malloc(sizeof *data);
	if (data == NULL)
		return NULL;

	data->display = display;
	data->surface = surface;

	config = display->argb_config;
	device = display->argb_device;

	data->window = wl_egl_window_create(surface,
					    rectangle->width,
					    rectangle->height);

	data->surf = eglCreateWindowSurface(display->dpy, config,
					    data->window, NULL);

	cairo_surface = cairo_gl_surface_create_for_egl(device,
							data->surf,
							rectangle->width,
							rectangle->height);

	cairo_surface_set_user_data(cairo_surface, &surface_data_key,
				    data, egl_window_surface_data_destroy);

	return cairo_surface;
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
	if (data->map)
		munmap(data->map, data->length);
}

static struct wl_shm_pool *
make_shm_pool(struct display *display, int size, void **data)
{
	char filename[] = "/tmp/wayland-shm-XXXXXX";
	struct wl_shm_pool *pool;
	int fd;

	fd = mkstemp(filename);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %m\n", filename);
		return NULL;
	}
	if (ftruncate(fd, size) < 0) {
		fprintf(stderr, "ftruncate failed: %m\n");
		close(fd);
		return NULL;
	}

	*data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	unlink(filename);

	if (*data == MAP_FAILED) {
		fprintf(stderr, "mmap failed: %m\n");
		close(fd);
		return NULL;
	}

	pool = wl_shm_create_pool(display->shm, fd, size);

	close(fd);

	return pool;
}

static cairo_surface_t *
display_create_shm_surface(struct display *display,
			   struct rectangle *rectangle, uint32_t flags,
			   struct window *window)
{
	struct shm_surface_data *data;
	struct wl_shm_pool *pool = NULL;
	uint32_t format;
	cairo_surface_t *surface;
	void *map;
	int stride;

	data = malloc(sizeof *data);
	if (data == NULL)
		return NULL;

	stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32,
						rectangle->width);
	data->length = stride * rectangle->height;
	if (window && window->pool && data->length < window->pool_size) {
		pool = window->pool;
		map = window->pool_data;
		data->map = NULL;
	} else {
		pool = make_shm_pool(display, data->length, &map);
		data->map = map;
	}

	surface = cairo_image_surface_create_for_data (map,
						       CAIRO_FORMAT_ARGB32,
						       rectangle->width,
						       rectangle->height,
						       stride);

	cairo_surface_set_user_data (surface, &surface_data_key,
				     data, shm_surface_data_destroy);

	if (flags & SURFACE_OPAQUE)
		format = WL_SHM_FORMAT_XRGB8888;
	else
		format = WL_SHM_FORMAT_ARGB8888;

	data->data.buffer = wl_shm_pool_create_buffer(pool, 0,
						      rectangle->width,
						      rectangle->height,
						      stride, format);

	if (data->map)
		wl_shm_pool_destroy(pool);

	return surface;
}

static cairo_surface_t *
display_create_shm_surface_from_file(struct display *display,
				     const char *filename,
				     struct rectangle *rect)
{
	cairo_surface_t *surface;
	pixman_image_t *image;
	void *dest;
	int size;

	image = load_image(filename);
	if (image == NULL)
		return NULL;

	surface = display_create_shm_surface(display, rect, 0, NULL);
	if (surface == NULL) {
		pixman_image_unref(image);
		return NULL;
	}

	size = pixman_image_get_stride(image) * pixman_image_get_height(image);
	dest = cairo_image_surface_get_data(surface);
	memcpy(dest, pixman_image_get_data(image), size);

	pixman_image_unref(image);

	return surface;
}

static int
check_size(struct rectangle *rect)
{
	if (rect->width && rect->height)
		return 0;

	fprintf(stderr, "tried to create surface of "
		"width: %d, height: %d\n", rect->width, rect->height);
	return -1;
}

cairo_surface_t *
display_create_surface(struct display *display,
		       struct wl_surface *surface,
		       struct rectangle *rectangle,
		       uint32_t flags)
{
	if (check_size(rectangle) < 0)
		return NULL;
#ifdef HAVE_CAIRO_EGL
	if (display->dpy)
		return display_create_egl_window_surface(display,
							 surface,
							 flags,
							 rectangle);
#endif
	return display_create_shm_surface(display, rectangle, flags, NULL);
}

static cairo_surface_t *
display_create_surface_from_file(struct display *display,
				 const char *filename,
				 struct rectangle *rectangle)
{
	if (check_size(rectangle) < 0)
		return NULL;

	return display_create_shm_surface_from_file(display, filename, rectangle);
}
 static const struct {
	const char *filename;
	int hotspot_x, hotspot_y;
} pointer_images[] = {
	{ DATADIR "/weston/bottom_left_corner.png",	 6, 30 },
	{ DATADIR "/weston/bottom_right_corner.png",	28, 28 },
	{ DATADIR "/weston/bottom_side.png",		16, 20 },
	{ DATADIR "/weston/grabbing.png",		20, 17 },
	{ DATADIR "/weston/left_ptr.png",		10,  5 },
	{ DATADIR "/weston/left_side.png",		10, 20 },
	{ DATADIR "/weston/right_side.png",		30, 19 },
	{ DATADIR "/weston/top_left_corner.png",	 8,  8 },
	{ DATADIR "/weston/top_right_corner.png",	26,  8 },
	{ DATADIR "/weston/top_side.png",		18,  8 },
	{ DATADIR "/weston/xterm.png",			15, 15 },
	{ DATADIR "/weston/hand1.png",			18, 11 }
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
		if (!display->pointer_surfaces[i]) {
			fprintf(stderr, "Error loading pointer image: %s\n",
				pointer_images[i].filename);
		}
	}

}

static void
destroy_pointer_surfaces(struct display *display)
{
	int i, count;

	count = ARRAY_LENGTH(pointer_images);
	for (i = 0; i < count; ++i) {
		if (display->pointer_surfaces[i])
			cairo_surface_destroy(display->pointer_surfaces[i]);
	}
	free(display->pointer_surfaces);
}

cairo_surface_t *
display_get_pointer_surface(struct display *display, int pointer,
			    int *width, int *height,
			    int *hotspot_x, int *hotspot_y)
{
	cairo_surface_t *surface;

	surface = display->pointer_surfaces[pointer];
#if HAVE_CAIRO_EGL
	*width = cairo_gl_surface_get_width(surface);
	*height = cairo_gl_surface_get_height(surface);
#else
	*width = cairo_image_surface_get_width(surface);
	*height = cairo_image_surface_get_height(surface);
#endif
	*hotspot_x = pointer_images[pointer].hotspot_x;
	*hotspot_y = pointer_images[pointer].hotspot_y;

	return cairo_surface_reference(surface);
}

static void
window_get_resize_dx_dy(struct window *window, int *x, int *y)
{
	if (window->resize_edges & WINDOW_RESIZING_LEFT)
		*x = window->server_allocation.width - window->allocation.width;
	else
		*x = 0;

	if (window->resize_edges & WINDOW_RESIZING_TOP)
		*y = window->server_allocation.height -
			window->allocation.height;
	else
		*y = 0;

	window->resize_edges = 0;
}

static void
window_attach_surface(struct window *window)
{
	struct display *display = window->display;
	struct wl_buffer *buffer;
#ifdef HAVE_CAIRO_EGL
	struct egl_window_surface_data *data;
#endif
	int32_t x, y;

	switch (window->buffer_type) {
#ifdef HAVE_CAIRO_EGL
	case WINDOW_BUFFER_TYPE_EGL_WINDOW:
		data = cairo_surface_get_user_data(window->cairo_surface,
						   &surface_data_key);

		cairo_gl_surface_swapbuffers(window->cairo_surface);
		wl_egl_window_get_attached_size(data->window,
				&window->server_allocation.width,
				&window->server_allocation.height);
		break;
#endif
	case WINDOW_BUFFER_TYPE_SHM:
		buffer =
			display_get_buffer_for_surface(display,
						       window->cairo_surface);

		window_get_resize_dx_dy(window, &x, &y);
		wl_surface_attach(window->surface, buffer, x, y);
		wl_surface_damage(window->surface, 0, 0,
				  window->allocation.width,
				  window->allocation.height);
		window->server_allocation = window->allocation;
		cairo_surface_destroy(window->cairo_surface);
		window->cairo_surface = NULL;
		break;
	default:
		return;
	}

	if (window->input_region) {
		wl_surface_set_input_region(window->surface,
					    window->input_region);
		wl_region_destroy(window->input_region);
		window->input_region = NULL;
	}

	if (window->opaque_region) {
		wl_surface_set_opaque_region(window->surface,
					     window->opaque_region);
		wl_region_destroy(window->opaque_region);
		window->opaque_region = NULL;
	}
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

#ifdef HAVE_CAIRO_EGL
static void
window_resize_cairo_window_surface(struct window *window)
{
	struct egl_window_surface_data *data;
	int x, y;

	data = cairo_surface_get_user_data(window->cairo_surface,
					   &surface_data_key);

	window_get_resize_dx_dy(window, &x, &y),
	wl_egl_window_resize(data->window,
			     window->allocation.width,
			     window->allocation.height,
			     x,y);

	cairo_gl_surface_set_size(window->cairo_surface,
				  window->allocation.width,
				  window->allocation.height);
}
#endif

struct display *
window_get_display(struct window *window)
{
	return window->display;
}

void
window_create_surface(struct window *window)
{
	cairo_surface_t *surface;
	uint32_t flags = 0;
	
	if (!window->transparent)
		flags = SURFACE_OPAQUE;
	
	switch (window->buffer_type) {
#ifdef HAVE_CAIRO_EGL
	case WINDOW_BUFFER_TYPE_EGL_WINDOW:
		if (window->cairo_surface) {
			window_resize_cairo_window_surface(window);
			return;
		}
		surface = display_create_surface(window->display,
						 window->surface,
						 &window->allocation, flags);
		break;
#endif
	case WINDOW_BUFFER_TYPE_SHM:
		surface = display_create_shm_surface(window->display,
						     &window->allocation,
						     flags, window);
		break;
        default:
		surface = NULL;
		break;
	}

	window_set_surface(window, surface);
	cairo_surface_destroy(surface);
}

static void frame_destroy(struct frame *frame);

void
window_destroy(struct window *window)
{
	struct display *display = window->display;
	struct input *input;

	if (window->redraw_scheduled)
		wl_list_remove(&window->redraw_task.link);

	wl_list_for_each(input, &display->input_list, link) {
		if (input->pointer_focus == window)
			input->pointer_focus = NULL;
		if (input->keyboard_focus == window)
			input->keyboard_focus = NULL;
		if (input->focus_widget &&
		    input->focus_widget->window == window)
			input->focus_widget = NULL;
	}

	if (window->input_region)
		wl_region_destroy(window->input_region);
	if (window->opaque_region)
		wl_region_destroy(window->opaque_region);

	if (window->frame)
		frame_destroy(window->frame);

	if (window->shell_surface)
		wl_shell_surface_destroy(window->shell_surface);
	wl_surface_destroy(window->surface);
	wl_list_remove(&window->link);

	if (window->cairo_surface != NULL)
		cairo_surface_destroy(window->cairo_surface);

	free(window->title);
	free(window);
}

static struct widget *
widget_find_widget(struct widget *widget, int32_t x, int32_t y)
{
	struct widget *child, *target;

	wl_list_for_each(child, &widget->child_list, link) {
		target = widget_find_widget(child, x, y);
		if (target)
			return target;
	}

	if (widget->allocation.x <= x &&
	    x < widget->allocation.x + widget->allocation.width &&
	    widget->allocation.y <= y &&
	    y < widget->allocation.y + widget->allocation.height) {
		return widget;
	}

	return NULL;
}

static struct widget *
widget_create(struct window *window, void *data)
{
	struct widget *widget;

	widget = malloc(sizeof *widget);
	memset(widget, 0, sizeof *widget);
	widget->window = window;
	widget->user_data = data;
	widget->allocation = window->allocation;
	wl_list_init(&widget->child_list);
	widget->opaque = 0;

	return widget;
}

struct widget *
window_add_widget(struct window *window, void *data)
{
	window->widget = widget_create(window, data);
	wl_list_init(&window->widget->link);

	return window->widget;
}

struct widget *
widget_add_widget(struct widget *parent, void *data)
{
	struct widget *widget;

	widget = widget_create(parent->window, data);
	wl_list_insert(parent->child_list.prev, &widget->link);

	return widget;
}

void
widget_destroy(struct widget *widget)
{
	struct display *display = widget->window->display;
	struct input *input;

	wl_list_for_each(input, &display->input_list, link) {
		if (input->focus_widget == widget)
			input->focus_widget = NULL;
	}

	wl_list_remove(&widget->link);
	free(widget);
}

void
widget_get_allocation(struct widget *widget, struct rectangle *allocation)
{
	*allocation = widget->allocation;
}

void
widget_set_size(struct widget *widget, int32_t width, int32_t height)
{
	widget->allocation.width = width;
	widget->allocation.height = height;
}

void
widget_set_allocation(struct widget *widget,
		      int32_t x, int32_t y, int32_t width, int32_t height)
{
	widget->allocation.x = x;
	widget->allocation.y = y;
	widget_set_size(widget, width, height);
}

void
widget_set_transparent(struct widget *widget, int transparent)
{
	widget->opaque = !transparent;
}

void *
widget_get_user_data(struct widget *widget)
{
	return widget->user_data;
}

void
widget_set_resize_handler(struct widget *widget,
			  widget_resize_handler_t handler)
{
	widget->resize_handler = handler;
}

void
widget_set_redraw_handler(struct widget *widget,
			  widget_redraw_handler_t handler)
{
	widget->redraw_handler = handler;
}

void
widget_set_enter_handler(struct widget *widget, widget_enter_handler_t handler)
{
	widget->enter_handler = handler;
}

void
widget_set_leave_handler(struct widget *widget, widget_leave_handler_t handler)
{
	widget->leave_handler = handler;
}

void
widget_set_motion_handler(struct widget *widget,
			  widget_motion_handler_t handler)
{
	widget->motion_handler = handler;
}

void
widget_set_button_handler(struct widget *widget,
			  widget_button_handler_t handler)
{
	widget->button_handler = handler;
}

void
widget_schedule_redraw(struct widget *widget)
{
	window_schedule_redraw(widget->window);
}

cairo_surface_t *
window_get_surface(struct window *window)
{
	return cairo_surface_reference(window->cairo_surface);
}

struct wl_surface *
window_get_wl_surface(struct window *window)
{
	return window->surface;
}

struct wl_shell_surface *
window_get_wl_shell_surface(struct window *window)
{
	return window->shell_surface;
}

static void
frame_resize_handler(struct widget *widget,
		     int32_t width, int32_t height, void *data)
{
	struct frame *frame = data;
	struct widget *child = frame->child;
	struct rectangle allocation;
	struct display *display = widget->window->display;
	int decoration_width, decoration_height;
	int opaque_margin;

	if (widget->window->type != TYPE_FULLSCREEN) {
		decoration_width = (frame->width + frame->margin) * 2;
		decoration_height = frame->width +
			frame->titlebar_height + frame->margin * 2;

		allocation.x = frame->width + frame->margin;
		allocation.y = frame->titlebar_height + frame->margin;
		allocation.width = width - decoration_width;
		allocation.height = height - decoration_height;

		widget->window->input_region =
			wl_compositor_create_region(display->compositor);
		wl_region_add(widget->window->input_region,
			      frame->margin, frame->margin,
			      width - 2 * frame->margin,
			      height - 2 * frame->margin);

		opaque_margin = frame->margin + display->frame_radius;
	} else {
		decoration_width = 0;
		decoration_height = 0;

		allocation.x = 0;
		allocation.y = 0;
		allocation.width = width;
		allocation.height = height;
		opaque_margin = 0;
	}

	widget_set_allocation(child, allocation.x, allocation.y,
			      allocation.width, allocation.height);

	if (child->resize_handler)
		child->resize_handler(child,
				      allocation.width,
				      allocation.height,
				      child->user_data);

	widget_set_allocation(widget, 0, 0,
			      child->allocation.width + decoration_width,
			      child->allocation.height + decoration_height);

	if (child->opaque) {
		widget->window->opaque_region =
			wl_compositor_create_region(display->compositor);
		wl_region_add(widget->window->opaque_region,
			      opaque_margin, opaque_margin,
			      widget->allocation.width - 2 * opaque_margin,
			      widget->allocation.height - 2 * opaque_margin);
	}
}

static void
frame_redraw_handler(struct widget *widget, void *data)
{
	struct frame *frame = data;
	cairo_t *cr;
	cairo_text_extents_t extents;
	cairo_surface_t *source;
	int x, y, width, height;
	struct window *window = widget->window;

	if (window->type == TYPE_FULLSCREEN)
		return;

	width = widget->allocation.width;
	height = widget->allocation.height;

	cr = cairo_create(window->cairo_surface);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);

	cairo_set_source_rgba(cr, 0, 0, 0, 0.45);
	tile_mask(cr, window->display->shadow,
		  2, 2, width + 8, height + 8,
		  64, 64);

	if (window->keyboard_device)
		source = window->display->active_frame;
	else
		source = window->display->inactive_frame;

	tile_source(cr, source,
		    frame->margin, frame->margin,
		    width - frame->margin * 2, height - frame->margin * 2,
		    frame->width, frame->titlebar_height);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_select_font_face(cr, "sans",
			       CAIRO_FONT_SLANT_NORMAL,
			       CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 14);
	cairo_text_extents(cr, window->title, &extents);
	x = (width - extents.width) / 2;
	y = frame->margin + 8 - extents.y_bearing;
	if (window->keyboard_device) {
		cairo_move_to(cr, x + 1, y  + 1);
		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_show_text(cr, window->title);
		cairo_move_to(cr, x, y);
		cairo_set_source_rgb(cr, 0, 0, 0);
		cairo_show_text(cr, window->title);
	} else {
		cairo_move_to(cr, x, y);
		cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
		cairo_show_text(cr, window->title);
	}

	cairo_destroy(cr);
}

static int
frame_get_pointer_location(struct frame *frame, int32_t x, int32_t y)
{
	struct widget *widget = frame->widget;
	int vlocation, hlocation, location;
	const int grip_size = 8;

	if (x < frame->margin)
		hlocation = WINDOW_EXTERIOR;
	else if (frame->margin <= x && x < frame->margin + grip_size)
		hlocation = WINDOW_RESIZING_LEFT;
	else if (x < widget->allocation.width - frame->margin - grip_size)
		hlocation = WINDOW_INTERIOR;
	else if (x < widget->allocation.width - frame->margin)
		hlocation = WINDOW_RESIZING_RIGHT;
	else
		hlocation = WINDOW_EXTERIOR;

	if (y < frame->margin)
		vlocation = WINDOW_EXTERIOR;
	else if (frame->margin <= y && y < frame->margin + grip_size)
		vlocation = WINDOW_RESIZING_TOP;
	else if (y < widget->allocation.height - frame->margin - grip_size)
		vlocation = WINDOW_INTERIOR;
	else if (y < widget->allocation.height - frame->margin)
		vlocation = WINDOW_RESIZING_BOTTOM;
	else
		vlocation = WINDOW_EXTERIOR;

	location = vlocation | hlocation;
	if (location & WINDOW_EXTERIOR)
		location = WINDOW_EXTERIOR;
	if (location == WINDOW_INTERIOR && y < frame->margin + 50)
		location = WINDOW_TITLEBAR;
	else if (location == WINDOW_INTERIOR)
		location = WINDOW_CLIENT_AREA;

	return location;
}

static int
frame_get_pointer_image_for_location(struct frame *frame, struct input *input)
{
	int location;

	location = frame_get_pointer_location(frame, input->sx, input->sy);
	switch (location) {
	case WINDOW_RESIZING_TOP:
		return POINTER_TOP;
	case WINDOW_RESIZING_BOTTOM:
		return POINTER_BOTTOM;
	case WINDOW_RESIZING_LEFT:
		return POINTER_LEFT;
	case WINDOW_RESIZING_RIGHT:
		return POINTER_RIGHT;
	case WINDOW_RESIZING_TOP_LEFT:
		return POINTER_TOP_LEFT;
	case WINDOW_RESIZING_TOP_RIGHT:
		return POINTER_TOP_RIGHT;
	case WINDOW_RESIZING_BOTTOM_LEFT:
		return POINTER_BOTTOM_LEFT;
	case WINDOW_RESIZING_BOTTOM_RIGHT:
		return POINTER_BOTTOM_RIGHT;
	case WINDOW_EXTERIOR:
	case WINDOW_TITLEBAR:
	default:
		return POINTER_LEFT_PTR;
	}
}

static void
frame_menu_func(struct window *window, int index, void *data)
{
	switch (index) {
	case 0: /* close */
		if (window->close_handler)
			window->close_handler(window->parent,
					      window->user_data);
		else
			display_exit(window->display);
		break;
	case 1: /* fullscreen */
		/* we don't have a way to get out of fullscreen for now */
		window_set_fullscreen(window, 1);
		break;
	case 2: /* rotate */
	case 3: /* scale */
		break;
	}
}

void
window_show_frame_menu(struct window *window,
		       struct input *input, uint32_t time)
{
	int32_t x, y;

	static const char *entries[] = {
		"Close", "Fullscreen", "Rotate", "Scale"
	};

	input_get_position(input, &x, &y);
	window_show_menu(window->display, input, time, window,
			 x - 10, y - 10, frame_menu_func, entries, 4);
}

static int
frame_enter_handler(struct widget *widget,
		    struct input *input, int32_t x, int32_t y, void *data)
{
	return frame_get_pointer_image_for_location(data, input);
}

static int
frame_motion_handler(struct widget *widget,
		     struct input *input, uint32_t time,
		     int32_t x, int32_t y, void *data)
{
	return frame_get_pointer_image_for_location(data, input);
}

static void
frame_button_handler(struct widget *widget,
		     struct input *input, uint32_t time,
		     int button, int state, void *data)

{
	struct frame *frame = data;
	struct window *window = widget->window;
	struct display *display = window->display;
	int location;

	location = frame_get_pointer_location(frame, input->sx, input->sy);

	if (window->display->shell && button == BTN_LEFT && state == 1) {
		switch (location) {
		case WINDOW_TITLEBAR:
			if (!window->shell_surface)
				break;
			input_set_pointer_image(input, time, POINTER_DRAGGING);
			input_ungrab(input);
			wl_shell_surface_move(window->shell_surface,
					      input_get_input_device(input),
					      display->serial);
			break;
		case WINDOW_RESIZING_TOP:
		case WINDOW_RESIZING_BOTTOM:
		case WINDOW_RESIZING_LEFT:
		case WINDOW_RESIZING_RIGHT:
		case WINDOW_RESIZING_TOP_LEFT:
		case WINDOW_RESIZING_TOP_RIGHT:
		case WINDOW_RESIZING_BOTTOM_LEFT:
		case WINDOW_RESIZING_BOTTOM_RIGHT:
			if (!window->shell_surface)
				break;
			input_ungrab(input);

			if (!display->dpy) {
				/* If we're using shm, allocate a big
				   pool to create buffers out of while
				   we resize.  We should probably base
				   this number on the size of the output. */
				window->pool_size = 6 * 1024 * 1024;
				window->pool = make_shm_pool(display,
							     window->pool_size,
							     &window->pool_data);
			}

			wl_shell_surface_resize(window->shell_surface,
						input_get_input_device(input),
						display->serial, location);
			break;
		}
	} else if (button == BTN_RIGHT && state == 1) {
		window_show_frame_menu(window, input, time);
	}
}

struct widget *
frame_create(struct window *window, void *data)
{
	struct frame *frame;

	frame = malloc(sizeof *frame);
	memset(frame, 0, sizeof *frame);

	frame->widget = window_add_widget(window, frame);
	frame->child = widget_add_widget(frame->widget, data);
	frame->margin = 32;
	frame->width = 4;
	frame->titlebar_height = 30
;
	widget_set_redraw_handler(frame->widget, frame_redraw_handler);
	widget_set_resize_handler(frame->widget, frame_resize_handler);
	widget_set_enter_handler(frame->widget, frame_enter_handler);
	widget_set_motion_handler(frame->widget, frame_motion_handler);
	widget_set_button_handler(frame->widget, frame_button_handler);

	window->frame = frame;

	return frame->child;
}

static void
frame_destroy(struct frame *frame)
{
	/* frame->child must be destroyed by the application */
	widget_destroy(frame->widget);
	free(frame);
}

static void
input_set_focus_widget(struct input *input, struct widget *focus,
		       int32_t x, int32_t y)
{
	struct widget *old, *widget;
	int pointer = POINTER_LEFT_PTR;

	if (focus == input->focus_widget)
		return;

	old = input->focus_widget;
	if (old) {
		widget = old;
		if (input->grab)
			widget = input->grab;
		if (widget->leave_handler)
			widget->leave_handler(old, input, widget->user_data);
		input->focus_widget = NULL;
	}

	if (focus) {
		widget = focus;
		if (input->grab)
			widget = input->grab;
		if (widget->enter_handler)
			pointer = widget->enter_handler(focus, input, x, y,
							widget->user_data);
		input->focus_widget = focus;

		input_set_pointer_image(input, input->pointer_enter_serial,
					pointer);
	}
}

static void
input_handle_motion(void *data, struct wl_input_device *input_device,
		    uint32_t time, int32_t sx, int32_t sy)
{
	struct input *input = data;
	struct window *window = input->pointer_focus;
	struct widget *widget;
	int pointer = POINTER_LEFT_PTR;

	input->sx = sx;
	input->sy = sy;

	if (!(input->grab && input->grab_button)) {
		widget = widget_find_widget(window->widget, sx, sy);
		input_set_focus_widget(input, widget, sx, sy);
	}

	if (input->grab)
		widget = input->grab;
	else
		widget = input->focus_widget;
	if (widget && widget->motion_handler)
		pointer = widget->motion_handler(input->focus_widget,
						 input, time, sx, sy,
						 widget->user_data);

	input_set_pointer_image(input, time, pointer);
}

void
input_grab(struct input *input, struct widget *widget, uint32_t button)
{
	input->grab = widget;
	input->grab_button = button;
}

void
input_ungrab(struct input *input)
{
	struct widget *widget;

	input->grab = NULL;
	if (input->pointer_focus) {
		widget = widget_find_widget(input->pointer_focus->widget,
					    input->sx, input->sy);
		input_set_focus_widget(input, widget, input->sx, input->sy);
	}
}

static void
input_handle_button(void *data,
		    struct wl_input_device *input_device, uint32_t serial,
		    uint32_t time, uint32_t button, uint32_t state)
{
	struct input *input = data;
	struct widget *widget;

	input->display->serial = serial;
	if (input->focus_widget && input->grab == NULL && state)
		input_grab(input, input->focus_widget, button);

	widget = input->grab;
	if (widget && widget->button_handler)
		(*widget->button_handler)(widget,
					  input, time,
					  button, state,
					  input->grab->user_data);

	if (input->grab && input->grab_button == button && !state)
		input_ungrab(input);
}

static void
input_handle_axis(void *data,
		    struct wl_input_device *input_device,
		    uint32_t time, uint32_t axis, int32_t value)
{
}

static void
input_handle_key(void *data, struct wl_input_device *input_device,
		 uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	struct input *input = data;
	struct window *window = input->keyboard_focus;
	struct display *d = input->display;
	uint32_t code, sym, level;

	input->display->serial = serial;
	code = key + 8;
	if (!window || window->keyboard_device != input)
		return;

	level = 0;
	if (input->modifiers & XKB_COMMON_SHIFT_MASK &&
	    XkbKeyGroupWidth(d->xkb, code, 0) > 1)
		level = 1;

	sym = XkbKeySymEntry(d->xkb, code, level, 0);

	if (state)
		input->modifiers |= d->xkb->map->modmap[code];
	else
		input->modifiers &= ~d->xkb->map->modmap[code];

	if (key == KEY_F5 && input->modifiers == Mod4Mask) {
		if (state)
			window_set_maximized(window,
					     window->type != TYPE_MAXIMIZED);
	} else if (window->key_handler) {
		(*window->key_handler)(window, input, time, key,
				       sym, state, window->user_data);
	}
}

static void
input_remove_pointer_focus(struct input *input)
{
	struct window *window = input->pointer_focus;

	if (!window)
		return;

	input_set_focus_widget(input, NULL, 0, 0);

	input->pointer_focus = NULL;
	input->current_pointer_image = POINTER_UNSET;
}

static void
input_handle_pointer_enter(void *data,
			   struct wl_input_device *input_device,
			   uint32_t serial, struct wl_surface *surface,
			   int32_t sx, int32_t sy)
{
	struct input *input = data;
	struct window *window;
	struct widget *widget;

	input->display->serial = serial;
	input->pointer_enter_serial = serial;
	input->pointer_focus = wl_surface_get_user_data(surface);
	window = input->pointer_focus;

	if (window->pool) {
		wl_shm_pool_destroy(window->pool);
		munmap(window->pool_data, window->pool_size);
		window->pool = NULL;
		/* Schedule a redraw to free the pool */
		window_schedule_redraw(window);
	}

	input->sx = sx;
	input->sy = sy;

	widget = widget_find_widget(window->widget, sx, sy);
	input_set_focus_widget(input, widget, sx, sy);
}

static void
input_handle_pointer_leave(void *data,
			   struct wl_input_device *input_device,
			   uint32_t serial, struct wl_surface *surface)
{
	struct input *input = data;

	input->display->serial = serial;
	input_remove_pointer_focus(input);
}

static void
input_remove_keyboard_focus(struct input *input)
{
	struct window *window = input->keyboard_focus;

	if (!window)
		return;

	window->keyboard_device = NULL;
	if (window->keyboard_focus_handler)
		(*window->keyboard_focus_handler)(window, NULL,
						  window->user_data);

	input->keyboard_focus = NULL;
}

static void
input_handle_keyboard_enter(void *data,
			    struct wl_input_device *input_device,
			    uint32_t serial,
			    struct wl_surface *surface,
			    struct wl_array *keys)
{
	struct input *input = data;
	struct window *window;
	struct display *d = input->display;
	uint32_t *k, *end;

	input->display->serial = serial;
	input->keyboard_focus = wl_surface_get_user_data(surface);

	end = keys->data + keys->size;
	input->modifiers = 0;
	for (k = keys->data; k < end; k++)
		input->modifiers |= d->xkb->map->modmap[*k];

	window = input->keyboard_focus;
	window->keyboard_device = input;
	if (window->keyboard_focus_handler)
		(*window->keyboard_focus_handler)(window,
						  window->keyboard_device,
						  window->user_data);
}

static void
input_handle_keyboard_leave(void *data,
			    struct wl_input_device *input_device,
			    uint32_t serial,
			    struct wl_surface *surface)
{
	struct input *input = data;

	input->display->serial = serial;
	input_remove_keyboard_focus(input);
}

static void
input_handle_touch_down(void *data,
			struct wl_input_device *wl_input_device,
			uint32_t serial, uint32_t time,
			struct wl_surface *surface,
			int32_t id, int32_t x, int32_t y)
{
}

static void
input_handle_touch_up(void *data,
		      struct wl_input_device *wl_input_device,
		      uint32_t serial, uint32_t time, int32_t id)
{
}

static void
input_handle_touch_motion(void *data,
			  struct wl_input_device *wl_input_device,
			  uint32_t time, int32_t id, int32_t x, int32_t y)
{
}

static void
input_handle_touch_frame(void *data,
			 struct wl_input_device *wl_input_device)
{
}

static void
input_handle_touch_cancel(void *data,
			  struct wl_input_device *wl_input_device)
{
}

static const struct wl_input_device_listener input_device_listener = {
	input_handle_motion,
	input_handle_button,
	input_handle_axis,
	input_handle_key,
	input_handle_pointer_enter,
	input_handle_pointer_leave,
	input_handle_keyboard_enter,
	input_handle_keyboard_leave,
	input_handle_touch_down,
	input_handle_touch_up,
	input_handle_touch_motion,
	input_handle_touch_frame,
	input_handle_touch_cancel,
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

uint32_t
input_get_modifiers(struct input *input)
{
	return input->modifiers;
}

struct widget *
input_get_focus_widget(struct input *input)
{
	return input->focus_widget;
}

struct data_offer {
	struct wl_data_offer *offer;
	struct input *input;
	struct wl_array types;
	int refcount;

	struct task io_task;
	int fd;
	data_func_t func;
	int32_t x, y;
	void *user_data;
};

static void
data_offer_offer(void *data, struct wl_data_offer *wl_data_offer, const char *type)
{
	struct data_offer *offer = data;
	char **p;

	p = wl_array_add(&offer->types, sizeof *p);
	*p = strdup(type);
}

static const struct wl_data_offer_listener data_offer_listener = {
	data_offer_offer,
};

static void
data_offer_destroy(struct data_offer *offer)
{
	char **p;

	offer->refcount--;
	if (offer->refcount == 0) {
		wl_data_offer_destroy(offer->offer);
		for (p = offer->types.data; *p; p++)
			free(*p);
		wl_array_release(&offer->types);
		free(offer);
	}
}

static void
data_device_data_offer(void *data,
		       struct wl_data_device *data_device, uint32_t id)
{
	struct data_offer *offer;

	offer = malloc(sizeof *offer);

	wl_array_init(&offer->types);
	offer->refcount = 1;
	offer->input = data;

	/* FIXME: Generate typesafe wrappers for this */
	offer->offer = (struct wl_data_offer *)
		wl_proxy_create_for_id((struct wl_proxy *) data_device,
				       id, &wl_data_offer_interface);

	wl_data_offer_add_listener(offer->offer,
				   &data_offer_listener, offer);
}

static void
data_device_enter(void *data, struct wl_data_device *data_device,
		  uint32_t serial, struct wl_surface *surface,
		  int32_t x, int32_t y, struct wl_data_offer *offer)
{
	struct input *input = data;
	struct window *window;
	char **p;

	input->pointer_enter_serial = serial;
	input->drag_offer = wl_data_offer_get_user_data(offer);
	window = wl_surface_get_user_data(surface);
	input->pointer_focus = window;

	p = wl_array_add(&input->drag_offer->types, sizeof *p);
	*p = NULL;

	window = input->pointer_focus;
	if (window->data_handler)
		window->data_handler(window, input, x, y,
				     input->drag_offer->types.data,
				     window->user_data);
}

static void
data_device_leave(void *data, struct wl_data_device *data_device)
{
	struct input *input = data;

	data_offer_destroy(input->drag_offer);
	input->drag_offer = NULL;
}

static void
data_device_motion(void *data, struct wl_data_device *data_device,
		   uint32_t time, int32_t x, int32_t y)
{
	struct input *input = data;
	struct window *window = input->pointer_focus;

	input->sx = x;
	input->sy = y;

	if (window->data_handler)
		window->data_handler(window, input, x, y,
				     input->drag_offer->types.data,
				     window->user_data);
}

static void
data_device_drop(void *data, struct wl_data_device *data_device)
{
	struct input *input = data;
	struct window *window = input->pointer_focus;

	if (window->drop_handler)
		window->drop_handler(window, input,
				     input->sx, input->sy, window->user_data);
}

static void
data_device_selection(void *data,
		      struct wl_data_device *wl_data_device,
		      struct wl_data_offer *offer)
{
	struct input *input = data;
	char **p;

	if (input->selection_offer)
		data_offer_destroy(input->selection_offer);

	if (offer) {
		input->selection_offer = wl_data_offer_get_user_data(offer);
		p = wl_array_add(&input->selection_offer->types, sizeof *p);
		*p = NULL;
	}
}

static const struct wl_data_device_listener data_device_listener = {
	data_device_data_offer,
	data_device_enter,
	data_device_leave,
	data_device_motion,
	data_device_drop,
	data_device_selection
};

void
input_set_pointer_image(struct input *input, uint32_t time, int pointer)
{
	struct display *display = input->display;
	struct wl_buffer *buffer;
	cairo_surface_t *surface;

	if (pointer == input->current_pointer_image)
		return;

	input->current_pointer_image = pointer;
	surface = display->pointer_surfaces[pointer];

	if (!surface)
		return;

	buffer = display_get_buffer_for_surface(display, surface);
	wl_input_device_attach(input->input_device, time, buffer,
			       pointer_images[pointer].hotspot_x,
			       pointer_images[pointer].hotspot_y);
}

struct wl_data_device *
input_get_data_device(struct input *input)
{
	return input->data_device;
}

void
input_set_selection(struct input *input,
		    struct wl_data_source *source, uint32_t time)
{
	wl_data_device_set_selection(input->data_device, source, time);
}

void
input_accept(struct input *input, const char *type)
{
	wl_data_offer_accept(input->drag_offer->offer,
			     input->pointer_enter_serial, type);
}

static void
offer_io_func(struct task *task, uint32_t events)
{
	struct data_offer *offer =
		container_of(task, struct data_offer, io_task);
	unsigned int len;
	char buffer[4096];

	len = read(offer->fd, buffer, sizeof buffer);
	offer->func(buffer, len,
		    offer->x, offer->y, offer->user_data);

	if (len == 0) {
		close(offer->fd);
		data_offer_destroy(offer);
	}
}

static void
data_offer_receive_data(struct data_offer *offer, const char *mime_type,
			data_func_t func, void *user_data)
{
	int p[2];

	if (pipe2(p, O_CLOEXEC) == -1)
		return;

	wl_data_offer_receive(offer->offer, mime_type, p[1]);
	close(p[1]);

	offer->io_task.run = offer_io_func;
	offer->fd = p[0];
	offer->func = func;
	offer->refcount++;
	offer->user_data = user_data;

	display_watch_fd(offer->input->display,
			 offer->fd, EPOLLIN, &offer->io_task);
}

void
input_receive_drag_data(struct input *input, const char *mime_type,
			data_func_t func, void *data)
{
	data_offer_receive_data(input->drag_offer, mime_type, func, data);
	input->drag_offer->x = input->sx;
	input->drag_offer->y = input->sy;
}

int
input_receive_selection_data(struct input *input, const char *mime_type,
			     data_func_t func, void *data)
{
	char **p;

	if (input->selection_offer == NULL)
		return -1;

	for (p = input->selection_offer->types.data; *p; p++)
		if (strcmp(mime_type, *p) == 0)
			break;

	if (*p == NULL)
		return -1;

	data_offer_receive_data(input->selection_offer,
				mime_type, func, data);
	return 0;
}

int
input_receive_selection_data_to_fd(struct input *input,
				   const char *mime_type, int fd)
{
	wl_data_offer_receive(input->selection_offer->offer, mime_type, fd);

	return 0;
}

void
window_move(struct window *window, struct input *input, uint32_t serial)
{
	if (!window->shell_surface)
		return;

	wl_shell_surface_move(window->shell_surface,
			      input->input_device, serial);
}

static void
idle_resize(struct window *window)
{
	struct widget *widget;

	window->resize_needed = 0;
	widget = window->widget;
	widget_set_allocation(widget,
			      window->pending_allocation.x,
			      window->pending_allocation.y,
			      window->pending_allocation.width,
			      window->pending_allocation.height);

	if (window->input_region) {
		wl_region_destroy(window->input_region);
		window->input_region = NULL;
	}

	if (window->opaque_region) {
		wl_region_destroy(window->opaque_region);
		window->opaque_region = NULL;
	}

	if (widget->resize_handler)
		widget->resize_handler(widget,
				       widget->allocation.width,
				       widget->allocation.height,
				       widget->user_data);

	if (window->allocation.width != widget->allocation.width ||
	    window->allocation.height != widget->allocation.height) {
		window->allocation = widget->allocation;
		window_schedule_redraw(window);
	}
}

void
window_schedule_resize(struct window *window, int width, int height)
{
	window->pending_allocation.x = 0;
	window->pending_allocation.y = 0;
	window->pending_allocation.width = width;
	window->pending_allocation.height = height;

	window->resize_needed = 1;
	window_schedule_redraw(window);
}

void
widget_schedule_resize(struct widget *widget, int32_t width, int32_t height)
{
	window_schedule_resize(widget->window, width, height);
}

static void
handle_ping(void *data, struct wl_shell_surface *shell_surface,
							uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
		 uint32_t edges, int32_t width, int32_t height)
{
	struct window *window = data;

	if (width <= 0 || height <= 0)
		return;

	window->resize_edges = edges;
	window_schedule_resize(window, width, height);
}

static void
menu_destroy(struct menu *menu)
{
	widget_destroy(menu->widget);
	window_destroy(menu->window);
	free(menu);
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
	struct window *window = data;
	struct menu *menu = window->widget->user_data;

	/* FIXME: Need more context in this event, at least the input
	 * device.  Or just use wl_callback.  And this really needs to
	 * be a window vfunc that the menu can set.  And we need the
	 * time. */

	menu->func(window->parent, menu->current, window->parent->user_data);
	input_ungrab(menu->input);
	menu_destroy(menu);
}

static const struct wl_shell_surface_listener shell_surface_listener = {
	handle_ping,
	handle_configure,
	handle_popup_done
};

void
window_get_allocation(struct window *window,
		      struct rectangle *allocation)
{
	*allocation = window->allocation;
}

static void
widget_redraw(struct widget *widget)
{
	struct widget *child;

	if (widget->redraw_handler)
		widget->redraw_handler(widget, widget->user_data);
	wl_list_for_each(child, &widget->child_list, link)
		widget_redraw(child);
}

static void
frame_callback(void *data, struct wl_callback *callback, uint32_t time)
{
	struct window *window = data;

	wl_callback_destroy(callback);
	window->redraw_scheduled = 0;
	if (window->redraw_needed)
		window_schedule_redraw(window);
}

static const struct wl_callback_listener listener = {
	frame_callback
};

static void
idle_redraw(struct task *task, uint32_t events)
{
	struct window *window = container_of(task, struct window, redraw_task);
	struct wl_callback *callback;

	if (window->resize_needed)
		idle_resize(window);

	window_create_surface(window);
	widget_redraw(window->widget);
	window_flush(window);
	window->redraw_needed = 0;
	wl_list_init(&window->redraw_task.link);

	callback = wl_surface_frame(window->surface);
	wl_callback_add_listener(callback, &listener, window);
}

void
window_schedule_redraw(struct window *window)
{
	window->redraw_needed = 1;
	if (!window->redraw_scheduled) {
		window->redraw_task.run = idle_redraw;
		display_defer(window->display, &window->redraw_task);
		window->redraw_scheduled = 1;
	}
}

void
window_set_custom(struct window *window)
{
	window->type = TYPE_CUSTOM;
}

void
window_set_fullscreen(struct window *window, int fullscreen)
{
	if (!window->display->shell)
		return;

	if ((window->type == TYPE_FULLSCREEN) == fullscreen)
		return;

	if (fullscreen) {
		window->type = TYPE_FULLSCREEN;
		window->saved_allocation = window->allocation;
		wl_shell_surface_set_fullscreen(window->shell_surface,
						WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
						0, NULL);
	} else {
		window->type = TYPE_TOPLEVEL;
		wl_shell_surface_set_toplevel(window->shell_surface);
		window_schedule_resize(window,
				       window->saved_allocation.width,
				       window->saved_allocation.height);
	}
}

void
window_set_maximized(struct window *window, int maximized)
{
	if (!window->display->shell)
		return;

	if ((window->type == TYPE_MAXIMIZED) == maximized)
		return;

	if (window->type == TYPE_TOPLEVEL) {
		window->saved_allocation = window->allocation;
		wl_shell_surface_set_maximized(window->shell_surface, NULL);
		window->type = TYPE_MAXIMIZED;
	} else {
		wl_shell_surface_set_toplevel(window->shell_surface);
		window->type = TYPE_TOPLEVEL;
		window_schedule_resize(window,
				       window->saved_allocation.width,
				       window->saved_allocation.height);
	}
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
window_set_key_handler(struct window *window,
		       window_key_handler_t handler)
{
	window->key_handler = handler;
}

void
window_set_keyboard_focus_handler(struct window *window,
				  window_keyboard_focus_handler_t handler)
{
	window->keyboard_focus_handler = handler;
}

void
window_set_data_handler(struct window *window, window_data_handler_t handler)
{
	window->data_handler = handler;
}

void
window_set_drop_handler(struct window *window, window_drop_handler_t handler)
{
	window->drop_handler = handler;
}

void
window_set_close_handler(struct window *window,
			 window_close_handler_t handler)
{
	window->close_handler = handler;
}

void
window_set_title(struct window *window, const char *title)
{
	free(window->title);
	window->title = strdup(title);
}

const char *
window_get_title(struct window *window)
{
	return window->title;
}

void
window_damage(struct window *window, int32_t x, int32_t y,
	      int32_t width, int32_t height)
{
	wl_surface_damage(window->surface, x, y, width, height);
}

static struct window *
window_create_internal(struct display *display, struct window *parent)
{
	struct window *window;

	window = malloc(sizeof *window);
	if (window == NULL)
		return NULL;

	memset(window, 0, sizeof *window);
	window->display = display;
	window->parent = parent;
	window->surface = wl_compositor_create_surface(display->compositor);
	if (display->shell) {
		window->shell_surface =
			wl_shell_get_shell_surface(display->shell,
						   window->surface);
	}
	window->allocation.x = 0;
	window->allocation.y = 0;
	window->allocation.width = 0;
	window->allocation.height = 0;
	window->saved_allocation = window->allocation;
	window->transparent = 1;
	window->type = TYPE_TOPLEVEL;
	window->input_region = NULL;
	window->opaque_region = NULL;

	if (display->dpy)
#ifdef HAVE_CAIRO_EGL
		window->buffer_type = WINDOW_BUFFER_TYPE_EGL_WINDOW;
#else
		window->buffer_type = WINDOW_BUFFER_TYPE_SHM;
#endif
	else
		window->buffer_type = WINDOW_BUFFER_TYPE_SHM;

	wl_surface_set_user_data(window->surface, window);
	wl_list_insert(display->window_list.prev, &window->link);
	wl_list_init(&window->redraw_task.link);

	if (window->shell_surface) {
		wl_shell_surface_set_user_data(window->shell_surface, window);
		wl_shell_surface_add_listener(window->shell_surface,
					      &shell_surface_listener, window);
	}

	return window;
}

struct window *
window_create(struct display *display)
{
	struct window *window;

	window = window_create_internal(display, NULL);
	if (!window)
		return NULL;

	window->type = TYPE_TOPLEVEL;
	if (display->shell)
		wl_shell_surface_set_toplevel(window->shell_surface);

	return window;
}

struct window *
window_create_transient(struct display *display, struct window *parent,
			int32_t x, int32_t y)
{
	struct window *window;

	window = window_create_internal(parent->display, parent);
	if (!window)
		return NULL;

	window->type = TYPE_TRANSIENT;
	window->x = x;
	window->y = y;

	if (display->shell)
		wl_shell_surface_set_transient(window->shell_surface,
					       window->parent->shell_surface,
					       window->x, window->y, 0);

	return window;
}

static void
menu_set_item(struct menu *menu, int sy)
{
	int next;

	next = (sy - 8) / 20;
	if (menu->current != next) {
		menu->current = next;
		widget_schedule_redraw(menu->widget);
	}
}

static int
menu_motion_handler(struct widget *widget,
		    struct input *input, uint32_t time,
		    int32_t x, int32_t y, void *data)
{
	struct menu *menu = data;

	if (widget == menu->widget)
		menu_set_item(data, y);

	return POINTER_LEFT_PTR;
}

static int
menu_enter_handler(struct widget *widget,
		   struct input *input, int32_t x, int32_t y, void *data)
{
	struct menu *menu = data;

	if (widget == menu->widget)
		menu_set_item(data, y);

	return POINTER_LEFT_PTR;
}

static void
menu_leave_handler(struct widget *widget, struct input *input, void *data)
{
	struct menu *menu = data;

	if (widget == menu->widget)
		menu_set_item(data, -200);
}

static void
menu_button_handler(struct widget *widget,
		    struct input *input, uint32_t time,
		    int button, int state, void *data)

{
	struct menu *menu = data;

	if (state == 0 && time - menu->time > 500) {
		/* Either relase after press-drag-release or
		 * click-motion-click. */
		menu->func(menu->window->parent, 
			   menu->current, menu->window->parent->user_data);
		input_ungrab(input);
		menu_destroy(menu);
	}
}

static void
menu_redraw_handler(struct widget *widget, void *data)
{
	cairo_t *cr;
	const int32_t r = 3, margin = 3;
	struct menu *menu = data;
	int32_t width, height, i;
	struct window *window = widget->window;

	cr = cairo_create(window->cairo_surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
	cairo_paint(cr);

	width = window->allocation.width;
	height = window->allocation.height;
	rounded_rect(cr, 0, 0, width, height, r);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.4, 0.8);
	cairo_fill(cr);

	for (i = 0; i < menu->count; i++) {
		if (i == menu->current) {
			cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
			cairo_rectangle(cr, margin, i * 20 + margin,
					width - 2 * margin, 20);
			cairo_fill(cr);
			cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
			cairo_move_to(cr, 10, i * 20 + 16);
			cairo_show_text(cr, menu->entries[i]);
		} else {
			cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
			cairo_move_to(cr, 10, i * 20 + 16);
			cairo_show_text(cr, menu->entries[i]);
		}
	}

	cairo_destroy(cr);
}

void
window_show_menu(struct display *display,
		 struct input *input, uint32_t time, struct window *parent,
		 int32_t x, int32_t y,
		 menu_func_t func, const char **entries, int count)
{
	struct window *window;
	struct menu *menu;
	const int32_t margin = 3;

	menu = malloc(sizeof *menu);
	if (!menu)
		return;

	window = window_create_internal(parent->display, parent);
	if (!window)
		return;

	menu->window = window;
	menu->widget = window_add_widget(menu->window, menu);
	menu->entries = entries;
	menu->count = count;
	menu->current = -1;
	menu->time = time;
	menu->func = func;
	menu->input = input;
	window->type = TYPE_MENU;
	window->x = x;
	window->y = y;

	input_ungrab(input);
	wl_shell_surface_set_popup(window->shell_surface, input->input_device,
				   display_get_serial(window->display),
				   window->parent->shell_surface,
				   window->x, window->y, 0);

	widget_set_redraw_handler(menu->widget, menu_redraw_handler);
	widget_set_enter_handler(menu->widget, menu_enter_handler);
	widget_set_leave_handler(menu->widget, menu_leave_handler);
	widget_set_motion_handler(menu->widget, menu_motion_handler);
	widget_set_button_handler(menu->widget, menu_button_handler);

	input_grab(input, menu->widget, 0);
	window_schedule_resize(window, 200, count * 20 + margin * 2);
}

void
window_set_buffer_type(struct window *window, enum window_buffer_type type)
{
	window->buffer_type = type;
}


static void
display_handle_geometry(void *data,
			struct wl_output *wl_output,
			int x, int y,
			int physical_width,
			int physical_height,
			int subpixel,
			const char *make,
			const char *model)
{
	struct output *output = data;

	output->allocation.x = x;
	output->allocation.y = y;
}

static void
display_handle_mode(void *data,
		    struct wl_output *wl_output,
		    uint32_t flags,
		    int width,
		    int height,
		    int refresh)
{
	struct output *output = data;
	struct display *display = output->display;

	if (flags & WL_OUTPUT_MODE_CURRENT) {
		output->allocation.width = width;
		output->allocation.height = height;
		if (display->output_configure_handler)
			(*display->output_configure_handler)(
						output, display->user_data);
	}
}

static const struct wl_output_listener output_listener = {
	display_handle_geometry,
	display_handle_mode
};

static void
display_add_output(struct display *d, uint32_t id)
{
	struct output *output;

	output = malloc(sizeof *output);
	if (output == NULL)
		return;

	memset(output, 0, sizeof *output);
	output->display = d;
	output->output =
		wl_display_bind(d->display, id, &wl_output_interface);
	wl_list_insert(d->output_list.prev, &output->link);

	wl_output_add_listener(output->output, &output_listener, output);
}

static void
output_destroy(struct output *output)
{
	if (output->destroy_handler)
		(*output->destroy_handler)(output, output->user_data);

	wl_output_destroy(output->output);
	wl_list_remove(&output->link);
	free(output);
}

void
display_set_output_configure_handler(struct display *display,
				     display_output_handler_t handler)
{
	struct output *output;

	display->output_configure_handler = handler;
	if (!handler)
		return;

	wl_list_for_each(output, &display->output_list, link)
		(*display->output_configure_handler)(output,
						     display->user_data);
}

void
output_set_user_data(struct output *output, void *data)
{
	output->user_data = data;
}

void *
output_get_user_data(struct output *output)
{
	return output->user_data;
}

void
output_set_destroy_handler(struct output *output,
			   display_output_handler_t handler)
{
	output->destroy_handler = handler;
	/* FIXME: implement this, once we have way to remove outputs */
}

void
output_get_allocation(struct output *output, struct rectangle *allocation)
{
	*allocation = output->allocation;
}

struct wl_output *
output_get_wl_output(struct output *output)
{
	return output->output;
}

static void
display_add_input(struct display *d, uint32_t id)
{
	struct input *input;

	input = malloc(sizeof *input);
	if (input == NULL)
		return;

	memset(input, 0, sizeof *input);
	input->display = d;
	input->input_device =
		wl_display_bind(d->display, id, &wl_input_device_interface);
	input->pointer_focus = NULL;
	input->keyboard_focus = NULL;
	wl_list_insert(d->input_list.prev, &input->link);

	wl_input_device_add_listener(input->input_device,
				     &input_device_listener, input);
	wl_input_device_set_user_data(input->input_device, input);

	input->data_device =
		wl_data_device_manager_get_data_device(d->data_device_manager,
						       input->input_device);
	wl_data_device_add_listener(input->data_device,
				    &data_device_listener, input);
}

static void
input_destroy(struct input *input)
{
	input_remove_keyboard_focus(input);
	input_remove_pointer_focus(input);

	if (input->drag_offer)
		data_offer_destroy(input->drag_offer);

	if (input->selection_offer)
		data_offer_destroy(input->selection_offer);

	wl_data_device_destroy(input->data_device);
	wl_list_remove(&input->link);
	wl_input_device_destroy(input->input_device);
	free(input);
}

static void
display_handle_global(struct wl_display *display, uint32_t id,
		      const char *interface, uint32_t version, void *data)
{
	struct display *d = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			wl_display_bind(display, id, &wl_compositor_interface);
	} else if (strcmp(interface, "wl_output") == 0) {
		display_add_output(d, id);
	} else if (strcmp(interface, "wl_input_device") == 0) {
		display_add_input(d, id);
	} else if (strcmp(interface, "wl_shell") == 0) {
		d->shell = wl_display_bind(display, id, &wl_shell_interface);
	} else if (strcmp(interface, "wl_shm") == 0) {
		d->shm = wl_display_bind(display, id, &wl_shm_interface);
	} else if (strcmp(interface, "wl_data_device_manager") == 0) {
		d->data_device_manager =
			wl_display_bind(display, id,
					&wl_data_device_manager_interface);
	}
}

static void
display_render_frame(struct display *d)
{
	cairo_t *cr;
	cairo_pattern_t *pattern;

	d->frame_radius = 3;
	d->shadow = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 128, 128);
	cr = cairo_create(d->shadow);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba(cr, 0, 0, 0, 1);
	rounded_rect(cr, 32, 32, 96, 96, d->frame_radius);
	cairo_fill(cr);
	cairo_destroy(cr);
	blur_surface(d->shadow, 64);

	d->active_frame =
		cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 128, 128);
	cr = cairo_create(d->active_frame);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	pattern = cairo_pattern_create_linear(16, 16, 16, 112);
	cairo_pattern_add_color_stop_rgb(pattern, 0.0, 1.0, 1.0, 1.0);
	cairo_pattern_add_color_stop_rgb(pattern, 0.2, 0.8, 0.8, 0.8);
	cairo_set_source(cr, pattern);
	cairo_pattern_destroy(pattern);

	rounded_rect(cr, 0, 0, 128, 128, d->frame_radius);
	cairo_fill(cr);
	cairo_destroy(cr);

	d->inactive_frame =
		cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 128, 128);
	cr = cairo_create(d->inactive_frame);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgba(cr, 0.75, 0.75, 0.75, 1);
	rounded_rect(cr, 0, 0, 128, 128, d->frame_radius);
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

static void
fini_xkb(struct display *display)
{
	xkb_free_keymap(display->xkb);
}

#ifdef HAVE_CAIRO_EGL
static int
init_egl(struct display *d)
{
	EGLint major, minor;
	EGLint n;

#ifdef USE_CAIRO_GLESV2
#  define GL_BIT EGL_OPENGL_ES2_BIT
#else
#  define GL_BIT EGL_OPENGL_BIT
#endif

	static const EGLint argb_cfg_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PIXMAP_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 1,
		EGL_DEPTH_SIZE, 1,
		EGL_RENDERABLE_TYPE, GL_BIT,
		EGL_NONE
	};

#ifdef USE_CAIRO_GLESV2
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	EGLint api = EGL_OPENGL_ES_API;
#else
	EGLint *context_attribs = NULL;
	EGLint api = EGL_OPENGL_API;
#endif

	d->dpy = eglGetDisplay(d->display);
	if (!eglInitialize(d->dpy, &major, &minor)) {
		fprintf(stderr, "failed to initialize display\n");
		return -1;
	}

	if (!eglBindAPI(api)) {
		fprintf(stderr, "failed to bind api EGL_OPENGL_API\n");
		return -1;
	}

	if (!eglChooseConfig(d->dpy, argb_cfg_attribs,
			     &d->argb_config, 1, &n) || n != 1) {
		fprintf(stderr, "failed to choose argb config\n");
		return -1;
	}

	d->argb_ctx = eglCreateContext(d->dpy, d->argb_config,
				       EGL_NO_CONTEXT, context_attribs);
	if (d->argb_ctx == NULL) {
		fprintf(stderr, "failed to create context\n");
		return -1;
	}

	if (!eglMakeCurrent(d->dpy, NULL, NULL, d->argb_ctx)) {
		fprintf(stderr, "failed to make context current\n");
		return -1;
	}

#ifdef HAVE_CAIRO_EGL
	d->argb_device = cairo_egl_device_create(d->dpy, d->argb_ctx);
	if (cairo_device_status(d->argb_device) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "failed to get cairo egl argb device\n");
		return -1;
	}
#endif

	return 0;
}

static void
fini_egl(struct display *display)
{
#ifdef HAVE_CAIRO_EGL
	cairo_device_destroy(display->argb_device);
#endif

	eglMakeCurrent(display->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
		       EGL_NO_CONTEXT);

	eglTerminate(display->dpy);
	eglReleaseThread();
}
#endif

static int
event_mask_update(uint32_t mask, void *data)
{
	struct display *d = data;

	d->mask = mask;

	return 0;
}

static void
handle_display_data(struct task *task, uint32_t events)
{
	struct display *display =
		container_of(task, struct display, display_task);
	
	wl_display_iterate(display->display, display->mask);
}

struct display *
display_create(int argc, char *argv[])
{
	struct display *d;

	argc = parse_options(xkb_options,
			     ARRAY_LENGTH(xkb_options), argc, argv);

	d = malloc(sizeof *d);
	if (d == NULL)
		return NULL;

        memset(d, 0, sizeof *d);

	d->display = wl_display_connect(NULL);
	if (d->display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return NULL;
	}

	d->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	d->display_fd = wl_display_get_fd(d->display, event_mask_update, d);
	d->display_task.run = handle_display_data;
	display_watch_fd(d, d->display_fd, EPOLLIN, &d->display_task);

	wl_list_init(&d->deferred_list);
	wl_list_init(&d->input_list);
	wl_list_init(&d->output_list);

	/* Set up listener so we'll catch all events. */
	wl_display_add_global_listener(d->display,
				       display_handle_global, d);

	/* Process connection events. */
	wl_display_iterate(d->display, WL_DISPLAY_READABLE);
#ifdef HAVE_CAIRO_EGL
	if (init_egl(d) < 0)
		return NULL;
#endif

	d->image_target_texture_2d =
		(void *) eglGetProcAddress("glEGLImageTargetTexture2DOES");
	d->create_image = (void *) eglGetProcAddress("eglCreateImageKHR");
	d->destroy_image = (void *) eglGetProcAddress("eglDestroyImageKHR");

	create_pointer_surfaces(d);

	display_render_frame(d);

	wl_list_init(&d->window_list);

	init_xkb(d);

	return d;
}

static void
display_destroy_outputs(struct display *display)
{
	struct output *tmp;
	struct output *output;

	wl_list_for_each_safe(output, tmp, &display->output_list, link)
		output_destroy(output);
}

static void
display_destroy_inputs(struct display *display)
{
	struct input *tmp;
	struct input *input;

	wl_list_for_each_safe(input, tmp, &display->input_list, link)
		input_destroy(input);
}

void
display_destroy(struct display *display)
{
	if (!wl_list_empty(&display->window_list))
		fprintf(stderr, "toytoolkit warning: windows exist.\n");

	if (!wl_list_empty(&display->deferred_list))
		fprintf(stderr, "toytoolkit warning: deferred tasks exist.\n");

	display_destroy_outputs(display);
	display_destroy_inputs(display);

	fini_xkb(display);

	cairo_surface_destroy(display->active_frame);
	cairo_surface_destroy(display->inactive_frame);
	cairo_surface_destroy(display->shadow);
	destroy_pointer_surfaces(display);

#ifdef HAVE_CAIRO_EGL
	fini_egl(display);
#endif

	if (display->shell)
		wl_shell_destroy(display->shell);

	if (display->shm)
		wl_shm_destroy(display->shm);

	if (display->data_device_manager)
		wl_data_device_manager_destroy(display->data_device_manager);

	wl_compositor_destroy(display->compositor);

	close(display->epoll_fd);

	wl_display_flush(display->display);
	wl_display_disconnect(display->display);
	free(display);
}

void
display_set_user_data(struct display *display, void *data)
{
	display->user_data = data;
}

void *
display_get_user_data(struct display *display)
{
	return display->user_data;
}

struct wl_display *
display_get_display(struct display *display)
{
	return display->display;
}

struct output *
display_get_output(struct display *display)
{
	return container_of(display->output_list.next, struct output, link);
}

struct wl_compositor *
display_get_compositor(struct display *display)
{
	return display->compositor;
}

uint32_t
display_get_serial(struct display *display)
{
	return display->serial;
}

EGLDisplay
display_get_egl_display(struct display *d)
{
	return d->dpy;
}

struct wl_data_source *
display_create_data_source(struct display *display)
{
	return wl_data_device_manager_create_data_source(display->data_device_manager);
}

EGLConfig
display_get_argb_egl_config(struct display *d)
{
	return d->argb_config;
}

struct wl_shell *
display_get_shell(struct display *display)
{
	return display->shell;
}

int
display_acquire_window_surface(struct display *display,
			       struct window *window,
			       EGLContext ctx)
{
#ifdef HAVE_CAIRO_EGL
	struct egl_window_surface_data *data;
	cairo_device_t *device;

	if (!window->cairo_surface)
		return -1;
	device = cairo_surface_get_device(window->cairo_surface);
	if (!device)
		return -1;

	if (!ctx) {
		if (device == display->argb_device)
			ctx = display->argb_ctx;
		else
			assert(0);
	}

	data = cairo_surface_get_user_data(window->cairo_surface,
					   &surface_data_key);

	cairo_device_flush(device);
	cairo_device_acquire(device);
	if (!eglMakeCurrent(display->dpy, data->surf, data->surf, ctx))
		fprintf(stderr, "failed to make surface current\n");

	return 0;
#else
	return -1;
#endif
}

void
display_release_window_surface(struct display *display,
			       struct window *window)
{
#ifdef HAVE_CAIRO_EGL
	cairo_device_t *device;
	
	device = cairo_surface_get_device(window->cairo_surface);
	if (!device)
		return;

	if (!eglMakeCurrent(display->dpy, NULL, NULL, display->argb_ctx))
		fprintf(stderr, "failed to make context current\n");
	cairo_device_release(device);
#endif
}

void
display_defer(struct display *display, struct task *task)
{
	wl_list_insert(&display->deferred_list, &task->link);
}

void
display_watch_fd(struct display *display,
		 int fd, uint32_t events, struct task *task)
{
	struct epoll_event ep;

	ep.events = events;
	ep.data.ptr = task;
	epoll_ctl(display->epoll_fd, EPOLL_CTL_ADD, fd, &ep);
}

void
display_run(struct display *display)
{
	struct task *task;
	struct epoll_event ep[16];
	int i, count;

	display->running = 1;
	while (1) {
		wl_display_flush(display->display);

		while (!wl_list_empty(&display->deferred_list)) {
			task = container_of(display->deferred_list.next,
					    struct task, link);
			wl_list_remove(&task->link);
			task->run(task, 0);
		}

		if (!display->running)
			break;

		wl_display_flush(display->display);

		count = epoll_wait(display->epoll_fd,
				   ep, ARRAY_LENGTH(ep), -1);
		for (i = 0; i < count; i++) {
			task = ep[i].data.ptr;
			task->run(task, ep[i].events);
		}
	}
}

void
display_exit(struct display *display)
{
	display->running = 0;
}
