/*
 * Copyright © 2011 Benjamin Franzke
 * Copyright © 2010 Intel Corporation
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

struct display {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_shell *shell;
	struct wl_shm *shm;
	uint32_t mask;
};

struct window {
	struct display *display;
	int width, height;
	struct wl_surface *surface;
	struct wl_buffer *buffer;
	void *data;
};

static struct wl_buffer *
create_shm_buffer(struct display *display,
		  int width, int height, uint32_t format, void **data_out)
{
	char filename[] = "/tmp/wayland-shm-XXXXXX";
	struct wl_buffer *buffer;
	int fd, size, stride;
	void *data;

	fd = mkstemp(filename);
	if (fd < 0) {
		fprintf(stderr, "open %s failed: %m\n", filename);
		return NULL;
	}
	stride = width * 4;
	size = stride * height;
	if (ftruncate(fd, size) < 0) {
		fprintf(stderr, "ftruncate failed: %m\n");
		close(fd);
		return NULL;
	}

	data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	unlink(filename);

	if (data == MAP_FAILED) {
		fprintf(stderr, "mmap failed: %m\n");
		close(fd);
		return NULL;
	}

	buffer = wl_shm_create_buffer(display->shm, fd,
				      width, height, stride, format);

	close(fd);

	*data_out = data;

	return buffer;
}

static struct window *
create_window(struct display *display, int width, int height)
{
	struct window *window;
	
	window = malloc(sizeof *window);
	window->display = display;
	window->width = width;
	window->height = height;
	window->surface = wl_compositor_create_surface(display->compositor);
	window->buffer = create_shm_buffer(display,
					   width, height,
					   WL_SHM_FORMAT_XRGB32,
					   &window->data);

	wl_shell_set_toplevel(display->shell, window->surface);

	return window;
}

static const struct wl_callback_listener frame_listener;

static void
redraw(void *data, struct wl_callback *callback, uint32_t time)
{
	struct window *window = data;
	uint32_t *p;
	int i, end, offset;

	p = window->data;
	end = window->width * window->height;
	offset = time >> 4;
	for (i = 0; i < end; i++)
		p[i] = (i + offset) * 0x0080401;
	wl_buffer_damage(window->buffer, 0, 0, window->width, window->height);

	wl_surface_attach(window->surface, window->buffer, 0, 0);
	wl_surface_damage(window->surface,
			  0, 0, window->width, window->height);

	if (callback)
		wl_callback_destroy(callback);

	callback = wl_surface_frame(window->surface);
	wl_callback_add_listener(callback, &frame_listener, window);
}

static const struct wl_callback_listener frame_listener = {
	redraw
};

static void
display_handle_global(struct wl_display *display, uint32_t id,
		      const char *interface, uint32_t version, void *data)
{
	struct display *d = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor =
			wl_display_bind(display, id, &wl_compositor_interface);
	} else if (strcmp(interface, "wl_shell") == 0) {
		d->shell = wl_display_bind(display, id, &wl_shell_interface);
	} else if (strcmp(interface, "wl_shm") == 0) {
		d->shm = wl_display_bind(display, id, &wl_shm_interface);
	}
}

static int
event_mask_update(uint32_t mask, void *data)
{
	struct display *d = data;

	d->mask = mask;

	return 0;
}

static struct display *
create_display(void)
{
	struct display *display;

	display = malloc(sizeof *display);
	display->display = wl_display_connect(NULL);
	assert(display->display);

	wl_display_add_global_listener(display->display,
				       display_handle_global, display);
	wl_display_iterate(display->display, WL_DISPLAY_READABLE);

	wl_display_get_fd(display->display, event_mask_update, display);
	
	return display;
}

int
main(int argc, char **argv)
{
	struct display *display;
	struct window *window;

	display = create_display();
	window = create_window(display, 250, 250);

	redraw(window, NULL, 0);

	while (true)
		wl_display_iterate(display->display, display->mask);

	return 0;
}
