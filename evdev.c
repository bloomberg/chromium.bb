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

#include <stdio.h>
#include <stdlib.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/input.h>

#include "wayland.h"

struct wl_input_device {
	struct wl_object base;
	struct wl_event_source *source;
	struct wl_display *display;
	int fd;
	int tool, new_x, new_y;
	int32_t x, y, base_x, base_y;
};

static const struct wl_method input_device_methods[] = {
};

static const struct wl_event input_device_events[] = {
	{ "motion", "ii" },
	{ "button", "uu" },
	{ "key", "uu" },
};

static const struct wl_interface input_device_interface = {
	"input_device", 1,
	ARRAY_LENGTH(input_device_methods),
	input_device_methods,
	ARRAY_LENGTH(input_device_events),
	input_device_events,
};

static void wl_input_device_data(int fd, uint32_t mask, void *data)
{
	struct wl_input_device *device = data;
	struct input_event ev[8], *e, *end;
	int len, value, dx, dy, absolute_event;

	dx = 0;
	dy = 0;
	absolute_event = 0;

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
					device->base_x = device->x - value;
					device->new_x = 0;
				}
				device->x = device->base_x + value;
				break;
			case ABS_Y:
				if (device->new_y) {
					device->base_y = device->y - value;
					device->new_y = 0;
				}
				device->y = device->base_y + value;
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
				wl_display_post_button_event(device->display,
							     &device->base, 0, value);
				break;

			case BTN_RIGHT:
				wl_display_post_button_event(device->display,
							     &device->base, 2, value);
				break;

			case BTN_MIDDLE:
				wl_display_post_button_event(device->display,
							     &device->base, 1, value);
				break;

			default:
				wl_display_post_key_event(device->display,
							  &device->base, e->code, value);
				break;
			}
		}
	}

	if (dx != 0 || dy != 0)
		wl_display_post_relative_event(device->display,
					       &device->base, dx, dy);
	if (absolute_event && device->tool)
		wl_display_post_absolute_event(device->display,
					       &device->base,
					       device->x, device->y);
}

WL_EXPORT struct wl_object *
wl_input_device_create(struct wl_display *display, const char *path)
{
	struct wl_input_device *device;
	struct wl_event_loop *loop;

	device = malloc(sizeof *device);
	if (device == NULL)
		return NULL;

	device->base.interface = &input_device_interface;
	device->display = display;
	device->tool = 1;
	device->x = 100;
	device->y = 100;
	device->new_x = 1;
	device->new_y = 1;

	device->fd = open(path, O_RDONLY);
	if (device->fd < 0) {
		free(device);
		fprintf(stderr, "couldn't create pointer for %s: %m\n", path);
		return NULL;
	}

	loop = wl_display_get_event_loop(display);
	device->source = wl_event_loop_add_fd(loop, device->fd,
					      WL_EVENT_READABLE,
					      wl_input_device_data, device);
	if (device->source == NULL) {
		close(device->fd);
		free(device);
		return NULL;
	}

	return &device->base;
}
