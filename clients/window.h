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

struct window;

struct rectangle {
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
};

struct display;
struct input;

struct display *
display_create(int *argc, char **argv[], const GOptionEntry *option_entries);

struct wl_compositor *
display_get_compositor(struct display *display);

#ifdef EGL_NO_DISPLAY
EGLDisplay
display_get_egl_display(struct display *d);
#endif

cairo_surface_t *
display_create_surface(struct display *display,
		       struct rectangle *rectangle);

struct wl_buffer *
display_get_buffer_for_surface(struct display *display,
			       cairo_surface_t *surface);

cairo_surface_t *
display_get_pointer_surface(struct display *display, int pointer,
			    int *width, int *height,
			    int *hotspot_x, int *hotspot_y);

void
display_add_drag_listener(struct display *display,
			  const struct wl_drag_listener *drag_listener,
			  void *data);

void
display_run(struct display *d);

enum {
	WINDOW_MODIFIER_SHIFT = 0x01,
	WINDOW_MODIFIER_LOCK = 0x02,
	WINDOW_MODIFIER_CONTROL = 0x04,
	WINDOW_MODIFIER_ALT = 0x08,
	WINDOW_MODIFIER_MOD2 = 0x10,
};

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

typedef void (*window_resize_handler_t)(struct window *window, void *data);
typedef void (*window_redraw_handler_t)(struct window *window, void *data);
typedef void (*window_frame_handler_t)(struct window *window, uint32_t frame, uint32_t timestamp, void *data);
typedef void (*window_acknowledge_handler_t)(struct window *window, uint32_t key, uint32_t frame, void *data);
typedef void (*window_key_handler_t)(struct window *window, uint32_t key, uint32_t unicode,
				     uint32_t state, uint32_t modifiers, void *data);
typedef void (*window_keyboard_focus_handler_t)(struct window *window,
						struct input *device, void *data);

typedef void (*window_button_handler_t)(struct window *window,
					struct input *input, uint32_t time,
					int button, int state, void *data);

typedef int (*window_motion_handler_t)(struct window *window,
				       struct input *input, uint32_t time,
				       int32_t x, int32_t y,
				       int32_t sx, int32_t sy, void *data);

struct window *
window_create(struct display *display, const char *title,
	      int32_t x, int32_t y, int32_t width, int32_t height);

void
window_draw(struct window *window);
void
window_commit(struct window *window, uint32_t key);
void
window_get_child_rectangle(struct window *window,
			   struct rectangle *rectangle);
void
window_set_child_size(struct window *window,
		      struct rectangle *rectangle);
void
window_copy_image(struct window *window,
		  struct rectangle *rectangle,
		  void *image);
void
window_schedule_redraw(struct window *window);

void
window_move(struct window *window, int32_t x, int32_t y);

cairo_surface_t *
window_get_surface(struct window *window);

void
window_copy_surface(struct window *window,
		    struct rectangle *rectangle,
		    cairo_surface_t *surface);

void
window_set_fullscreen(struct window *window, int fullscreen);

void
window_set_user_data(struct window *window, void *data);

void
window_set_redraw_handler(struct window *window,
			  window_redraw_handler_t handler);

void
window_set_decoration(struct window *window, int decoration);

void
window_set_resize_handler(struct window *window,
			  window_resize_handler_t handler);
void
window_set_frame_handler(struct window *window,
			 window_frame_handler_t handler);
void
window_set_acknowledge_handler(struct window *window,
			       window_acknowledge_handler_t handler);

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
window_set_keyboard_focus_handler(struct window *window,
				  window_keyboard_focus_handler_t handler);

void
window_set_acknowledge_handler(struct window *window,
			       window_acknowledge_handler_t handler);

void
window_set_frame_handler(struct window *window,
			 window_frame_handler_t handler);

void
window_start_drag(struct window *window, struct input *input, uint32_t time);

void
input_get_position(struct input *input, int32_t *x, int32_t *y);

struct wl_input_device *
input_get_input_device(struct input *input);

#endif
