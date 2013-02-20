/*
 * Copyright © 2011, 2012 Intel Corporation
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
#include <unistd.h>
#include <sys/wait.h>

#include "window.h"
#include "../shared/cairo-util.h"
#include "../shared/config-parser.h"

#include "tablet-shell-client-protocol.h"

struct tablet {
	struct display *display;
	struct tablet_shell *tablet_shell;
	struct rectangle allocation;
	struct window *switcher;

	struct homescreen *homescreen;
	struct lockscreen *lockscreen;
};

struct homescreen {
	struct window *window;
	struct widget *widget;
	struct wl_list launcher_list;
};

struct lockscreen {
	struct window *window;
	struct widget *widget;
};

struct launcher {
	struct widget *widget;
	struct homescreen *homescreen;
	cairo_surface_t *icon;
	int focused, pressed;
	char *path;
	struct wl_list link;
};

static char *key_lockscreen_icon;
static char *key_lockscreen_background;
static char *key_homescreen_background;
static char *key_launcher_icon;
static char *key_launcher_path;
static void launcher_section_done(void *data);

static const struct config_key shell_config_keys[] = {
	{ "lockscreen-icon", CONFIG_KEY_STRING, &key_lockscreen_icon },
	{ "lockscreen", CONFIG_KEY_STRING, &key_lockscreen_background },
	{ "homescreen", CONFIG_KEY_STRING, &key_homescreen_background },
};

static const struct config_key launcher_config_keys[] = {
	{ "icon", CONFIG_KEY_STRING, &key_launcher_icon },
	{ "path", CONFIG_KEY_STRING, &key_launcher_path },
};

static const struct config_section config_sections[] = {
	{ "shell",
	  shell_config_keys, ARRAY_LENGTH(shell_config_keys) },
	{ "launcher",
	  launcher_config_keys, ARRAY_LENGTH(launcher_config_keys),
	  launcher_section_done }
};

static void
sigchild_handler(int s)
{
	int status;
	pid_t pid;

	while (pid = waitpid(-1, &status, WNOHANG), pid > 0)
		fprintf(stderr, "child %d exited\n", pid);
}

static void
paint_background(cairo_t *cr, const char *path, struct rectangle *allocation)
{
	cairo_surface_t *image = NULL;
	cairo_pattern_t *pattern;
	cairo_matrix_t matrix;
	double sx, sy;

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	if (path)
		image = load_cairo_surface(path);
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
		fprintf(stderr, "couldn't load background image: %s\n", path);
		cairo_set_source_rgb(cr, 0.2, 0, 0);
		cairo_paint(cr);
	}
}

static void
homescreen_draw(struct widget *widget, void *data)
{
	struct homescreen *homescreen = data;
	cairo_surface_t *surface;
	struct rectangle allocation;
	cairo_t *cr;
	struct launcher *launcher;
	const int rows = 4, columns = 5, icon_width = 128, icon_height = 128;
	int x, y, i, width, height, vmargin, hmargin, vpadding, hpadding;

	surface = window_get_surface(homescreen->window);
	cr = cairo_create(surface);

	widget_get_allocation(widget, &allocation);
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

	wl_list_for_each(launcher, &homescreen->launcher_list, link) {
		widget_set_allocation(launcher->widget,
				      x, y, icon_width, icon_height);
		x += icon_width + hpadding;
		i++;
		if (i == columns) {
			x = hmargin;
			y += icon_height + vpadding;
			i = 0;
		}
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

static void
lockscreen_draw(struct widget *widget, void *data)
{
	struct lockscreen *lockscreen = data;
	cairo_surface_t *surface;
	cairo_surface_t *icon;
	struct rectangle allocation;
	cairo_t *cr;
	int width, height;

	surface = window_get_surface(lockscreen->window);
	cr = cairo_create(surface);

	widget_get_allocation(widget, &allocation);
	paint_background(cr, key_lockscreen_background, &allocation);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	icon = load_cairo_surface(key_lockscreen_icon);
	if (icon) {
		width = cairo_image_surface_get_width(icon);
		height = cairo_image_surface_get_height(icon);
		cairo_set_source_surface(cr, icon,
			allocation.x + (allocation.width - width) / 2,
			allocation.y + (allocation.height - height) / 2);
	} else {
		fprintf(stderr, "couldn't load lockscreen icon: %s\n",
				 key_lockscreen_icon);
		cairo_set_source_rgb(cr, 0.2, 0, 0);
	}
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(icon);
	cairo_surface_destroy(surface);
}

static void
lockscreen_button_handler(struct widget *widget,
			  struct input *input, uint32_t time,
			  uint32_t button,
			  enum wl_pointer_button_state state, void *data)
{
	struct lockscreen *lockscreen = data;

	if (state == WL_POINTER_BUTTON_STATE_PRESSED && lockscreen->window) {
		window_destroy(lockscreen->window);
		lockscreen->window = NULL;
	}
}

static struct homescreen *
homescreen_create(struct tablet *tablet)
{
	struct homescreen *homescreen;

	homescreen = malloc (sizeof *homescreen);
	memset(homescreen, 0, sizeof *homescreen);

	homescreen->window = window_create_custom(tablet->display);
	homescreen->widget =
		window_add_widget(homescreen->window, homescreen);
	window_set_user_data(homescreen->window, homescreen);
	window_set_title(homescreen->window, "homescreen");
	widget_set_redraw_handler(homescreen->widget, homescreen_draw);

	return homescreen;
}

static struct lockscreen *
lockscreen_create(struct tablet *tablet)
{
	struct lockscreen *lockscreen;

	lockscreen = malloc (sizeof *lockscreen);
	memset(lockscreen, 0, sizeof *lockscreen);

	lockscreen->window = window_create_custom(tablet->display);
	lockscreen->widget =
		window_add_widget(lockscreen->window, lockscreen);
	window_set_user_data(lockscreen->window, lockscreen);
	window_set_title(lockscreen->window, "lockscreen");
	widget_set_redraw_handler(lockscreen->widget, lockscreen_draw);
	widget_set_button_handler(lockscreen->widget,
				  lockscreen_button_handler);

	return lockscreen;
}

static void
show_lockscreen(void *data, struct tablet_shell *tablet_shell)
{
	struct tablet *tablet = data;

	tablet->lockscreen = lockscreen_create(tablet);
	tablet_shell_set_lockscreen(tablet->tablet_shell,
			window_get_wl_surface(tablet->lockscreen->window));

	widget_schedule_resize(tablet->lockscreen->widget,
			       tablet->allocation.width,
			       tablet->allocation.height);
}

static void
show_switcher(void *data, struct tablet_shell *tablet_shell)
{
	struct tablet *tablet = data;

	tablet->switcher = window_create_custom(tablet->display);
	window_set_user_data(tablet->switcher, tablet);
	tablet_shell_set_switcher(tablet->tablet_shell,
				  window_get_wl_surface(tablet->switcher));
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

static int
launcher_enter_handler(struct widget *widget, struct input *input,
			     float x, float y, void *data)
{
	struct launcher *launcher = data;

	launcher->focused = 1;
	widget_schedule_redraw(widget);

	return CURSOR_LEFT_PTR;
}

static void
launcher_leave_handler(struct widget *widget,
			     struct input *input, void *data)
{
	struct launcher *launcher = data;

	launcher->focused = 0;
	widget_schedule_redraw(widget);
}

static void
launcher_activate(struct launcher *widget)
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		fprintf(stderr, "fork failed: %m\n");
		return;
	}

	if (pid)
		return;

	if (execl(widget->path, widget->path, NULL) < 0) {
		fprintf(stderr, "execl '%s' failed: %m\n", widget->path);
		exit(1);
	}
}

static void
launcher_button_handler(struct widget *widget,
			      struct input *input, uint32_t time,
			      uint32_t button,
			      enum wl_pointer_button_state state, void *data)
{
	struct launcher *launcher;

	launcher = widget_get_user_data(widget);
	widget_schedule_redraw(widget);
	if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
		launcher_activate(launcher);
		launcher->pressed = 0;
	} else if (state == WL_POINTER_BUTTON_STATE_PRESSED) 
		launcher->pressed = 1;
}

static void
launcher_redraw_handler(struct widget *widget, void *data)
{
	struct launcher *launcher = data;
	cairo_surface_t *surface;
	struct rectangle allocation;
	cairo_t *cr;

	surface = window_get_surface(launcher->homescreen->window);
	cr = cairo_create(surface);

	widget_get_allocation(widget, &allocation);
	if (launcher->pressed) {
		allocation.x++;
		allocation.y++;
	}

	cairo_set_source_surface(cr, launcher->icon,
				 allocation.x, allocation.y);
	cairo_paint(cr);

	if (launcher->focused) {
		cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.4);
		cairo_mask_surface(cr, launcher->icon,
				   allocation.x, allocation.y);
	}

	cairo_destroy(cr);
}

static void
tablet_shell_add_launcher(struct tablet *tablet,
			  const char *icon, const char *path)
{
	struct launcher *launcher;
	struct homescreen *homescreen = tablet->homescreen;

	launcher = malloc(sizeof *launcher);
	launcher->path = strdup(path);
	launcher->icon = load_cairo_surface(icon);
	if ( !launcher->icon ||
	     cairo_surface_status (launcher->icon) != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "couldn't load %s\n", icon);
		free(launcher);
		return;
	}

	launcher->homescreen = homescreen;
	launcher->widget = widget_add_widget(homescreen->widget, launcher);
	widget_set_enter_handler(launcher->widget,
				 launcher_enter_handler);
	widget_set_leave_handler(launcher->widget,
				 launcher_leave_handler);
	widget_set_button_handler(launcher->widget,
				  launcher_button_handler);
	widget_set_redraw_handler(launcher->widget,
				  launcher_redraw_handler);

	wl_list_insert(&homescreen->launcher_list, &launcher->link);
}

static void
launcher_section_done(void *data)
{
	struct tablet *tablet = data;

	if (key_launcher_icon == NULL || key_launcher_path == NULL) {
		fprintf(stderr, "invalid launcher section\n");
		return;
	}

	tablet_shell_add_launcher(tablet, key_launcher_icon, key_launcher_path);

	free(key_launcher_icon);
	key_launcher_icon = NULL;
	free(key_launcher_path);
	key_launcher_path = NULL;
}

static void
global_handler(struct display *display, uint32_t name,
		const char *interface, uint32_t version, void *data)
{
	struct tablet *tablet = data;

	if (!strcmp(interface, "tablet_shell")) {
		tablet->tablet_shell =
			display_bind(display, name,
				     &tablet_shell_interface, 1);
		tablet_shell_add_listener(tablet->tablet_shell,
				&tablet_shell_listener, tablet);
	}
}

int main(int argc, char *argv[])
{
	struct tablet tablet = { 0 };
	struct display *display;
	char *config_file;
	struct output *output;

	display = display_create(&argc, argv);
	if (display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	tablet.display = display;

	display_set_user_data(tablet.display, &tablet);
	display_set_global_handler(tablet.display, global_handler);

	tablet.homescreen = homescreen_create(&tablet);
	tablet_shell_set_homescreen(tablet.tablet_shell,
			window_get_wl_surface(tablet.homescreen->window));
	wl_list_init(&tablet.homescreen->launcher_list);

	config_file = config_file_path("weston.ini");
	parse_config_file(config_file,
			  config_sections, ARRAY_LENGTH(config_sections),
			  &tablet);
	free(config_file);

	signal(SIGCHLD, sigchild_handler);

	output = display_get_output(tablet.display);
	output_get_allocation(output, &tablet.allocation);
	widget_schedule_resize(tablet.homescreen->widget,
			tablet.allocation.width,
			tablet.allocation.height);
	display_run(display);

	return 0;
}
