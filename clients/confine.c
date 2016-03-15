/*
 * Copyright © 2010 Intel Corporation
 * Copyright © 2012 Collabora, Ltd.
 * Copyright © 2012 Jonas Ådahl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>
#include <math.h>
#include <assert.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <linux/input.h>
#include <wayland-client.h>

#include "window.h"
#include "shared/helpers.h"
#include "shared/xalloc.h"

struct confine {
	struct display *display;
	struct window *window;
	struct widget *widget;

	cairo_surface_t *buffer;

	struct {
		int32_t x, y;
		int32_t old_x, old_y;
	} line;

	int reset;

	struct input *cursor_timeout_input;
	int cursor_timeout_fd;
	struct task cursor_timeout_task;
};

static void
draw_line(struct confine *confine, cairo_t *cr,
	  struct rectangle *allocation)
{
	cairo_t *bcr;
	cairo_surface_t *tmp_buffer = NULL;

	if (confine->reset) {
		tmp_buffer = confine->buffer;
		confine->buffer = NULL;
		confine->line.x = -1;
		confine->line.y = -1;
		confine->line.old_x = -1;
		confine->line.old_y = -1;
		confine->reset = 0;
	}

	if (confine->buffer == NULL) {
		confine->buffer =
			cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						   allocation->width,
						   allocation->height);
		bcr = cairo_create(confine->buffer);
		cairo_set_source_rgba(bcr, 0, 0, 0, 0);
		cairo_rectangle(bcr,
				0, 0,
				allocation->width, allocation->height);
		cairo_fill(bcr);
	}
	else
		bcr = cairo_create(confine->buffer);

	if (tmp_buffer) {
		cairo_set_source_surface(bcr, tmp_buffer, 0, 0);
		cairo_rectangle(bcr, 0, 0,
				allocation->width, allocation->height);
		cairo_clip(bcr);
		cairo_paint(bcr);

		cairo_surface_destroy(tmp_buffer);
	}

	if (confine->line.x != -1 && confine->line.y != -1) {
		if (confine->line.old_x != -1 &&
		    confine->line.old_y != -1) {
			cairo_set_line_width(bcr, 2.0);
			cairo_set_source_rgb(bcr, 1, 1, 1);
			cairo_translate(bcr,
					-allocation->x, -allocation->y);

			cairo_move_to(bcr,
				      confine->line.old_x,
				      confine->line.old_y);
			cairo_line_to(bcr,
				      confine->line.x,
				      confine->line.y);

			cairo_stroke(bcr);
		}

		confine->line.old_x = confine->line.x;
		confine->line.old_y = confine->line.y;
	}
	cairo_destroy(bcr);

	cairo_set_source_surface(cr, confine->buffer,
				 allocation->x, allocation->y);
	cairo_set_operator(cr, CAIRO_OPERATOR_ADD);
	cairo_rectangle(cr,
			allocation->x, allocation->y,
			allocation->width, allocation->height);
	cairo_clip(cr);
	cairo_paint(cr);
}

static void
redraw_handler(struct widget *widget, void *data)
{
	struct confine *confine = data;
	cairo_surface_t *surface;
	cairo_t *cr;
	struct rectangle allocation;

	widget_get_allocation(confine->widget, &allocation);

	surface = window_get_surface(confine->window);

	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_rectangle(cr,
			allocation.x,
			allocation.y,
			allocation.width,
			allocation.height);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.8);
	cairo_fill(cr);

	draw_line(confine, cr, &allocation);

	cairo_destroy(cr);

	cairo_surface_destroy(surface);
}

static void
keyboard_focus_handler(struct window *window,
		       struct input *device, void *data)
{
	struct confine *confine = data;

	window_schedule_redraw(confine->window);
}

static void
key_handler(struct window *window, struct input *input, uint32_t time,
	    uint32_t key, uint32_t sym,
	    enum wl_keyboard_key_state state, void *data)
{
	struct confine *confine = data;

	if (state == WL_KEYBOARD_KEY_STATE_RELEASED)
		return;

	switch (sym) {
	case XKB_KEY_Escape:
		display_exit(confine->display);
		break;
	}
}

static void
cursor_timeout_reset(struct confine *confine)
{
	const long cursor_timeout = 500;
	struct itimerspec its;

	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;
	its.it_value.tv_sec = cursor_timeout / 1000;
	its.it_value.tv_nsec = (cursor_timeout % 1000) * 1000 * 1000;
	timerfd_settime(confine->cursor_timeout_fd, 0, &its, NULL);
}

static int
motion_handler(struct widget *widget,
	       struct input *input, uint32_t time,
	       float x, float y, void *data)
{
	struct confine *confine = data;
	confine->line.x = x;
	confine->line.y = y;

	window_schedule_redraw(confine->window);

	cursor_timeout_reset(confine);
	confine->cursor_timeout_input = input;

	return CURSOR_BLANK;
}

static void
resize_handler(struct widget *widget,
	       int32_t width, int32_t height,
	       void *data)
{
	struct confine *confine = data;

	confine->reset = 1;
}

static void
leave_handler(struct widget *widget,
	      struct input *input, void *data)
{
	struct confine *confine = data;

	confine->reset = 1;
}

static void
cursor_timeout_func(struct task *task, uint32_t events)
{
	struct confine *confine =
		container_of(task, struct confine, cursor_timeout_task);
	uint64_t exp;

	if (read(confine->cursor_timeout_fd, &exp, sizeof (uint64_t)) !=
	    sizeof(uint64_t))
		abort();

	input_set_pointer_image(confine->cursor_timeout_input,
				CURSOR_LEFT_PTR);
}

static struct confine *
confine_create(struct display *display)
{
	struct confine *confine;

	confine = xzalloc(sizeof *confine);
	confine->window = window_create(display);
	confine->widget = window_frame_create(confine->window, confine);
	window_set_title(confine->window, "Wayland Confine");
	confine->display = display;
	confine->buffer = NULL;

	window_set_key_handler(confine->window, key_handler);
	window_set_user_data(confine->window, confine);
	window_set_keyboard_focus_handler(confine->window,
					  keyboard_focus_handler);

	widget_set_redraw_handler(confine->widget, redraw_handler);
	widget_set_motion_handler(confine->widget, motion_handler);
	widget_set_resize_handler(confine->widget, resize_handler);
	widget_set_leave_handler(confine->widget, leave_handler);

	widget_schedule_resize(confine->widget, 500, 400);
	confine->line.x = -1;
	confine->line.y = -1;
	confine->line.old_x = -1;
	confine->line.old_y = -1;
	confine->reset = 0;

	confine->cursor_timeout_fd =
		timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	confine->cursor_timeout_task.run = cursor_timeout_func;
	display_watch_fd(window_get_display(confine->window),
			 confine->cursor_timeout_fd,
			 EPOLLIN, &confine->cursor_timeout_task);

	return confine;
}

static void
confine_destroy(struct confine *confine)
{
	display_unwatch_fd(window_get_display(confine->window),
			   confine->cursor_timeout_fd);
	close(confine->cursor_timeout_fd);
	if (confine->buffer)
		cairo_surface_destroy(confine->buffer);
	widget_destroy(confine->widget);
	window_destroy(confine->window);
	free(confine);
}

int
main(int argc, char *argv[])
{
	struct display *display;
	struct confine *confine;

	display = display_create(&argc, argv);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	confine = confine_create(display);

	display_run(display);

	confine_destroy(confine);
	display_destroy(display);

	return 0;
}
