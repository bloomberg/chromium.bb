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
#include "wayland.h"
#include "wayland-util.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

struct wlsc_matrix {
	GLfloat d[16];
};

struct wl_visual {
	struct wl_object base;
};

struct wlsc_surface;

struct wlsc_listener {
	struct wl_list link;
	void (*func)(struct wlsc_listener *listener,
		     struct wlsc_surface *surface);
};

struct wlsc_output {
	struct wl_object base;
	struct wl_list link;
	struct wlsc_compositor *compositor;
	struct wlsc_surface *background;
	struct wlsc_matrix matrix;
	int32_t x, y, width, height;
};

struct wlsc_input_device {
	struct wl_object base;
	int32_t x, y;
	struct wlsc_compositor *ec;
	struct wlsc_surface *sprite;
	struct wl_list link;

	int grab;
	struct wlsc_surface *grab_surface;
	struct wlsc_surface *pointer_focus;
	struct wlsc_surface *keyboard_focus;
	struct wl_array keys;

	struct wlsc_listener listener;
};

struct wlsc_compositor {
	struct wl_compositor base;
	struct wl_visual argb_visual, premultiplied_argb_visual, rgb_visual;

	EGLDisplay display;
	EGLContext context;
	GLuint fbo, vbo;
	GLuint proj_uniform, tex_uniform;
	struct wl_display *wl_display;

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
	uint32_t current_frame;

	uint32_t modifier_state;
	uint32_t focus;

	void (*present)(struct wlsc_compositor *c);
};

#define MODIFIER_CTRL	(1 << 8)
#define MODIFIER_ALT	(1 << 9)

struct wlsc_vector {
	GLfloat f[4];
};

struct wlsc_surface {
	struct wl_surface base;
	struct wlsc_compositor *compositor;
	struct wl_visual *visual;
	GLuint texture;
	EGLImageKHR image;
	int width, height;
	struct wl_list link;
	struct wlsc_matrix matrix;
	struct wlsc_matrix matrix_inv;
};

void
notify_motion(struct wlsc_input_device *device, int x, int y);
void
notify_button(struct wlsc_input_device *device, int32_t button, int32_t state);
void
notify_key(struct wlsc_input_device *device, uint32_t key, uint32_t state);

void
wlsc_compositor_finish_frame(struct wlsc_compositor *compositor, int msecs);
void
wlsc_compositor_schedule_repaint(struct wlsc_compositor *compositor);

int
wlsc_compositor_init(struct wlsc_compositor *ec, struct wl_display *display);
void
wlsc_output_init(struct wlsc_output *output, struct wlsc_compositor *c,
		 int x, int y, int width, int height);
void
wlsc_input_device_init(struct wlsc_input_device *device,
		       struct wlsc_compositor *ec);

struct wlsc_compositor *
x11_compositor_create(struct wl_display *display);

struct wlsc_compositor *
drm_compositor_create(struct wl_display *display);

void
screenshooter_create(struct wlsc_compositor *ec);

extern const char *option_background;
extern int option_connector;

#endif
