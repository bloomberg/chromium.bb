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

#include <libudev.h>
#include <pixman.h>
#include "wayland-server.h"
#include "wayland-util.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

#define FD_TO_EGL_NATIVE_DPY(fd) ((EGLNativeDisplayType)(intptr_t)(fd))

struct wlsc_matrix {
	GLfloat d[16];
};

void
wlsc_matrix_init(struct wlsc_matrix *matrix);
void
wlsc_matrix_scale(struct wlsc_matrix *matrix, GLfloat x, GLfloat y, GLfloat z);
void
wlsc_matrix_translate(struct wlsc_matrix *matrix,
		      GLfloat x, GLfloat y, GLfloat z);

struct wlsc_transform {
	struct wlsc_matrix matrix;
	struct wlsc_matrix inverse;
};

struct wlsc_surface;
struct wlsc_input_device;

struct wlsc_mode {
	uint32_t flags;
	int32_t width, height;
	uint32_t refresh;
	struct wl_list link;
};

struct wlsc_output {
	struct wl_resource resource;
	struct wl_list link;
	struct wlsc_compositor *compositor;
	struct wlsc_surface *background;
	struct wlsc_matrix matrix;
	struct wl_list frame_callback_list;
	int32_t x, y, mm_width, mm_height;
	pixman_region32_t region;
	pixman_region32_t previous_damage;
	uint32_t flags;
	int repaint_needed;
	int repaint_scheduled;

	char *make, *model;
	uint32_t subpixel;
	
	struct wlsc_mode *current;
	struct wl_list mode_list;
	struct wl_buffer *scanout_buffer;
	struct wl_listener scanout_buffer_destroy_listener;

	int (*prepare_render)(struct wlsc_output *output);
	int (*present)(struct wlsc_output *output);
	int (*prepare_scanout_surface)(struct wlsc_output *output,
				       struct wlsc_surface *es);
	int (*set_hardware_cursor)(struct wlsc_output *output,
				   struct wlsc_input_device *input);
	void (*destroy)(struct wlsc_output *output);
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

enum wlsc_visual {
	WLSC_NONE_VISUAL,
	WLSC_ARGB_VISUAL,
	WLSC_PREMUL_ARGB_VISUAL,
	WLSC_RGB_VISUAL
};

struct wlsc_sprite {
	GLuint texture;
	EGLImageKHR image;
	uint32_t visual;
	int width;
	int height;
};

struct wlsc_shader {
	GLuint program;
	GLuint vertex_shader, fragment_shader;
	GLuint proj_uniform;
	GLuint tex_uniform;
	GLuint color_uniform;
};

struct wlsc_animation {
	void (*frame)(struct wlsc_animation *animation,
		      struct wlsc_output *output, uint32_t msecs);
	struct wl_list link;
};

struct wlsc_spring {
	double k;
	double friction;
	double current;
	double target;
	double previous;
	uint32_t timestamp;
};

struct wlsc_shell {
	void (*lock)(struct wlsc_shell *shell);
	void (*attach)(struct wlsc_shell *shell, struct wlsc_surface *surface);
	void (*set_selection_focus)(struct wlsc_shell *shell,
				    struct wl_selection *selection,
				    struct wl_surface *surface, uint32_t time);
};

enum {
	WLSC_COMPOSITOR_ACTIVE,
	WLSC_COMPOSITOR_SLEEPING
};

struct wlsc_compositor {
	struct wl_compositor compositor;

	struct wl_shm *shm;
	struct wlsc_xserver *wxs;

	EGLDisplay display;
	EGLContext context;
	EGLConfig config;
	GLuint fbo;
	GLuint proj_uniform, tex_uniform;
	struct wlsc_sprite **pointer_sprites;
	struct wlsc_shader texture_shader;
	struct wlsc_shader solid_shader;
	struct wl_display *wl_display;

	struct wlsc_shell *shell;

	/* There can be more than one, but not right now... */
	struct wl_input_device *input_device;

	struct wl_list output_list;
	struct wl_list input_device_list;
	struct wl_list surface_list;
	struct wl_list binding_list;
	struct wl_list animation_list;
	struct {
		struct wlsc_spring spring;
		struct wlsc_animation animation;
	} fade;

	uint32_t state;
	struct wl_event_source *idle_source;
	uint32_t idle_inhibit;

	/* Repaint state. */
	struct timespec previous_swap;
	pixman_region32_t damage;
	struct wl_array vertices, indices;

	struct wlsc_surface *overlay;
	struct wlsc_switcher *switcher;
	uint32_t focus;

	PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC
		image_target_renderbuffer_storage;
	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_target_texture_2d;
	PFNEGLCREATEIMAGEKHRPROC create_image;
	PFNEGLDESTROYIMAGEKHRPROC destroy_image;
	PFNEGLBINDWAYLANDDISPLAYWL bind_display;
	PFNEGLUNBINDWAYLANDDISPLAYWL unbind_display;
	int has_bind_display;

	void (*destroy)(struct wlsc_compositor *ec);
	int (*authenticate)(struct wlsc_compositor *c, uint32_t id);
	EGLImageKHR (*create_cursor_image)(struct wlsc_compositor *c,
					   int32_t width, int32_t height);
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
	GLuint texture, saved_texture;
	pixman_region32_t damage;
	pixman_region32_t opaque;
	int32_t x, y, width, height;
	int32_t pitch;
	int32_t saved_x, saved_y;
	struct wl_list link;
	struct wl_list buffer_link;
	struct wlsc_transform *transform;
	uint32_t visual;
	struct wlsc_output *output;
	enum wlsc_surface_map_type map_type;
	struct wlsc_output *fullscreen_output;

	EGLImageKHR image;

	struct wl_buffer *buffer;
	struct wl_listener buffer_destroy_listener;
};

void
wlsc_spring_init(struct wlsc_spring *spring,
		 double k, double current, double target);
void
wlsc_spring_update(struct wlsc_spring *spring, uint32_t msec);
int
wlsc_spring_done(struct wlsc_spring *spring);

void
wlsc_surface_activate(struct wlsc_surface *surface,
		      struct wlsc_input_device *device, uint32_t time);

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
wlsc_output_finish_frame(struct wlsc_output *output, int msecs);
void
wlsc_output_damage(struct wlsc_output *output);
void
wlsc_compositor_schedule_repaint(struct wlsc_compositor *compositor);
void
wlsc_compositor_fade(struct wlsc_compositor *compositor, float tint);
void
wlsc_compositor_damage_all(struct wlsc_compositor *compositor);
void
wlsc_compositor_unlock(struct wlsc_compositor *compositor);
void
wlsc_compositor_wake(struct wlsc_compositor *compositor);

struct wlsc_binding;
typedef void (*wlsc_binding_handler_t)(struct wl_input_device *device,
				       uint32_t time, uint32_t key,
				       uint32_t button,
				       uint32_t state, void *data);
struct wlsc_binding *
wlsc_compositor_add_binding(struct wlsc_compositor *compositor,
			    uint32_t key, uint32_t button, uint32_t modifier,
			    wlsc_binding_handler_t binding, void *data);
void
wlsc_binding_destroy(struct wlsc_binding *binding);

struct wlsc_surface *
wlsc_surface_create(struct wlsc_compositor *compositor,
		    int32_t x, int32_t y, int32_t width, int32_t height);

void
wlsc_surface_configure(struct wlsc_surface *surface,
		       int x, int y, int width, int height);

void
wlsc_surface_assign_output(struct wlsc_surface *surface);

void
wlsc_surface_damage(struct wlsc_surface *surface);

void
wlsc_surface_damage_below(struct wlsc_surface *surface);

void
wlsc_surface_damage_rectangle(struct wlsc_surface *surface,
			      int32_t x, int32_t y,
			      int32_t width, int32_t height);

void
wlsc_input_device_set_pointer_image(struct wlsc_input_device *device,
				    enum wlsc_pointer_type type);
struct wlsc_surface *
pick_surface(struct wl_input_device *device, int32_t *sx, int32_t *sy);

uint32_t
wlsc_compositor_get_time(void);

int
wlsc_compositor_init(struct wlsc_compositor *ec, struct wl_display *display);
int
wlsc_compositor_shutdown(struct wlsc_compositor *ec);
void
wlsc_output_move(struct wlsc_output *output, int x, int y);
void
wlsc_output_init(struct wlsc_output *output, struct wlsc_compositor *c,
		 int x, int y, int width, int height, uint32_t flags);
void
wlsc_output_destroy(struct wlsc_output *output);

void
wlsc_input_device_init(struct wlsc_input_device *device,
		       struct wlsc_compositor *ec);

void
wlsc_switcher_init(struct wlsc_compositor *compositor);

void
evdev_input_add_devices(struct wlsc_compositor *c,
			struct udev *udev, const char *seat);

enum {
	TTY_ENTER_VT,
	TTY_LEAVE_VT
};

typedef void (*tty_vt_func_t)(struct wlsc_compositor *compositor, int event);

struct tty *
tty_create(struct wlsc_compositor *compositor, tty_vt_func_t vt_func);

void
tty_destroy(struct tty *tty);

void
screenshooter_create(struct wlsc_compositor *ec);

uint32_t *
wlsc_load_image(const char *filename,
		int32_t *width_arg, int32_t *height_arg, uint32_t *stride_arg);

struct wlsc_process {
	pid_t pid;
	void (*cleanup)(struct wlsc_process *process, int status);
	struct wl_list link;
};

void
wlsc_watch_process(struct wlsc_process *process);

int
wlsc_xserver_init(struct wlsc_compositor *compositor);
void
wlsc_xserver_destroy(struct wlsc_compositor *compositor);

#endif
