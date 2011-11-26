/*
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "window.h"
#include "cairo-util.h"

#include "tablet-shell-client-protocol.h"

struct tablet_shell {
	struct display *display;
	struct tablet_shell *tablet_shell;
	struct rectangle allocation;
	struct window *lockscreen;
	struct window *switcher;
	struct window *homescreen;
};

static void
draw_stub(struct window *window, const char *caption)
{
	cairo_surface_t *surface;
	struct rectangle allocation;
	const int margin = 50, radius = 10;
	cairo_text_extents_t extents;
	cairo_t *cr;

	window_draw(window);
	window_get_child_allocation(window, &allocation);
	surface = window_get_surface(window);
	cr = cairo_create(surface);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.4);
	cairo_paint(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	rounded_rect(cr, allocation.x + margin, allocation.y + margin,
		     allocation.x + allocation.width - margin,
		     allocation.y + allocation.height - margin, radius);
	cairo_set_line_width(cr, 6);
	cairo_stroke(cr);

	cairo_select_font_face(cr, "Sans",
			       CAIRO_FONT_SLANT_NORMAL,
			       CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 40);

	cairo_text_extents(cr, caption, &extents);
	cairo_move_to(cr,
		      allocation.x + (allocation.width - extents.width) / 2,
		      allocation.y + (allocation.height - extents.height) / 2);
	cairo_show_text(cr, caption);

	cairo_surface_flush(surface);
	cairo_surface_destroy(surface);
	window_flush(window);
}


static void
homescreen_draw(struct tablet_shell *shell)
{
	draw_stub(shell->homescreen, "Homescreen Stub");
}


static void
lockscreen_draw(struct tablet_shell *shell)
{
	draw_stub(shell->lockscreen, "Lockscreen Stub");
}

static int
lockscreen_motion_handler(struct window *window,
			  struct input *input, uint32_t time,
			  int32_t x, int32_t y,
			  int32_t sx, int32_t sy, void *data)
{
	return POINTER_LEFT_PTR;
}

static void
lockscreen_button_handler(struct window *window,
			  struct input *input, uint32_t time,
			  int button, int state, void *data)
{
	struct tablet_shell *shell = data;

	window_destroy(shell->lockscreen);
	shell->lockscreen = NULL;
}

static void
show_lockscreen(void *data, struct tablet_shell *tablet_shell)
{
	struct tablet_shell *shell = data;

	shell->lockscreen = window_create(shell->display,
					  shell->allocation.width,
					  shell->allocation.height);
	window_set_user_data(shell->lockscreen, shell);
	window_set_decoration(shell->lockscreen, 0);
	window_set_custom(shell->lockscreen);
	window_set_button_handler(shell->lockscreen,
				  lockscreen_button_handler);
	window_set_motion_handler(shell->lockscreen,
				  lockscreen_motion_handler);


	tablet_shell_set_lockscreen(shell->tablet_shell,
				    window_get_wl_surface(shell->lockscreen));
	lockscreen_draw(shell);
}

static void
show_switcher(void *data, struct tablet_shell *tablet_shell)
{
	struct tablet_shell *shell = data;

	shell->switcher = window_create(shell->display, 0, 0);
	window_set_user_data(shell->switcher, shell);
	window_set_decoration(shell->switcher, 0);
	window_set_custom(shell->switcher);
	tablet_shell_set_switcher(shell->tablet_shell,
				  window_get_wl_surface(shell->switcher));
}

static void
hide_switcher(void *data, struct tablet_shell *tablet_shell)
{
}

static const struct tablet_shell_listener tablet_shell_listener = {
	show_lockscreen,
	show_switcher,
	hide_switcher
};

static struct tablet_shell *
tablet_shell_create(struct display *display, uint32_t id)
{
	struct tablet_shell *shell;
	struct output *output;

	shell = malloc(sizeof *shell);

	shell->display = display;
	shell->tablet_shell =
		wl_display_bind(display_get_display(display),
				id, &tablet_shell_interface);
	tablet_shell_add_listener(shell->tablet_shell,
				  &tablet_shell_listener, shell);
	output = display_get_output(display);
	output_get_allocation(output, &shell->allocation);

	shell->homescreen = window_create(display,
					  shell->allocation.width,
					  shell->allocation.height);
	window_set_user_data(shell->homescreen, shell);
	window_set_decoration(shell->homescreen, 0);
	window_set_custom(shell->homescreen);

	tablet_shell_set_homescreen(shell->tablet_shell,
				    window_get_wl_surface(shell->homescreen));

	homescreen_draw(shell);

	return shell;
}

int main(int argc, char *argv[])
{
	struct display *display;
	uint32_t id;

	fprintf(stderr, "tablet shell client running\n");

	display = display_create(&argc, &argv, NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	wl_display_roundtrip(display_get_display(display));
	id = wl_display_get_global(display_get_display(display),
				   "tablet_shell", 1);
	tablet_shell_create(display, id);
	display_run(display);

	return 0;
}
