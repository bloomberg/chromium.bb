/*
 * Copyright © 2012 Intel Corporation
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

#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>
#include <math.h>
#include <assert.h>

#include <linux/input.h>
#include <wayland-client.h>

#include "window.h"
#include "../shared/matrix.h"

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

/* Our points for the calibration must be not be on a line */
static const struct {
	float x_ratio, y_ratio;
} test_ratios[] =  {
	{ 0.20, 0.40 },
	{ 0.80, 0.60 },
	{ 0.40, 0.80 }
};

struct calibrator {
	struct tests {
		int32_t drawn_x, drawn_y;
		int32_t clicked_x, clicked_y;
	} tests[ARRAY_LENGTH(test_ratios)];
	int current_test;

	struct display *display;
	struct window *window;
	struct widget *widget;
};

/*
 * Calibration algorithm:
 *
 * The equation we want to apply at event time where x' and y' are the
 * calibrated co-ordinates.
 *
 * x' = Ax + By + C
 * y' = Dx + Ey + F
 *
 * For example "zero calibration" would be A=1.0 B=0.0 C=0.0, D=0.0, E=1.0,
 * and F=0.0.
 *
 * With 6 unknowns we need 6 equations to find the constants:
 *
 * x1' = Ax1 + By1 + C
 * y1' = Dx1 + Ey1 + F
 * ...
 * x3' = Ax3 + By3 + C
 * y3' = Dx3 + Ey3 + F
 *
 * In matrix form:
 *
 * x1'   x1 y1 1      A
 * x2' = x2 y2 1  x   B
 * x3'   x3 y3 1      C
 *
 * So making the matrix M we can find the constants with:
 *
 * A            x1'
 * B = M^-1  x  x2'
 * C            x3'
 *
 * (and similarly for D, E and F)
 *
 * For the calibration the desired values x, y are the same values at which
 * we've drawn at.
 *
 */
static void
finish_calibration (struct calibrator *calibrator)
{
	struct weston_matrix m;
	struct weston_matrix inverse;
	struct weston_vector x_calib, y_calib;
	int i;


	/*
	 * x1 y1  1  0
	 * x2 y2  1  0
	 * x3 y3  1  0
	 *  0  0  0  1
	 */
	memset(&m, 0, sizeof(m));
	for (i = 0; i < (int)ARRAY_LENGTH(test_ratios); i++) {
		m.d[i] = calibrator->tests[i].clicked_x;
		m.d[i + 4] = calibrator->tests[i].clicked_y;
		m.d[i + 8] = 1;
	}
	m.d[15] = 1;

	weston_matrix_invert(&inverse, &m);

	memset(&x_calib, 0, sizeof(x_calib));
	memset(&y_calib, 0, sizeof(y_calib));

	for (i = 0; i < (int)ARRAY_LENGTH(test_ratios); i++) {
		x_calib.f[i] = calibrator->tests[i].drawn_x;
		y_calib.f[i] = calibrator->tests[i].drawn_y;
	}

	/* Multiples into the vector */
	weston_matrix_transform(&inverse, &x_calib);
	weston_matrix_transform(&inverse, &y_calib);

	printf ("Calibration values: %f %f %f %f %f %f\n",
		x_calib.f[0], x_calib.f[1], x_calib.f[2],
		y_calib.f[0], y_calib.f[1], y_calib.f[2]);

	exit(0);
}


static void
button_handler(struct widget *widget,
	       struct input *input, uint32_t time,
	       uint32_t button,
	       enum wl_pointer_button_state state, void *data)
{
	struct calibrator *calibrator = data;
	int32_t x, y;

	if (state == WL_POINTER_BUTTON_STATE_PRESSED && button == BTN_LEFT) {
		input_get_position(input, &x, &y);
		calibrator->tests[calibrator->current_test].clicked_x = x;
		calibrator->tests[calibrator->current_test].clicked_y = y;

		calibrator->current_test--;
		if (calibrator->current_test < 0)
			finish_calibration(calibrator);
	}

	widget_schedule_redraw(widget);
}

static void
touch_handler(struct widget *widget, struct input *input, uint32_t serial,
	      uint32_t time, int32_t id, float x, float y, void *data)
{
	struct calibrator *calibrator = data;

	calibrator->tests[calibrator->current_test].clicked_x = x;
	calibrator->tests[calibrator->current_test].clicked_y = y;
	calibrator->current_test--;

	if (calibrator->current_test < 0)
		  finish_calibration(calibrator);

	widget_schedule_redraw(widget);
}

static void
redraw_handler(struct widget *widget, void *data)
{
	struct calibrator *calibrator = data;
	struct rectangle allocation;
	cairo_surface_t *surface;
	cairo_t *cr;
	int32_t drawn_x, drawn_y;

	widget_get_allocation(calibrator->widget, &allocation);
	surface = window_get_surface(calibrator->window);

	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
	cairo_paint(cr);

	drawn_x = test_ratios[calibrator->current_test].x_ratio * allocation.width;
	drawn_y = test_ratios[calibrator->current_test].y_ratio * allocation.height;

	calibrator->tests[calibrator->current_test].drawn_x = drawn_x;
	calibrator->tests[calibrator->current_test].drawn_y = drawn_y;

	cairo_translate(cr, drawn_x, drawn_y);
	cairo_set_line_width(cr, 2.0);
	cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
	cairo_move_to(cr, 0, -10.0);
	cairo_line_to(cr, 0, 10.0);
	cairo_stroke(cr);
	cairo_move_to(cr, -10.0, 0);
	cairo_line_to(cr, 10.0, 0.0);
	cairo_stroke(cr);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

static struct calibrator *
calibrator_create(struct display *display)
{
	struct calibrator *calibrator;

	calibrator = malloc(sizeof *calibrator);
	if (calibrator == NULL)
		return NULL;

	calibrator->window = window_create(display);
	calibrator->widget = window_add_widget(calibrator->window, calibrator);
	window_set_title(calibrator->window, "Wayland calibrator");
	calibrator->display = display;

	calibrator->current_test = ARRAY_LENGTH(test_ratios) - 1;

	widget_set_button_handler(calibrator->widget, button_handler);
	widget_set_touch_down_handler(calibrator->widget, touch_handler);
	widget_set_redraw_handler(calibrator->widget, redraw_handler);

	window_set_fullscreen(calibrator->window, 1);

	return calibrator;
}

static void
calibrator_destroy(struct calibrator *calibrator)
{
	widget_destroy(calibrator->widget);
	window_destroy(calibrator->window);
	free(calibrator);
}


int
main(int argc, char *argv[])
{
	struct display *display;
	struct calibrator *calibrator;

	display = display_create(&argc, argv);

	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	calibrator = calibrator_create(display);

	if (!calibrator)
		return -1;

	display_run(display);

	calibrator_destroy(calibrator);
	display_destroy(display);

	return 0;
}

