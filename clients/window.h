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

#ifndef _WINDOW_H_
#define _WINDOW_H_

#include <X11/extensions/XKBcommon.h>
#include <glib.h>
#include <wayland-client.h>
#include <cairo.h>

struct window;
struct item;
struct display;
struct input;
struct output;

struct task {
	void (*run)(struct task *task, uint32_t events);
	struct wl_list link;
};

struct rectangle {
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
};

struct display *
display_create(int *argc, char **argv[], const GOptionEntry *option_entries);

void
display_destroy(struct display *display);

void
display_set_user_data(struct display *display, void *data);

void *
display_get_user_data(struct display *display);

struct wl_display *
display_get_display(struct display *display);

struct wl_compositor *
display_get_compositor(struct display *display);

struct wl_shell *
display_get_shell(struct display *display);

struct output *
display_get_output(struct display *display);

typedef void (*display_output_handler_t)(struct output *output, void *data);

/*
 * The output configure handler is called, when a new output is connected
 * and we know its current mode, or when the current mode changes.
 * Test and set the output user data in your handler to know, if the
 * output is new. Note: 'data' in the configure handler is the display
 * user data.
 */
void
display_set_output_configure_handler(struct display *display,
				     display_output_handler_t handler);

struct wl_data_source *
display_create_data_source(struct display *display);

#ifdef EGL_NO_DISPLAY
EGLDisplay
display_get_egl_display(struct display *d);

EGLConfig
display_get_rgb_egl_config(struct display *d);

EGLConfig
display_get_argb_egl_config(struct display *d);

int
display_acquire_window_surface(struct display *display,
			       struct window *window,
			       EGLContext ctx);
void
display_release_window_surface(struct display *display,
			       struct window *window);

#ifdef HAVE_CAIRO_EGL
EGLImageKHR
display_get_image_for_egl_image_surface(struct display *display,
					cairo_surface_t *surface);
#endif
#endif

#define SURFACE_OPAQUE 0x01

cairo_surface_t *
display_create_surface(struct display *display,
		       struct wl_surface *surface,
		       struct rectangle *rectangle,
		       uint32_t flags);

struct wl_buffer *
display_get_buffer_for_surface(struct display *display,
			       cairo_surface_t *surface);

cairo_surface_t *
display_get_pointer_surface(struct display *display, int pointer,
			    int *width, int *height,
			    int *hotspot_x, int *hotspot_y);

void
display_defer(struct display *display, struct task *task);

void
display_watch_fd(struct display *display,
		 int fd, uint32_t events, struct task *task);

void
display_run(struct display *d);

void
display_exit(struct display *d);

enum pointer_type {
	POINTER_BOTTOM_LEFT,
	POINTER_BOTTOM_RIGHT,
	POINTER_BOTTOM,
	POINTER_DRAGGING,
	POINTER_LEFT_PTR,
	POINTER_LEFT,
	POINTER_RIGHT,
	POINTER_TOP_LEFT,
	POINTER_TOP_RIGHT,
	POINTER_TOP,
	POINTER_IBEAM,
	POINTER_HAND1,
};

typedef void (*window_resize_handler_t)(struct window *window,
					int32_t width, int32_t height,
					void *data);
typedef void (*window_redraw_handler_t)(struct window *window, void *data);

typedef void (*window_key_handler_t)(struct window *window, struct input *input,
				     uint32_t time, uint32_t key, uint32_t unicode,
				     uint32_t state, void *data);

typedef void (*window_keyboard_focus_handler_t)(struct window *window,
						struct input *device, void *data);

typedef void (*window_button_handler_t)(struct window *window,
					struct input *input, uint32_t time,
					int button, int state, void *data);

typedef int (*window_enter_handler_t)(struct window *window,
				      struct input *input, uint32_t time,
				      int32_t x, int32_t y, void *data);
typedef void (*window_leave_handler_t)(struct window *window,
				       struct input *input, uint32_t time,
				       void *data);

typedef int (*window_motion_handler_t)(struct window *window,
				       struct input *input, uint32_t time,
				       int32_t x, int32_t y,
				       int32_t sx, int32_t sy, void *data);

typedef void (*window_data_handler_t)(struct window *window,
				      struct input *input, uint32_t time,
				      int32_t x, int32_t y,
				      const char **types,
				      void *data);

typedef void (*window_drop_handler_t)(struct window *window,
				      struct input *input,
				      int32_t x, int32_t y, void *data);

typedef void (*window_item_focus_handler_t)(struct window *window,
					    struct item *focus, void *data);

struct window *
window_create(struct display *display, int32_t width, int32_t height);
struct window *
window_create_transient(struct display *display, struct window *parent,
			int32_t x, int32_t y, int32_t width, int32_t height);
struct window *
window_create_menu(struct display *display,
		   struct input *input, uint32_t time, struct window *parent,
		   int32_t x, int32_t y, const char **entries, int count);

void
window_destroy(struct window *window);

struct item *
window_add_item(struct window *window, void *data);

typedef void (*item_func_t)(struct item *item, void *data);

typedef void (*data_func_t)(void *data, size_t len,
			    int32_t x, int32_t y, void *user_data);

void
window_for_each_item(struct window *window, item_func_t func, void *data);

struct item *
window_get_focus_item(struct window *window);
struct display *
window_get_display(struct window *window);

void
window_move(struct window *window, struct input *input, uint32_t time);

void
window_draw(struct window *window);

void
window_get_allocation(struct window *window,
		      struct rectangle *allocation);

void
window_get_child_allocation(struct window *window,
			    struct rectangle *allocation);

void
window_set_transparent(struct window *window, int transparent);
void
window_set_child_size(struct window *window, int32_t width, int32_t height);
void
window_schedule_redraw(struct window *window);

void
window_damage(struct window *window, int32_t x, int32_t y,
	      int32_t width, int32_t height);

cairo_surface_t *
window_get_surface(struct window *window);

struct wl_surface *
window_get_wl_surface(struct window *window);

struct wl_shell_surface *
window_get_wl_shell_surface(struct window *window);

void
window_flush(struct window *window);

void
window_set_surface(struct window *window, cairo_surface_t *surface);

void
window_create_surface(struct window *window);

enum window_buffer_type {
	WINDOW_BUFFER_TYPE_EGL_WINDOW,
	WINDOW_BUFFER_TYPE_EGL_IMAGE,
	WINDOW_BUFFER_TYPE_SHM,
};

void
display_surface_damage(struct display *display, cairo_surface_t *cairo_surface,
		       int32_t x, int32_t y, int32_t width, int32_t height);

void
window_set_buffer_type(struct window *window, enum window_buffer_type type);

void
window_set_fullscreen(struct window *window, int fullscreen);

void
window_set_custom(struct window *window);

void
window_set_user_data(struct window *window, void *data);

void *
window_get_user_data(struct window *window);

void
window_set_redraw_handler(struct window *window,
			  window_redraw_handler_t handler);

void
window_set_decoration(struct window *window, int decoration);

void
window_set_resize_handler(struct window *window,
			  window_resize_handler_t handler);

void
window_set_key_handler(struct window *window,
		       window_key_handler_t handler);

void
window_set_button_handler(struct window *window,
			  window_button_handler_t handler);

void
window_set_motion_handler(struct window *window,
			  window_motion_handler_t handler);

void
window_set_enter_handler(struct window *window,
			 window_enter_handler_t handler);
void
window_set_leave_handler(struct window *window,
			 window_leave_handler_t handler);

void
window_set_keyboard_focus_handler(struct window *window,
				  window_keyboard_focus_handler_t handler);

void
window_set_item_focus_handler(struct window *window,
			      window_item_focus_handler_t handler);

void
window_set_data_handler(struct window *window,
			window_data_handler_t handler);

void
window_set_drop_handler(struct window *window,
			window_drop_handler_t handler);

void
window_set_title(struct window *window, const char *title);

const char *
window_get_title(struct window *window);

void
item_get_allocation(struct item *item, struct rectangle *allocation);

void
item_set_allocation(struct item *item,
		    int32_t x, int32_t y, int32_t width, int32_t height);

void *
item_get_user_data(struct item *item);

void
input_set_pointer_image(struct input *input, uint32_t time, int pointer);

void
input_get_position(struct input *input, int32_t *x, int32_t *y);

uint32_t
input_get_modifiers(struct input *input);

struct wl_input_device *
input_get_input_device(struct input *input);

struct wl_data_device *
input_get_data_device(struct input *input);

void
input_set_selection(struct input *input,
		    struct wl_data_source *source, uint32_t time);

void
input_accept(struct input *input, uint32_t time, const char *type);


void
input_receive_drag_data(struct input *input, const char *mime_type,
			data_func_t func, void *user_data);

int
input_receive_selection_data(struct input *input, const char *mime_type,
			     data_func_t func, void *data);
int
input_receive_selection_data_to_fd(struct input *input,
				   const char *mime_type, int fd);

void
output_set_user_data(struct output *output, void *data);

void *
output_get_user_data(struct output *output);

void
output_set_destroy_handler(struct output *output,
			   display_output_handler_t handler);

void
output_get_allocation(struct output *output, struct rectangle *allocation);

struct wl_output *
output_get_wl_output(struct output *output);

#endif
