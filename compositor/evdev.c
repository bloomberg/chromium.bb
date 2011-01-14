/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h>

#include "compositor.h"

struct evdev_input {
	struct wlsc_input_device base;
};

struct evdev_input_device {
	struct evdev_input *master;
	struct wl_event_source *source;
	int tool, new_x, new_y;
	int base_x, base_y;
	int fd;
};

static void evdev_input_device_data(int fd, uint32_t mask, void *data)
{
	struct wlsc_compositor *ec;
	struct evdev_input_device *device = data;
	struct input_event ev[8], *e, *end;
	int len, value, dx, dy, absolute_event;
	int x, y;
	uint32_t time;

	ec = (struct wlsc_compositor *)
		device->master->base.input_device.compositor;
	if (!ec->focus)
		return;

	dx = 0;
	dy = 0;
	absolute_event = 0;
	x = device->master->base.input_device.x;
	y = device->master->base.input_device.y;

	len = read(fd, &ev, sizeof ev);
	if (len < 0 || len % sizeof e[0] != 0) {
		/* FIXME: handle error... reopen device? */;
		return;
	}

	e = ev;
	end = (void *) ev + len;
	for (e = ev; e < end; e++) {
		/* Get the signed value, earlier kernels had this as unsigned */
		value = e->value;
		time = e->time.tv_sec * 1000 + e->time.tv_usec / 1000;

		switch (e->type) {
		case EV_REL:
			switch (e->code) {
			case REL_X:
				dx += value;
				break;

			case REL_Y:
				dy += value;
				break;
			}
			break;

		case EV_ABS:
			absolute_event = 1;
			switch (e->code) {
			case ABS_X:
				if (device->new_x) {
					device->base_x = x - value;
					device->new_x = 0;
				}
				x = device->base_x + value;
				break;
			case ABS_Y:
				if (device->new_y) {
					device->base_y = y - value;
					device->new_y = 0;
				}
				y = device->base_y + value;
				break;
			}
			break;

		case EV_KEY:
			if (value == 2)
				break;

			switch (e->code) {
			case BTN_TOUCH:
			case BTN_TOOL_PEN:
			case BTN_TOOL_RUBBER:
			case BTN_TOOL_BRUSH:
			case BTN_TOOL_PENCIL:
			case BTN_TOOL_AIRBRUSH:
			case BTN_TOOL_FINGER:
			case BTN_TOOL_MOUSE:
			case BTN_TOOL_LENS:
				if (device->tool == 0 && value) {
					device->new_x = 1;
					device->new_y = 1;
				}
				device->tool = value ? e->code : 0;
				break;

			case BTN_LEFT:
			case BTN_RIGHT:
			case BTN_MIDDLE:
			case BTN_SIDE:
			case BTN_EXTRA:
			case BTN_FORWARD:
			case BTN_BACK:
			case BTN_TASK:
				notify_button(&device->master->base.input_device,
					      time, e->code, value);
				break;

			default:
				notify_key(&device->master->base.input_device,
					   time, e->code, value);
				break;
			}
		}
	}

	if (dx != 0 || dy != 0)
		notify_motion(&device->master->base.input_device,
			      time, x + dx, y + dy);
	if (absolute_event && device->tool)
		notify_motion(&device->master->base.input_device, time, x, y);
}

static struct evdev_input_device *
evdev_input_device_create(struct evdev_input *master,
			  struct wl_display *display, const char *path)
{
	struct evdev_input_device *device;
	struct wl_event_loop *loop;

	device = malloc(sizeof *device);
	if (device == NULL)
		return NULL;

	device->tool = 1;
	device->new_x = 1;
	device->new_y = 1;
	device->master = master;

	device->fd = open(path, O_RDONLY);
	if (device->fd < 0) {
		free(device);
		fprintf(stderr, "couldn't create pointer for %s: %m\n", path);
		return NULL;
	}

	loop = wl_display_get_event_loop(display);
	device->source = wl_event_loop_add_fd(loop, device->fd,
					      WL_EVENT_READABLE,
					      evdev_input_device_data, device);
	if (device->source == NULL) {
		close(device->fd);
		free(device);
		return NULL;
	}

	return device;
}

void
evdev_input_add_devices(struct wlsc_compositor *c, struct udev *udev)
{
	struct evdev_input *input;
	struct udev_enumerate *e;
	struct udev_list_entry *entry;
	struct udev_device *device;
	const char *path;

	input = malloc(sizeof *input);
	if (input == NULL)
		return;

	memset(input, 0, sizeof *input);
	wlsc_input_device_init(&input->base, c);

	e = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(e, "input");
	udev_enumerate_add_match_property(e, "WAYLAND_SEAT", "1");
	udev_enumerate_scan_devices(e);
	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		path = udev_list_entry_get_name(entry);
		device = udev_device_new_from_syspath(udev, path);
		evdev_input_device_create(input, c->wl_display,
					  udev_device_get_devnode(device));
	}
	udev_enumerate_unref(e);

	c->input_device = &input->base.input_device;
}
