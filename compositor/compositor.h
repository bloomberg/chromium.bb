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

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libudev.h>
#include <pixman.h>
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

struct wlsc_output {
	struct wl_object object;
	struct wl_list link;
	struct wlsc_compositor *compositor;
	struct wlsc_surface *background;
	struct wlsc_matrix matrix;
	int32_t x, y, width, height;
	pixman_region32_t previous_damage_region;
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
	struct wlsc_surface *sprite;
	int32_t hotspot_x, hotspot_y;
	struct wl_list link;
	uint32_t modifier_state;
	struct wl_selection *selection;
};

struct wlsc_drm {
	struct wl_object object;
	int fd;
	char *filename;
};

struct wlsc_shm {
	struct wl_object object;
};

struct wlsc_compositor {
	struct wl_compositor compositor;

	struct wlsc_drm drm;
	struct wlsc_shm shm;
	EGLDisplay display;
	EGLContext context;
	GLuint fbo;
	GLuint proj_uniform, tex_uniform;
	struct wl_buffer **pointer_buffers;
	struct wl_display *wl_display;

	/* We implement the shell interface. */
	struct wl_shell shell;

	/* There can be more than one, but not right now... */
	struct wl_input_device *input_device;

	struct wl_list output_list;
	struct wl_list input_device_list;
	struct wl_list surface_list;

	/* Repaint state. */
	struct wl_event_source *timer_source;
	int repaint_needed;
	int repaint_on_timeout;
	struct timespec previous_swap;
	pixman_region32_t damage_region;
	struct wl_array vertices, indices;

	struct wlsc_switcher *switcher;
	uint32_t focus;

	void (*destroy)(struct wlsc_compositor *ec);
	int (*authenticate)(struct wlsc_compositor *c, uint32_t id);
	void (*present)(struct wlsc_compositor *c);
	struct wl_buffer *(*create_buffer)(struct wlsc_compositor *c,
					   int32_t width, int32_t height,
					   struct wl_visual *visual,
					   const void *data);
};

#define MODIFIER_CTRL	(1 << 8)
#define MODIFIER_ALT	(1 << 9)
#define MODIFIER_SUPER	(1 << 10)

enum wlsc_output_flags {
	WL_OUTPUT_FLIPPED = 0x01
};

struct wlsc_vector {
	GLfloat f[4];
};

enum wlsc_surface_map_type {
	WLSC_SURFACE_MAP_UNMAPPED,
	WLSC_SURFACE_MAP_TOPLEVEL,
	WLSC_SURFACE_MAP_TRANSIENT,
	WLSC_SURFACE_MAP_FULLSCREEN
};

struct wlsc_surface {
	struct wl_surface surface;
	struct wlsc_compositor *compositor;
	GLuint texture;
	int32_t x, y, width, height;
	int32_t saved_x, saved_y;
	struct wl_list link;
	struct wlsc_matrix matrix;
	struct wlsc_matrix matrix_inv;
	struct wl_visual *visual;
	struct wl_buffer *buffer;
	enum wlsc_surface_map_type map_type;
	struct wlsc_output *fullscreen_output;
};

void
wlsc_surface_update_matrix(struct wlsc_surface *es);

void
notify_motion(struct wl_input_device *device,
	      uint32_t time, int x, int y);
void
notify_button(struct wl_input_device *device,
	      uint32_t time, int32_t button, int32_t state);
void
notify_key(struct wl_input_device *device,
	   uint32_t time, uint32_t key, uint32_t state);

void
notify_pointer_focus(struct wl_input_device *device,
		     uint32_t time,
		     struct wlsc_output *output,
		     int32_t x, int32_t y);

void
notify_keyboard_focus(struct wl_input_device *device,
		      uint32_t time, struct wlsc_output *output,
		      struct wl_array *keys);

void
wlsc_compositor_finish_frame(struct wlsc_compositor *compositor, int msecs);
void
wlsc_compositor_schedule_repaint(struct wlsc_compositor *compositor);

void
wlsc_surface_damage(struct wlsc_surface *surface);

void
wlsc_surface_damage_rectangle(struct wlsc_surface *surface,
			      int32_t x, int32_t y,
			      int32_t width, int32_t height);

void
wlsc_input_device_set_pointer_image(struct wlsc_input_device *device,
				    enum wlsc_pointer_type type);
struct wlsc_surface *
pick_surface(struct wl_input_device *device, int32_t *sx, int32_t *sy);

void
wlsc_selection_set_focus(struct wl_selection *selection,
			 struct wl_surface *surface, uint32_t time);

uint32_t
get_time(void);

struct wl_buffer *
wlsc_drm_buffer_create(struct wlsc_compositor *ec,
		       int width, int height,
		       struct wl_visual *visual, const void *data);

int
wlsc_compositor_init(struct wlsc_compositor *ec, struct wl_display *display);
void
wlsc_output_init(struct wlsc_output *output, struct wlsc_compositor *c,
		 int x, int y, int width, int height, uint32_t flags);
void
wlsc_input_device_init(struct wlsc_input_device *device,
		       struct wlsc_compositor *ec);
int
wlsc_drm_init(struct wlsc_compositor *ec, int fd, const char *filename);

int
wlsc_shm_init(struct wlsc_compositor *ec);

int
wlsc_shell_init(struct wlsc_compositor *ec);

void
shell_move(struct wl_client *client, struct wl_shell *shell,
	   struct wl_surface *surface,
	   struct wl_input_device *device, uint32_t time);

void
shell_resize(struct wl_client *client, struct wl_shell *shell,
	     struct wl_surface *surface,
	     struct wl_input_device *device, uint32_t time, uint32_t edges);

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
evdev_input_add_devices(struct wlsc_compositor *c, struct udev *udev);

struct tty *
tty_create(struct wlsc_compositor *compositor);

void
tty_destroy(struct tty *tty);

void
screenshooter_create(struct wlsc_compositor *ec);

uint32_t *
wlsc_load_image(const char *filename, int width, int height);

#endif
