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

#ifndef _WAYLAND_SYSTEM_COMPOSITOR_H_
#define _WAYLAND_SYSTEM_COMPOSITOR_H_

#include <termios.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libudev.h>
#include "wayland-server.h"
#include "wayland-util.h"

#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

struct wlsc_matrix {
	GLfloat d[16];
};

struct wlsc_surface;

struct wlsc_listener {
	struct wl_list link;
	void (*func)(struct wlsc_listener *listener,
		     struct wlsc_surface *surface);
};

struct wlsc_output {
	struct wl_object object;
	struct wl_list link;
	struct wlsc_compositor *compositor;
	struct wlsc_surface *background;
	struct wlsc_matrix matrix;
	int32_t x, y, width, height;
};

/* These should be part of the protocol */
enum wlsc_grab_type {
	WLSC_DEVICE_GRAB_NONE = 0,
	WLSC_DEVICE_GRAB_RESIZE_TOP = 1,
	WLSC_DEVICE_GRAB_RESIZE_BOTTOM = 2,
	WLSC_DEVICE_GRAB_RESIZE_LEFT = 4,
	WLSC_DEVICE_GRAB_RESIZE_TOP_LEFT = 5,
	WLSC_DEVICE_GRAB_RESIZE_BOTTOM_LEFT = 6,
	WLSC_DEVICE_GRAB_RESIZE_RIGHT = 8,
	WLSC_DEVICE_GRAB_RESIZE_TOP_RIGHT = 9,
	WLSC_DEVICE_GRAB_RESIZE_BOTTOM_RIGHT = 10,
	WLSC_DEVICE_GRAB_RESIZE_MASK = 15,
	WLSC_DEVICE_GRAB_MOVE = 16,
	WLSC_DEVICE_GRAB_MOTION = 17,
	WLSC_DEVICE_GRAB_DRAG = 18
};

enum wlsc_pointer_type {
	WLSC_POINTER_BOTTOM_LEFT,
	WLSC_POINTER_BOTTOM_RIGHT,
	WLSC_POINTER_BOTTOM,
	WLSC_POINTER_DRAGGING,
	WLSC_POINTER_LEFT_PTR,
	WLSC_POINTER_LEFT,
	WLSC_POINTER_RIGHT,
	WLSC_POINTER_TOP_LEFT,
	WLSC_POINTER_TOP_RIGHT,
	WLSC_POINTER_TOP,
	WLSC_POINTER_IBEAM,
};

struct wlsc_input_device {
	struct wl_input_device input_device;
	int32_t x, y;
	struct wlsc_compositor *ec;
	struct wlsc_surface *sprite;
	int32_t hotspot_x, hotspot_y;
	struct wl_list link;

	uint32_t modifier_state;

	enum wlsc_grab_type grab;
	struct wlsc_surface *grab_surface;
	uint32_t grab_time;
	int32_t grab_x, grab_y;
	int32_t grab_width, grab_height;
	int32_t grab_dx, grab_dy;
	uint32_t grab_button;
	struct wl_drag *drag;

	struct wlsc_listener listener;
};

struct wlsc_drm {
	struct wl_object object;
	int fd;
	char *filename;
};

struct wlsc_drm_buffer {
	struct wl_buffer buffer;
	EGLImageKHR image;
};

struct wlsc_shm {
	struct wl_object object;
};

struct wlsc_shm_buffer {
	struct wl_buffer buffer;
	int32_t stride;
	void *data;
};

struct wlsc_compositor {
	struct wl_compositor compositor;
	struct wl_visual argb_visual, premultiplied_argb_visual, rgb_visual;

	struct wlsc_drm drm;
	struct wlsc_shm shm;
	EGLDisplay display;
	EGLContext context;
	GLuint fbo, vbo;
	GLuint proj_uniform, tex_uniform;
	struct wlsc_drm_buffer **pointer_buffers;
	struct wl_display *wl_display;

	/* We implement the shell interface. */
	struct wl_shell shell;

	/* There can be more than one, but not right now... */
	struct wlsc_input_device *input_device;

	struct wl_list output_list;
	struct wl_list input_device_list;
	struct wl_list surface_list;

	struct wl_list surface_destroy_listener_list;

	/* Repaint state. */
	struct wl_event_source *timer_source;
	int repaint_needed;
	int repaint_on_timeout;
	struct timespec previous_swap;

	uint32_t focus;

	void (*destroy)(struct wlsc_compositor *ec);
	int (*authenticate)(struct wlsc_compositor *c, uint32_t id);
	void (*present)(struct wlsc_compositor *c);
};

#define MODIFIER_CTRL	(1 << 8)
#define MODIFIER_ALT	(1 << 9)
#define MODIFIER_SUPER	(1 << 10)

struct wlsc_vector {
	GLfloat f[4];
};

struct wlsc_surface {
	struct wl_surface surface;
	struct wlsc_compositor *compositor;
	GLuint texture;
	int32_t x, y, width, height;
	struct wl_list link;
	struct wlsc_matrix matrix;
	struct wlsc_matrix matrix_inv;
	struct wl_visual *visual;
	struct wl_buffer *buffer;
};

void
notify_motion(struct wlsc_input_device *device,
	      uint32_t time, int x, int y);
void
notify_button(struct wlsc_input_device *device,
	      uint32_t time, int32_t button, int32_t state);
void
notify_key(struct wlsc_input_device *device,
	   uint32_t time, uint32_t key, uint32_t state);

void
wlsc_compositor_finish_frame(struct wlsc_compositor *compositor, int msecs);
void
wlsc_compositor_schedule_repaint(struct wlsc_compositor *compositor);

struct wlsc_drm_buffer *
wlsc_drm_buffer_create(struct wlsc_compositor *ec,
		       int width, int height, struct wl_visual *visual);

int
wlsc_compositor_init(struct wlsc_compositor *ec, struct wl_display *display);
void
wlsc_output_init(struct wlsc_output *output, struct wlsc_compositor *c,
		 int x, int y, int width, int height);
void
wlsc_input_device_init(struct wlsc_input_device *device,
		       struct wlsc_compositor *ec);
int
wlsc_drm_init(struct wlsc_compositor *ec, int fd, const char *filename);

int
wlsc_shm_init(struct wlsc_compositor *ec);

struct wl_buffer *
wl_buffer_create_drm(struct wlsc_compositor *compositor,
		     struct wl_visual *visual);

struct wlsc_compositor *
x11_compositor_create(struct wl_display *display, int width, int height);

struct wlsc_compositor *
drm_compositor_create(struct wl_display *display, int connector);

struct wlsc_compositor *
wayland_compositor_create(struct wl_display *display, int width, int height);

void
screenshooter_create(struct wlsc_compositor *ec);

#endif
