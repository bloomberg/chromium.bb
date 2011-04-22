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
	int min_x, max_x, min_y, max_y;
};

static int
evdev_input_device_data(int fd, uint32_t mask, void *data)
{
	struct wlsc_compositor *ec;
	struct evdev_input_device *device = data;
	struct input_event ev[8], *e, *end;
	int len, value, dx, dy, absolute_event;
	int x, y;
	uint32_t time;

	/* FIXME: Obviously we need to not hardcode these here, but
	 * instead get the values from the output it's associated with. */
	const int screen_width = 1024, screen_height = 600;

	ec = (struct wlsc_compositor *)
		device->master->base.input_device.compositor;
	if (!ec->focus)
		return 1;

	dx = 0;
	dy = 0;
	absolute_event = 0;
	x = device->master->base.input_device.x;
	y = device->master->base.input_device.y;

	len = read(fd, &ev, sizeof ev);
	if (len < 0 || len % sizeof e[0] != 0) {
		/* FIXME: handle error... reopen device? */;
		return 1;
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
			switch (e->code) {
			case ABS_X:
				absolute_event = device->tool;
				x = (value - device->min_x) * screen_width /
					(device->max_x - device->min_x);
				break;
			case ABS_Y:
				absolute_event = device->tool;
				y = (value - device->min_y) * screen_height /
					(device->max_y - device->min_y);
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
	if (absolute_event)
		notify_motion(&device->master->base.input_device, time, x, y);

	return 1;
}

#define TEST_BIT(b, i) (b[(i) / 32] & (1 << (i & 31)))

static struct evdev_input_device *
evdev_input_device_create(struct evdev_input *master,
			  struct wl_display *display, const char *path)
{
	struct evdev_input_device *device;
	struct wl_event_loop *loop;
	struct input_absinfo absinfo;
	uint32_t ev_bits[EV_MAX];
	uint32_t key_bits[KEY_MAX];

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

	ioctl(device->fd, EVIOCGBIT(0, EV_MAX), ev_bits);
	if (TEST_BIT(ev_bits, EV_ABS)) {
		ioctl(device->fd, EVIOCGBIT(EV_ABS, EV_MAX), key_bits);
		if (TEST_BIT(key_bits, ABS_X)) {
			ioctl(device->fd, EVIOCGABS(ABS_X), &absinfo);
			device->min_x = absinfo.minimum;
			device->max_x = absinfo.maximum;
		}
		if (TEST_BIT(key_bits, ABS_Y)) {
			ioctl(device->fd, EVIOCGABS(ABS_Y), &absinfo);
			device->min_y = absinfo.minimum;
			device->max_y = absinfo.maximum;
		}
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
