/*
 * Copyright © 2011 Benjamin Franzke
 * Copyright © 2011 Intel Corporation
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
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>

#include <wayland-client.h>
#include <wayland-egl.h>

struct touch {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_shell *shell;
	struct wl_shm *shm;
	struct wl_input_device *input_device;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	struct wl_buffer *buffer;
	int has_argb;
	uint32_t mask;
	int width, height;
	void *data;
};

static void
create_shm_buffer(struct touch *touch)
{
	char filename[] = "/tmp/wayland-shm-XXXXXX";
	int fd, size, stride;

	fd = mkstemp(filename);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %m\n", filename);
		exit(1);
	}
	stride = touch->width * 4;
	size = stride * touch->height;
	if (ftruncate(fd, size) < 0) {
		fprintf(stderr, "ftruncate failed: %m\n");
		close(fd);
		exit(1);
	}

	touch->data =
		mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	unlink(filename);

	if (touch->data == MAP_FAILED) {
		fprintf(stderr, "mmap failed: %m\n");
		close(fd);
		exit(1);
	}

	touch->buffer =
		wl_shm_create_buffer(touch->shm, fd,
				     touch->width, touch->height, stride,
				     WL_SHM_FORMAT_ARGB8888);

	close(fd);
}

static void
shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
	struct touch *touch = data;

	if (format == WL_SHM_FORMAT_ARGB8888)
		touch->has_argb = 1;
}

struct wl_shm_listener shm_listenter = {
	shm_format
};


static void
input_device_handle_motion(void *data, struct wl_input_device *input_device,
			   uint32_t time,
			   int32_t x, int32_t y, int32_t sx, int32_t sy)
{
}

static void
input_device_handle_button(void *data,
			   struct wl_input_device *input_device,
			   uint32_t time, uint32_t button, uint32_t state)
{
}

static void
input_device_handle_key(void *data, struct wl_input_device *input_device,
			uint32_t time, uint32_t key, uint32_t state)
{
}

static void
input_device_handle_pointer_focus(void *data,
				  struct wl_input_device *input_device,
				  uint32_t time, struct wl_surface *surface,
				  int32_t x, int32_t y, int32_t sx, int32_t sy)
{
}

static void
input_device_handle_keyboard_focus(void *data,
				   struct wl_input_device *input_device,
				   uint32_t time,
				   struct wl_surface *surface,
				   struct wl_array *keys)
{
}

static void
touch_paint(struct touch *touch, int32_t x, int32_t y, int32_t id)
{
	uint32_t *p, c;
	static const uint32_t colors[] = {
		0xffff0000,
		0xffffff00,
		0xff0000ff,
		0xffff00ff,
	};

	if (id < ARRAY_LENGTH(colors))
		c = colors[id];
	else
		c = 0xffffffff;

	if (x < 1 || touch->width - 1 < x ||
	    y < 1 || touch->height - 1 < y)
		return;

	p = (uint32_t *) touch->data + (x - 1) + (y -1 ) * touch->width;
	p[1] = c;
	p += touch->width;
	p[0] = c;
	p[1] = c;
	p[2] = c;
	p += touch->width;
	p[1] = c;

	wl_buffer_damage(touch->buffer, 0, 0, touch->width, touch->height);
	wl_surface_damage(touch->surface,
			  0, 0, touch->width, touch->height);
}

static void
input_device_handle_touch_down(void *data,
			       struct wl_input_device *wl_input_device,
			       uint32_t time, struct wl_surface *surface,
			       int32_t id, int32_t x, int32_t y)
{
	struct touch *touch = data;

	touch_paint(touch, x, y, id);
}

static void
input_device_handle_touch_up(void *data,
			     struct wl_input_device *wl_input_device,
			     uint32_t time, int32_t id)
{
}

static void
input_device_handle_touch_motion(void *data,
				 struct wl_input_device *wl_input_device,
				 uint32_t time,
				 int32_t id, int32_t x, int32_t y)
{
	struct touch *touch = data;

	touch_paint(touch, x, y, id);
}

static void
input_device_handle_touch_frame(void *data,
				struct wl_input_device *wl_input_device)
{
}

static void
input_device_handle_touch_cancel(void *data,
				 struct wl_input_device *wl_input_device)
{
}

static const struct wl_input_device_listener input_device_listener = {
	input_device_handle_motion,
	input_device_handle_button,
	input_device_handle_key,
	input_device_handle_pointer_focus,
	input_device_handle_keyboard_focus,
	input_device_handle_touch_down,
	input_device_handle_touch_up,
	input_device_handle_touch_motion,
	input_device_handle_touch_frame,
	input_device_handle_touch_cancel,
};

static void
handle_global(struct wl_display *display, uint32_t id,
	      const char *interface, uint32_t version, void *data)
{
	struct touch *touch = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		touch->compositor =
			wl_display_bind(display, id, &wl_compositor_interface);
	} else if (strcmp(interface, "wl_shell") == 0) {
		touch->shell =
			wl_display_bind(display, id, &wl_shell_interface);
	} else if (strcmp(interface, "wl_shm") == 0) {
		touch->shm = wl_display_bind(display, id, &wl_shm_interface);
		wl_shm_add_listener(touch->shm, &shm_listenter, touch);
	} else if (strcmp(interface, "wl_input_device") == 0) {
		touch->input_device =
			wl_display_bind(display, id,
					&wl_input_device_interface);
		wl_input_device_add_listener(touch->input_device,
					     &input_device_listener, touch);
	}
}

static int
event_mask_update(uint32_t mask, void *data)
{
	struct touch *touch = data;

	touch->mask = mask;

	return 0;
}

static struct touch *
touch_create(int width, int height)
{
	struct touch *touch;

	touch = malloc(sizeof *touch);
	touch->display = wl_display_connect(NULL);
	assert(touch->display);

	touch->has_argb = 0;
	wl_display_add_global_listener(touch->display, handle_global, touch);
	wl_display_iterate(touch->display, WL_DISPLAY_READABLE);
	wl_display_roundtrip(touch->display);

	if (!touch->has_argb) {
		fprintf(stderr, "WL_SHM_FORMAT_ARGB32 not available\n");
		exit(1);
	}

	wl_display_get_fd(touch->display, event_mask_update, touch);
	
	touch->width = width;
	touch->height = height;
	touch->surface = wl_compositor_create_surface(touch->compositor);
	touch->shell_surface = wl_shell_get_shell_surface(touch->shell,
							  touch->surface);
	create_shm_buffer(touch);

	wl_shell_surface_set_toplevel(touch->shell_surface);
	wl_surface_set_user_data(touch->surface, touch);

	memset(touch->data, 64, width * height * 4);
	wl_buffer_damage(touch->buffer, 0, 0, width, height);
	wl_surface_attach(touch->surface, touch->buffer, 0, 0);
	wl_surface_damage(touch->surface, 0, 0, width, height);

	return touch;
}

int
main(int argc, char **argv)
{
	struct touch *touch;

	touch = touch_create(600, 500);

	while (true)
		wl_display_iterate(touch->display, touch->mask);

	return 0;
}
