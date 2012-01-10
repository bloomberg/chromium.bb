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
#include "../shared/config-parser.h"

#include "tablet-shell-client-protocol.h"

struct tablet_shell {
	struct display *display;
	struct tablet_shell *tablet_shell;
	struct rectangle allocation;
	struct window *lockscreen;
	struct window *switcher;
	struct window *homescreen;
	struct wl_list launcher_list;
};

struct launcher {
	cairo_surface_t *icon;
	char *path;
	struct wl_list link;
};

static char *key_lockscreen_icon;
static char *key_lockscreen_background;
static char *key_homescreen_background;
static char *key_launcher_icon;
static char *key_launcher_path;
static void launcher_section_done(void *data);

static const struct config_key lockscreen_config_keys[] = {
	{ "icon", CONFIG_KEY_STRING, &key_lockscreen_icon },
	{ "background", CONFIG_KEY_STRING, &key_lockscreen_background },
};

static const struct config_key homescreen_config_keys[] = {
	{ "background", CONFIG_KEY_STRING, &key_homescreen_background },
};

static const struct config_key launcher_config_keys[] = {
	{ "icon", CONFIG_KEY_STRING, &key_launcher_icon },
	{ "path", CONFIG_KEY_STRING, &key_launcher_path },
};

static const struct config_section config_sections[] = {
	{ "lockscreen",
	  lockscreen_config_keys, ARRAY_LENGTH(lockscreen_config_keys) },
	{ "homescreen",
	  homescreen_config_keys, ARRAY_LENGTH(homescreen_config_keys) },
	{ "launcher",
	  launcher_config_keys, ARRAY_LENGTH(launcher_config_keys),
	  launcher_section_done }
};

static void
paint_background(cairo_t *cr, const char *path, struct rectangle *allocation)
{
	cairo_surface_t *image = NULL;
	cairo_pattern_t *pattern;
	cairo_matrix_t matrix;
	double sx, sy;

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	if (path)
		image = load_jpeg(path);
	if (image) {
		pattern = cairo_pattern_create_for_surface(image);
		sx = (double) cairo_image_surface_get_width(image) /
			allocation->width;
		sy = (double) cairo_image_surface_get_height(image) /
			allocation->height;
		cairo_matrix_init_scale(&matrix, sx, sy);
		cairo_pattern_set_matrix(pattern, &matrix);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy (pattern);
		cairo_surface_destroy(image);
		cairo_paint(cr);
	} else {
		fprintf(stderr, "couldn't load backgrond image: %s\n",
			key_lockscreen_background);
		cairo_set_source_rgb(cr, 0.2, 0, 0);
		cairo_paint(cr);
	}
}

static void
homescreen_draw(struct tablet_shell *shell)
{
	cairo_surface_t *surface;
	struct rectangle allocation;
	cairo_pattern_t *pattern;
	cairo_matrix_t matrix;
	cairo_t *cr;
	struct launcher *launcher;
	const int rows = 4, columns = 5, icon_width = 128, icon_height = 128;
	int x, y, i, width, height, vmargin, hmargin, vpadding, hpadding;

	window_draw(shell->homescreen);
	window_get_child_allocation(shell->homescreen, &allocation);
	surface = window_get_surface(shell->homescreen);
	cr = cairo_create(surface);

	paint_background(cr, key_homescreen_background, &allocation);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	width = allocation.width - columns * icon_width;
	hpadding = width / (columns + 1);
	hmargin = (width - hpadding * (columns - 1)) / 2;

	height = allocation.height - rows * icon_height;
	vpadding = height / (rows + 1);
	vmargin = (height - vpadding * (rows - 1)) / 2;

	x = hmargin;
	y = vmargin;
	i = 0;

	wl_list_for_each(launcher, &shell->launcher_list, link) {
		pattern = cairo_pattern_create_for_surface(launcher->icon);
		cairo_matrix_init_scale(&matrix, 2.0, 2.0);
		cairo_matrix_translate(&matrix, -x, -y);
		cairo_pattern_set_matrix(pattern, &matrix);
		cairo_pattern_set_extend(pattern, CAIRO_EXTEND_NONE);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy(pattern);
		cairo_paint(cr);
		x += icon_width + hpadding;
		i++;
		if (i == columns) {
			x = hmargin;
			y += icon_height + vpadding;
			i = 0;
		}
	}

	cairo_surface_flush(surface);
	cairo_surface_destroy(surface);
	window_flush(shell->homescreen);
}


static void
lockscreen_draw(struct tablet_shell *shell)
{
	cairo_surface_t *surface;
	cairo_surface_t *icon;
	struct rectangle allocation;
	cairo_t *cr;
	int width, height;

	window_draw(shell->lockscreen);
	window_get_child_allocation(shell->lockscreen, &allocation);
	surface = window_get_surface(shell->lockscreen);
	cr = cairo_create(surface);

	paint_background(cr, key_lockscreen_background, &allocation);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	icon = cairo_image_surface_create_from_png(key_lockscreen_icon);
	width = cairo_image_surface_get_width(icon);
	height = cairo_image_surface_get_height(icon);
	cairo_set_source_surface(cr, icon,
				 allocation.x + (allocation.width - width) / 2,
				 allocation.y + (allocation.height - height) / 2);
	cairo_paint(cr);
	cairo_surface_destroy(icon);

	cairo_surface_flush(surface);
	cairo_surface_destroy(surface);
	window_flush(shell->lockscreen);
}

static void
lockscreen_button_handler(struct widget *widget,
			  struct input *input, uint32_t time,
			  int button, int state, void *data)
{
	struct window *window = data;
	struct tablet_shell *shell = window_get_user_data(window);

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
	wl_list_init(&shell->launcher_list);

	return shell;
}

static void
tablet_shell_add_launcher(struct tablet_shell *shell,
			  const char *icon, const char *path)
{
	struct launcher *launcher;

	launcher = malloc(sizeof *launcher);
	launcher->path = strdup(path);
	launcher->icon = cairo_image_surface_create_from_png(icon);
	if (cairo_surface_status (launcher->icon) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "couldn't load %s\n", icon);
		free(launcher);
		return;
	}

	wl_list_insert(&shell->launcher_list, &launcher->link);
}

static void
launcher_section_done(void *data)
{
	struct tablet_shell *shell = data;

	if (key_launcher_icon == NULL || key_launcher_path == NULL) {
		fprintf(stderr, "invalid launcher section\n");
		return;
	}

	tablet_shell_add_launcher(shell, key_launcher_icon, key_launcher_path);

	free(key_launcher_icon);
	key_launcher_icon = NULL;
	free(key_launcher_path);
	key_launcher_path = NULL;
}

int main(int argc, char *argv[])
{
	struct display *display;
	char *config_file;
	uint32_t id;
	struct tablet_shell *shell;

	display = display_create(&argc, &argv, NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	wl_display_roundtrip(display_get_display(display));
	id = wl_display_get_global(display_get_display(display),
				   "tablet_shell", 1);
	shell = tablet_shell_create(display, id);

	config_file = config_file_path("weston-tablet-shell.ini");
	parse_config_file(config_file,
			  config_sections, ARRAY_LENGTH(config_sections),
			  shell);
	free(config_file);

	homescreen_draw(shell);

	display_run(display);

	return 0;
}
