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
	struct wlsc_output *output;
	int tool, new_x, new_y;
	int base_x, base_y;
	int fd;
	int min_x, max_x, min_y, max_y;
	int is_touchpad, old_x_value, old_y_value, reset_x_value, reset_y_value;
};

static inline void
evdev_process_key(struct evdev_input_device *device,
                        struct input_event *e, int value, int time)
{
	switch (e->code) {
	case BTN_TOOL_PEN:
	case BTN_TOOL_RUBBER:
	case BTN_TOOL_BRUSH:
	case BTN_TOOL_PENCIL:
	case BTN_TOOL_AIRBRUSH:
	case BTN_TOOL_FINGER:
	case BTN_TOOL_MOUSE:
	case BTN_TOOL_LENS:
		device->tool = value ? e->code : 0;
		if (device->is_touchpad)
		{
			device->reset_x_value = 1;
			device->reset_y_value = 1;
		}
		break;

	case BTN_TOUCH:
		/* Treat BTN_TOUCH from devices that only have BTN_TOUCH as
		 * BTN_LEFT */
		e->code = BTN_LEFT;
		/* Intentional fallthrough! */
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

static inline void
evdev_process_absolute_motion(struct evdev_input_device *device,
			struct input_event *e, int value, int *x, int *y,
			int *absolute_event)
{
	const int screen_width = device->output->current->width;
	const int screen_height = device->output->current->height;

	switch (e->code) {
	case ABS_X:
		*absolute_event = device->tool;
		*x = (value - device->min_x) * screen_width /
			(device->max_x - device->min_x) + device->output->x;
		break;
	case ABS_Y:
		*absolute_event = device->tool;
		*y = (value - device->min_y) * screen_height /
			(device->max_y - device->min_y) + device->output->y;
		break;
	}
}

static inline void
evdev_process_absolute_motion_touchpad(struct evdev_input_device *device,
			struct input_event *e, int value, int *dx, int *dy)
{
	/* FIXME: Make this configurable somehow. */
	const int touchpad_speed = 700;

	switch (e->code) {
	case ABS_X:
		value -= device->min_x;
		if (device->reset_x_value)
			device->reset_x_value = 0;
		else {
			*dx = (value - device->old_x_value) * touchpad_speed /
				(device->max_x - device->min_x);
		}
		device->old_x_value = value;
		break;
	case ABS_Y:
		value -= device->min_y;
		if (device->reset_y_value)
			device->reset_y_value = 0;
		else {
			*dy = (value - device->old_y_value) * touchpad_speed /
				/* maybe use x size here to have the same scale? */
				(device->max_y - device->min_y);
		}
		device->old_y_value = value;
		break;
	}
}

static inline void
evdev_process_relative_motion(struct input_event *e, int value, int *dx,
			int *dy)
{
	switch (e->code) {
	case REL_X:
		*dx += value;
		break;
	case REL_Y:
		*dy += value;
		break;
	}
}

static int
evdev_input_device_data(int fd, uint32_t mask, void *data)
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
			evdev_process_relative_motion(e, value, &dx, &dy);
			break;
		case EV_ABS:
			if (device->is_touchpad)
				evdev_process_absolute_motion_touchpad(device,
					e, value, &dx, &dy);
			else
				evdev_process_absolute_motion(device, e, value,
					&x, &y, &absolute_event);
			break;
		case EV_KEY:
			if (value == 2)
				break;
			evdev_process_key(device, e, value, time);
		}
	}

	if (dx != 0 || dy != 0)
		notify_motion(&device->master->base.input_device,
			      time, x + dx, y + dy);
	if (absolute_event)
		notify_motion(&device->master->base.input_device, time, x, y);

	return 1;
}


/* copied from udev/extras/input_id/input_id.c */
/* we must use this kernel-compatible implementation */
#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define TEST_BIT(array, bit)    ((array[LONG(bit)] >> OFF(bit)) & 1)
/* end copied */

static int
evdev_configure_device(struct evdev_input_device *device)
{
	struct input_absinfo absinfo;
	unsigned long ev_bits[NBITS(EV_MAX)];
	unsigned long abs_bits[NBITS(ABS_MAX)];
	unsigned long key_bits[NBITS(KEY_MAX)];
	int has_key, has_abs;

	has_key = 0;
	has_abs = 0;

	ioctl(device->fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits);
	if (TEST_BIT(ev_bits, EV_ABS)) {
		has_abs = 1;
		ioctl(device->fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)),
		      abs_bits);
		if (TEST_BIT(abs_bits, ABS_X)) {
			ioctl(device->fd, EVIOCGABS(ABS_X), &absinfo);
			device->min_x = absinfo.minimum;
			device->max_x = absinfo.maximum;
		}
		if (TEST_BIT(abs_bits, ABS_Y)) {
			ioctl(device->fd, EVIOCGABS(ABS_Y), &absinfo);
			device->min_y = absinfo.minimum;
			device->max_y = absinfo.maximum;
		}
	}
	if (TEST_BIT(ev_bits, EV_KEY)) {
		has_key = 1;
		ioctl(device->fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)),
		      key_bits);
		if (TEST_BIT(key_bits, BTN_TOOL_FINGER) &&
		             !TEST_BIT(key_bits, BTN_TOOL_PEN))
			device->is_touchpad = 1;
	}

	/* This rule tries to catch accelerometer devices and opt out. We may
	 * want to adjust the protocol later adding a proper event for dealing
	 * with accelerometers and implement here accordingly */
	if (has_abs && !has_key)
		return -1;

	return 0;
}

static struct evdev_input_device *
evdev_input_device_create(struct evdev_input *master,
			  struct wl_display *display, const char *path)
{
	struct evdev_input_device *device;
	struct wl_event_loop *loop;
	struct wlsc_compositor *ec;

	device = malloc(sizeof *device);
	if (device == NULL)
		return NULL;

	ec = (struct wlsc_compositor *) master->base.input_device.compositor;
	device->output = 
		container_of(ec->output_list.next, struct wlsc_output, link);

	device->tool = 1;
	device->new_x = 1;
	device->new_y = 1;
	device->master = master;
	device->is_touchpad = 0;

	device->fd = open(path, O_RDONLY);
	if (device->fd < 0) {
		free(device);
		fprintf(stderr, "couldn't create pointer for %s: %m\n", path);
		return NULL;
	}

	if (evdev_configure_device(device) == -1) {
		close(device->fd);
		free(device);
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

static const char default_seat[] = "seat0";

void
evdev_input_add_devices(struct wlsc_compositor *c,
			struct udev *udev, const char *seat)
{
	struct evdev_input *input;
	struct udev_enumerate *e;
	struct udev_list_entry *entry;
	struct udev_device *device;
	const char *device_seat;
	const char *path;

	input = malloc(sizeof *input);
	if (input == NULL)
		return;

	memset(input, 0, sizeof *input);
	wlsc_input_device_init(&input->base, c);

	e = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(e, "input");
	udev_enumerate_scan_devices(e);
	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		path = udev_list_entry_get_name(entry);
		device = udev_device_new_from_syspath(udev, path);

		if (strncmp("event", udev_device_get_sysname(device), 5) != 0)
			continue;

                device_seat =
			udev_device_get_property_value(device, "ID_SEAT");
		if (!device_seat)
			device_seat = default_seat;

		if (strcmp(device_seat, seat) == 0) {
			evdev_input_device_create(input, c->wl_display,
						  udev_device_get_devnode(device));
		}

		udev_device_unref(device);
	}
	udev_enumerate_unref(e);

	c->input_device = &input->base.input_device;
}
