/*
 * Copyright © 2011 Tim Wiederhake
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

/**
 * \file eventdemo.c
 * \brief Demonstrate the use of Wayland's toytoolkit.
 *
 * Heavily commented demo program that can report all events that are
 * dispatched to the window. For other functionality, eg. opengl/egl,
 * drag and drop, etc. have a look at the other demos.
 * \author Tim Wiederhake
 */

#include <stdio.h>
#include <stdlib.h>

#include <cairo.h>
#include <glib.h>

#include "window.h"

/** window title */
static char *title = "EventDemo";

/** window width */
static int width = 500;

/** window height */
static int height = 400;

/** set if window has no borders */
static int noborder = 0;

/** if non-zero, maximum window width */
static int width_max = 0;

/** if non-zero, maximum window height */
static int height_max = 0;

/** set to log redrawing */
static int log_redraw = 0;

/** set to log resizing */
static int log_resize = 0;

/** set to log keyboard focus */
static int log_focus = 0;

/** set to log key events */
static int log_key = 0;

/** set to log button events */
static int log_button = 0;

/** set to log motion events */
static int log_motion = 0;

/**
 * \struct eventdemo
 * \brief Holds all data the program needs per window
 *
 * In this demo the struct holds the position of a
 * red rectangle that is drawn in the window's area.
 */
struct eventdemo {
	struct window *window;
	struct display *display;

	unsigned int x, y, w, h;
};

/**
 * \brief Redraws the window
 *
 * Draws a red rectangle as demonstration of per-window data.
 */
static void
eventdemo_draw(struct eventdemo *e) {
	if (log_redraw)
		printf("redraw\n");

	cairo_surface_t *surface;
	cairo_t *cr;
	struct rectangle rect;

	window_draw(e->window);
	window_get_child_allocation(e->window, &rect);
	surface = window_get_surface(e->window);

	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

	cairo_rectangle(cr, rect.x, rect.y, rect.width, rect.height);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.8);
	cairo_fill(cr);

	cairo_rectangle(cr, e->x, e->y, e->w, e->h);
	cairo_set_source_rgba(cr, 1.0, 0, 0, 1);
	cairo_fill(cr);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	window_flush(e->window);
}

/**
 * \brief CALLBACK function, Wayland requests the window to redraw.
 * \param window window to be redrawn
 * \param data user data associated to the window
 */
static void
redraw_handler(struct window *window, void *data)
{
	struct eventdemo *e = data;
	eventdemo_draw(e);
}

/**
 * \brief CALLBACK function, Wayland requests the window to resize.
 * \param window window to be resized
 * \param width desired width
 * \param height desired height
 * \param data user data associated to the window
 */

static void
resize_handler(struct window *window,
	       int32_t width, int32_t height, void *data)
{
	struct eventdemo *e = data;
	if (log_resize)
		printf("resize width: %d, height: %d\n", width, height);

	/* if a maximum width is set, constrain to it */
	if (width_max && width_max < width)
		width = width_max;

	/* if a maximum height is set, constrain to it */
	if (height_max && height_max < height)
		height = height_max;

	/* set the new window dimensions */
	window_set_child_size(e->window, width, height);

	/* inform Wayland that the window needs to be redrawn */
	window_schedule_redraw(e->window);
}

/**
 * \brief CALLBACK function, Wayland informs about keyboard focus change
 * \param window window
 * \param device device that caused the focus change
 * \param data user data associated to the window
 */
static void
keyboard_focus_handler(struct window *window,
		       struct input *device, void *data)
{
	int32_t x, y;
	struct eventdemo *e = data;

	if(log_focus) {
		if(device) {
			input_get_position(device, &x, &y);
			printf("focus x: %d, y: %d\n", x, y);
		} else {
			printf("focus lost\n");
		}
	}

	window_schedule_redraw(e->window);
}

/**
 * \brief CALLBACK function, Wayland informs about key event
 * \param window window
 * \param key keycode
 * \param unicode associated character
 * \param state pressed or released
 * \param modifiers modifiers: ctrl, alt, meta etc.
 * \param data user data associated to the window
 */
static void
key_handler(struct window *window, struct input *input, uint32_t time,
            uint32_t key, uint32_t unicode, uint32_t state, void *data)
{
	uint32_t modifiers = input_get_modifiers(input);

	if(!log_key)
		return;

	printf("key key: %d, unicode: %d, state: %d, modifiers: %d\n",
	       key, unicode, state, modifiers);
}

/**
 * \brief CALLBACK function, Wayland informs about button event
 * \param window window
 * \param input input device that caused the button event
 * \param time time the event happend
 * \param button button
 * \param state pressed or released
 * \param data user data associated to the window
 */
static void
button_handler(struct window *window, struct input *input, uint32_t time,
	       int button, int state, void *data)
{
	int32_t x, y;

	if (!log_button)
		return;

	input_get_position(input, &x, &y);
	printf("button time: %d, button: %d, state: %d, x: %d, y: %d\n",
	       time, button, state, x, y);
}

/**
 * \brief CALLBACK function, Waylands informs about pointer motion
 * \param window window
 * \param input input device that caused the motion event
 * \param time time the event happend
 * \param x absolute x position
 * \param y absolute y position
 * \param sx x position relative to the window
 * \param sy y position relative to the window
 * \param data user data associated to the window
 *
 * Demonstrates the use of different cursors
 */
static int
motion_handler(struct window *window, struct input *input, uint32_t time,
	       int32_t x, int32_t y, int32_t sx, int32_t sy, void *data)
{
	struct eventdemo *e = data;

	if (log_motion) {
		printf("motion time: %d, x: %d, y: %d, sx: %d, sy: %d\n",
		       time, x, y, sx, sy);
	}

	if(sx > e->x && sx < e->x + e->w)
		if(sy > e->y && sy < e->y + e->h)
			return POINTER_HAND1;

	return POINTER_LEFT_PTR;
}

/**
 * \brief Create and initialise a new eventdemo window.
 * \param d associated display
 */
static struct eventdemo *
eventdemo_create(struct display *d)
{
	struct eventdemo *e;

	e = malloc(sizeof (struct eventdemo));
	if(e == NULL)
		return NULL;

	/* Creates a new window with the given title, width and height.
	 * To set the size of the window for a given usable areas width
	 * and height in a window decoration agnostic way use:
	 * 	window_set_child_size(struct window *window,
	 *			      int32_t width, int32_t height);
	 */
	e->window = window_create(d, width, height);
	window_set_title(e->window, title);
	e->display = d;

	/* The eventdemo window draws a red rectangle as a demonstration
	 * of per-window data. The dimensions of that rectangle are set
	 * here.
	 */
	e->x = width * 1.0 / 4.0;
	e->w = width * 2.0 / 4.0;
	e->y = height * 1.0 / 4.0;
	e->h = height * 2.0 / 4.0;

	/* Connect the user data to the window */
	window_set_user_data(e->window, e);

	/* Set the callback redraw handler for the window */
	window_set_redraw_handler(e->window, redraw_handler);

	/* Set the callback resize handler for the window */
	window_set_resize_handler(e->window, resize_handler);

	/* Set the callback focus handler for the window */
	window_set_keyboard_focus_handler(e->window,
					  keyboard_focus_handler);

	/* Set the callback key handler for the window */
	window_set_key_handler(e->window, key_handler);

	/* Set the callback button handler for the window */
	window_set_button_handler(e->window, button_handler);

	/* Set the callback motion handler for the window */
	window_set_motion_handler(e->window, motion_handler);

	/* Demonstrate how to create a borderless window.
	   Move windows with META + left mouse button.
	 */
	if(noborder) {
		window_set_decoration(e->window, 0);
	}

	/* Initial drawing of the window */
	eventdemo_draw(e);

	return e;
}
/**
 * \brief command line options for eventdemo
 *
 * see
 * http://developer.gimp.org/api/2.0/glib/glib-Commandline-option-parser.html
 */
static const GOptionEntry option_entries[] = {
	{"title", 0, 0, G_OPTION_ARG_STRING,
	 &title, "Set the window title to TITLE", "TITLE"},
	{"width", 'w', 0, G_OPTION_ARG_INT,
	 &width, "Set the window's width to W", "W"},
	{"height", 'h', 0, G_OPTION_ARG_INT,
	 &height, "Set the window's height to H", "H"},
	{"maxwidth", 0, 0, G_OPTION_ARG_INT,
	 &width_max, "Set the window's maximum width to W", "W"},
	{"maxheight", 0, 0, G_OPTION_ARG_INT,
	 &height_max, "Set the window's maximum height to H", "H"},
	{"noborder", 'b', 0, G_OPTION_ARG_NONE,
	 &noborder, "Don't draw window borders", 0},
	{"log-redraw", 0, 0, G_OPTION_ARG_NONE,
	 &log_redraw, "Log redraw events to stdout", 0},
	{"log-resize", 0, 0, G_OPTION_ARG_NONE,
	 &log_resize, "Log resize events to stdout", 0},
	{"log-focus", 0, 0, G_OPTION_ARG_NONE,
	 &log_focus, "Log keyboard focus events to stdout", 0},
	{"log-key", 0, 0, G_OPTION_ARG_NONE,
	 &log_key, "Log key events to stdout", 0},
	{"log-button", 0, 0, G_OPTION_ARG_NONE,
	 &log_button, "Log button events to stdout", 0},
	{"log-motion", 0, 0, G_OPTION_ARG_NONE,
	 &log_motion, "Log motion events to stdout", 0},
	{NULL}
};

/**
 * \brief Connects to the display, creates the window and hands over
 * to the main loop.
 */
int
main(int argc, char *argv[])
{
	struct display *d;
	struct eventdemo *e;

	/* Connect to the display and have the arguments parsed */
	d = display_create(&argc, &argv, option_entries);
	if (d == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	/* Create new eventdemo window */
	e = eventdemo_create(d);
	if (e == NULL) {
		fprintf(stderr, "failed to create eventdemo: %m\n");
		return -1;
	}

	display_run(d);

	return 0;
}
