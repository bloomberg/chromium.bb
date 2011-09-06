/*
 * Copyright © 2011 Kristian Høgsberg
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
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <cairo.h>
#include <sys/wait.h>
#include <linux/input.h>

#include "wayland-client.h"
#include "wayland-glib.h"
#include "window.h"

#include <desktop-shell-client-protocol.h>

struct desktop {
	struct display *display;
	struct desktop_shell *shell;
	struct panel *panel;
	struct window *background;
	const char *background_path;
};

struct panel {
	struct window *window;
	struct wl_list item_list;
	struct panel_item *focus;
};

struct panel_item {
	struct wl_list link;
	cairo_surface_t *icon;
	int x, y, width, height;
	int pressed;
	const char *path;
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
panel_activate_item(struct panel *panel, struct panel_item *item)
{
	pid_t pid;

	pid = fork();
	if (pid < 0) {
		fprintf(stderr, "fork failed: %m\n");
		return;
	}

	if (pid)
		return;
	
	if (execl(item->path, item->path, NULL) < 0) {
		fprintf(stderr, "execl failed: %m\n");
		exit(1);
	}
}

static struct panel_item *
panel_find_item(struct panel *panel, int32_t x, int32_t y)
{
	struct panel_item *item;

	wl_list_for_each(item, &panel->item_list, link) {
		if (item->x <= x && x < item->x + item->width &&
		    item->y <= y && y < item->y + item->height) {
			return item;
		}
	}

	return NULL;
}

static void
panel_draw_item(struct panel *panel, cairo_t *cr, struct panel_item *item)
{
	int x, y;

	if (item->pressed) {
		x = item->x + 1;
		y = item->y + 1;
	} else {
		x = item->x;
		y = item->y;
	}

	cairo_set_source_surface(cr, item->icon, x, y);
	cairo_paint(cr);

	if (panel->focus == item) {
		cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.4);
		cairo_mask_surface(cr, item->icon, x, y);
	}
}

static void
panel_redraw_handler(struct window *window, void *data)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	struct panel *panel = window_get_user_data(window);
	struct panel_item *item;

	window_draw(window);
	surface = window_get_surface(window);
	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.9);
	cairo_paint(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	wl_list_for_each(item, &panel->item_list, link)
		panel_draw_item(panel, cr, item);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	window_flush(window);
}

static void
panel_set_focus(struct panel *panel, struct panel_item *focus)
{
	if (focus == panel->focus)
		return;

	panel->focus = focus;
	window_schedule_redraw(panel->window);
}

static int
panel_enter_handler(struct window *window,
		    struct input *input, uint32_t time,
		    int32_t x, int32_t y, void *data)
{
	struct panel *panel = data;
	struct panel_item *item;

	item = panel_find_item(panel, x, y);
	panel_set_focus(panel, item);

	return POINTER_LEFT_PTR;
}

static void
panel_leave_handler(struct window *window,
		    struct input *input, uint32_t time, void *data)
{
	struct panel *panel = data;

	panel_set_focus(panel, NULL);
}

static int
panel_motion_handler(struct window *window,
		     struct input *input, uint32_t time,
		     int32_t x, int32_t y,
		     int32_t sx, int32_t sy, void *data)
{
	struct panel *panel = data;
	struct panel_item *item;

	if (panel->focus && panel->focus->pressed)
		return POINTER_LEFT_PTR;

	item = panel_find_item(panel, sx, sy);
	panel_set_focus(panel, item);

	return POINTER_LEFT_PTR;
}

static void
panel_button_handler(struct window *window,
		     struct input *input, uint32_t time,
		     int button, int state, void *data)
{
	struct panel *panel = data;
	struct panel_item *item;
	int32_t x, y;

	if (panel->focus && button == BTN_LEFT) {
		panel->focus->pressed = state;
		window_schedule_redraw(panel->window);

		if (state == 0) {
			panel_activate_item(panel, panel->focus);

			input_get_position(input, &x, &y);
			item = panel_find_item(panel, x, y);
			panel_set_focus(panel, item);
		}
	}
}

static struct panel *
panel_create(struct display *display)
{
	struct panel *panel;

	panel = malloc(sizeof *panel);
	memset(panel, 0, sizeof *panel);
	wl_list_init(&panel->item_list);

	panel->window = window_create(display, 0, 0);

	window_set_title(panel->window, "panel");
	window_set_decoration(panel->window, 0);
	window_set_redraw_handler(panel->window, panel_redraw_handler);
	window_set_custom(panel->window);
	window_set_user_data(panel->window, panel);
	window_set_enter_handler(panel->window, panel_enter_handler);
	window_set_leave_handler(panel->window, panel_leave_handler);
	window_set_motion_handler(panel->window, panel_motion_handler);
	window_set_button_handler(panel->window, panel_button_handler);

	return panel;
}

static void
panel_add_item(struct panel *panel, const char *icon, const char *path)
{
	struct panel_item *item;

	item = malloc(sizeof *item);
	memset(item, 0, sizeof *item);
	item->icon = cairo_image_surface_create_from_png(icon);
	item->path = strdup(path);
	wl_list_insert(panel->item_list.prev, &item->link);

	item->width = cairo_image_surface_get_width(item->icon);
	item->height = cairo_image_surface_get_height(item->icon);
}

static void
panel_allocate(struct panel *panel, int width, int height)
{
	struct panel_item *item;
	int x;

	window_set_child_size(panel->window, width, height);
	window_schedule_redraw(panel->window);

	x = 10;
	wl_list_for_each(item, &panel->item_list, link) {
		item->x = x;
		item->y = (height - item->height) / 2;
		x += item->width + 10;
	}
}

static void
background_draw(struct window *window, int width, int height, const char *path)
{
	cairo_surface_t *surface, *image;
	cairo_t *cr;

	window_set_child_size(window, width, height);
	window_draw(window);
	surface = window_get_surface(window);

	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.2, 1.0);
	cairo_paint(cr);

	if (path) {
		image = cairo_image_surface_create_from_png(path);
		cairo_set_source_surface(cr, image, 0, 0);
		cairo_paint(cr);
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	window_flush(window);
}

static void
desktop_shell_configure(void *data,
			struct desktop_shell *desktop_shell,
			uint32_t time, uint32_t edges,
			struct wl_surface *surface,
			int32_t width, int32_t height)
{
	struct desktop *desktop = data;

	if (surface == window_get_wl_surface(desktop->panel->window)) {
		panel_allocate(desktop->panel, width, 32);
	} else if (surface == window_get_wl_surface(desktop->background)) {
		background_draw(desktop->background,
				width, height, desktop->background_path);
	}
}

static const struct desktop_shell_listener listener = {
	desktop_shell_configure
};

static void
global_handler(struct wl_display *display, uint32_t id,
	       const char *interface, uint32_t version, void *data)
{
	struct desktop *desktop = data;

	if (!strcmp(interface, "desktop_shell")) {
		desktop->shell =
			wl_display_bind(display, id, &desktop_shell_interface);
		desktop_shell_add_listener(desktop->shell, &listener, desktop);
	}
}

static const struct {
	const char *icon;
	const char *path;
} launchers[] = {
	{
		"/usr/share/icons/gnome/24x24/apps/utilities-terminal.png",
		"/usr/bin/gnome-terminal"
	},
	{
		"/usr/share/icons/hicolor/24x24/apps/google-chrome.png",
		"/usr/bin/google-chrome"
	},
};

int main(int argc, char *argv[])
{
	struct desktop desktop;
	int i;

	desktop.display = display_create(&argc, &argv, NULL);
	if (desktop.display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	wl_display_add_global_listener(display_get_display(desktop.display),
				       global_handler, &desktop);

	desktop.panel = panel_create(desktop.display);

	for (i = 0; i < ARRAY_LENGTH(launchers); i++)
		panel_add_item(desktop.panel,
			       launchers[i].icon, launchers[i].path);

	desktop_shell_set_panel(desktop.shell,
				window_get_wl_surface(desktop.panel->window));

	desktop.background = window_create(desktop.display, 0, 0);
	window_set_decoration(desktop.background, 0);
	window_set_custom(desktop.background);
	desktop.background_path = argv[1];
	desktop_shell_set_background(desktop.shell,
				     window_get_wl_surface(desktop.background));

	signal(SIGCHLD, sigchild_handler);

	display_run(desktop.display);

	return 0;
}
