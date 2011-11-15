/*
 * Copyright © 2011 Kristian Høgsberg
 * Copyright © 2011 Collabora, Ltd.
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
#include "cairo-util.h"
#include "window.h"

#include <desktop-shell-client-protocol.h>

struct desktop {
	struct display *display;
	struct desktop_shell *shell;
	struct panel *panel;
	struct window *background;
	const char *background_path;
	struct unlock_dialog *unlock_dialog;
	struct task unlock_task;
};

struct panel {
	struct window *window;
	struct window *menu;
};

struct panel_item {
	struct item *item;
	struct panel *panel;
	cairo_surface_t *icon;
	int pressed;
	const char *path;
};

struct unlock_dialog {
	struct window *window;
	struct item *button;
	int closing;

	struct desktop *desktop;
};

static char *key_background_image;
static uint32_t key_panel_color;
static char *key_launcher_icon;
static char *key_launcher_path;
static void launcher_section_done(void *data);

static const struct config_key shell_config_keys[] = {
	{ "background-image", CONFIG_KEY_STRING, &key_background_image },
	{ "panel-color", CONFIG_KEY_INTEGER, &key_panel_color },
};

static const struct config_key launcher_config_keys[] = {
	{ "icon", CONFIG_KEY_STRING, &key_launcher_icon },
	{ "path", CONFIG_KEY_STRING, &key_launcher_path },
};

static const struct config_section config_sections[] = {
	{ "wayland-desktop-shell",
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
show_menu(struct panel *panel, struct input *input)
{
	int32_t x, y, width = 200, height = 200;
	struct display *display;

	input_get_position(input, &x, &y);
	display = window_get_display(panel->window);
	panel->menu = window_create_transient(display, panel->window,
					      x - 10, y - 10, width, height);

	window_draw(panel->menu);
	window_flush(panel->menu);
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

static void
panel_draw_item(struct item *item, void *data)
{
	cairo_t *cr = data;
	struct panel_item *pi;
	int x, y, width, height;
	double dx, dy;

	pi = item_get_user_data(item);
	width = cairo_image_surface_get_width(pi->icon);
	height = cairo_image_surface_get_height(pi->icon);
	x = 0;
	y = -height / 2;
	if (pi->pressed) {
		x++;
		y++;
	}

	dx = x;
	dy = y;
	cairo_user_to_device(cr, &dx, &dy);
	item_set_allocation(item, dx, dy, width, height);

	cairo_set_source_surface(cr, pi->icon, x, y);
	cairo_paint(cr);

	if (window_get_focus_item(pi->panel->window) == item) {
		cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.4);
		cairo_mask_surface(cr, pi->icon, x, y);
	}

	cairo_translate(cr, width + 10, 0);
}

static void
set_hex_color(cairo_t *cr, uint32_t color)
{
	cairo_set_source_rgba(cr, 
			      ((color >> 16) & 0xff) / 255.0,
			      ((color >>  8) & 0xff) / 255.0,
			      ((color >>  0) & 0xff) / 255.0,
			      ((color >> 24) & 0xff) / 255.0);
}

static void
panel_redraw_handler(struct window *window, void *data)
{
	cairo_surface_t *surface;
	cairo_t *cr;

	window_draw(window);
	surface = window_get_surface(window);
	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	set_hex_color(cr, key_panel_color);
	cairo_paint(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_translate(cr, 10, 32 / 2);
	window_for_each_item(window, panel_draw_item, cr);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	window_flush(window);
}

static void
panel_item_focus_handler(struct window *window,
			 struct item *focus, void *data)
{
	window_schedule_redraw(window);
}

static void
panel_button_handler(struct window *window,
		     struct input *input, uint32_t time,
		     int button, int state, void *data)
{
	struct panel *panel = data;
	struct panel_item *pi;
	struct item *focus;

	focus = window_get_focus_item(panel->window);
	if (focus && button == BTN_LEFT) {
		pi = item_get_user_data(focus);
		window_schedule_redraw(panel->window);
		if (state == 0)
			panel_activate_item(panel, pi);
	} else if (button == BTN_RIGHT) {
		if (state)
			show_menu(panel, input);
		else
			window_destroy(panel->menu);
	}
}

static struct panel *
panel_create(struct display *display)
{
	struct panel *panel;

	panel = malloc(sizeof *panel);
	memset(panel, 0, sizeof *panel);

	panel->window = window_create(display, 0, 0);

	window_set_title(panel->window, "panel");
	window_set_decoration(panel->window, 0);
	window_set_redraw_handler(panel->window, panel_redraw_handler);
	window_set_custom(panel->window);
	window_set_user_data(panel->window, panel);
	window_set_button_handler(panel->window, panel_button_handler);
	window_set_item_focus_handler(panel->window, panel_item_focus_handler);

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
	item->panel = panel;
	window_add_item(panel->window, item);
}

static void
background_draw(struct window *window, int width, int height, const char *path)
{
	cairo_surface_t *surface, *image;
	cairo_pattern_t *pattern;
	cairo_matrix_t matrix;
	cairo_t *cr;
	double sx, sy;

	window_set_child_size(window, width, height);
	window_draw(window);
	surface = window_get_surface(window);

	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.2, 1.0);
	cairo_paint(cr);

	if (path) {
		image = load_jpeg(path);
		pattern = cairo_pattern_create_for_surface(image);
		sx = (double) cairo_image_surface_get_width(image) / width;
		sy = (double) cairo_image_surface_get_height(image) / height;
		cairo_matrix_init_scale(&matrix, sx, sy);
		cairo_pattern_set_matrix(pattern, &matrix);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy (pattern);
		cairo_paint(cr);
		cairo_surface_destroy(image);
	}

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	window_flush(window);
}

static void
unlock_dialog_draw(struct unlock_dialog *dialog)
{
	struct rectangle allocation;
	cairo_t *cr;
	cairo_surface_t *surface;
	cairo_pattern_t *pat;
	double cx, cy, r, f;

	window_draw(dialog->window);

	surface = window_get_surface(dialog->window);
	cr = cairo_create(surface);
	window_get_child_allocation(dialog->window, &allocation);
	cairo_rectangle(cr, allocation.x, allocation.y,
			allocation.width, allocation.height);
	cairo_clip(cr);
	cairo_push_group(cr);
	cairo_translate(cr, allocation.x, allocation.y);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
	cairo_paint(cr);

	if (window_get_focus_item(dialog->window) == dialog->button)
		f = 1.0;
	else
		f = 0.7;

	cx = allocation.width / 2.0;
	cy = allocation.height / 2.0;
	r = (cx < cy ? cx : cy) * 0.4;
	pat = cairo_pattern_create_radial(cx, cy, r * 0.7, cx, cy, r);
	cairo_pattern_add_color_stop_rgb(pat, 0.0, 0, 0.86 * f, 0);
	cairo_pattern_add_color_stop_rgb(pat, 0.85, 0.2 * f, f, 0.2 * f);
	cairo_pattern_add_color_stop_rgb(pat, 1.0, 0, 0.86 * f, 0);
	cairo_set_source(cr, pat);
	cairo_arc(cr, cx, cy, r, 0.0, 2.0 * M_PI);
	cairo_fill(cr);

	item_set_allocation(dialog->button,
			    allocation.x + cx - r,
			    allocation.y + cy - r, 2 * r, 2 * r);
	cairo_pattern_destroy(pat);

	cairo_pop_group_to_source(cr);
	cairo_paint(cr);
	cairo_destroy(cr);

	window_flush(dialog->window);
	cairo_surface_destroy(surface);
}

static void
unlock_dialog_button_handler(struct window *window,
			     struct input *input, uint32_t time,
			     int button, int state, void *data)
{
	struct unlock_dialog *dialog = data;
	struct desktop *desktop = dialog->desktop;
	struct item *focus;

	focus = window_get_focus_item(dialog->window);
	if (focus && button == BTN_LEFT) {
		if (state == 0 && !dialog->closing) {
			display_defer(desktop->display, &desktop->unlock_task);
			dialog->closing = 1;
		}
	}
}

static void
unlock_dialog_redraw_handler(struct window *window, void *data)
{
	struct unlock_dialog *dialog = data;

	unlock_dialog_draw(dialog);
}

static void
unlock_dialog_keyboard_focus_handler(struct window *window,
				     struct input *device, void *data)
{
	window_schedule_redraw(window);
}

static void
unlock_dialog_item_focus_handler(struct window *window,
			 struct item *focus, void *data)
{
	window_schedule_redraw(window);
}

static struct unlock_dialog *
unlock_dialog_create(struct desktop *desktop)
{
	struct display *display = desktop->display;
	struct unlock_dialog *dialog;

	dialog = malloc(sizeof *dialog);
	if (!dialog)
		return NULL;
	memset(dialog, 0, sizeof *dialog);

	dialog->window = window_create(display, 260, 230);
	window_set_title(dialog->window, "Unlock your desktop");

	window_set_user_data(dialog->window, dialog);
	window_set_redraw_handler(dialog->window, unlock_dialog_redraw_handler);
	window_set_keyboard_focus_handler(dialog->window,
					  unlock_dialog_keyboard_focus_handler);
	window_set_button_handler(dialog->window, unlock_dialog_button_handler);
	window_set_item_focus_handler(dialog->window,
				      unlock_dialog_item_focus_handler);
	dialog->button = window_add_item(dialog->window, NULL);

	desktop_shell_set_lock_surface(desktop->shell,
				       window_get_wl_surface(dialog->window));

	unlock_dialog_draw(dialog);

	return dialog;
}

static void
unlock_dialog_destroy(struct unlock_dialog *dialog)
{
	window_destroy(dialog->window);
	free(dialog);
}

static void
unlock_dialog_finish(struct task *task, uint32_t events)
{
	struct desktop *desktop =
			container_of(task, struct desktop, unlock_task);

	desktop_shell_unlock(desktop->shell);
	unlock_dialog_destroy(desktop->unlock_dialog);
	desktop->unlock_dialog = NULL;
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
		window_set_child_size(desktop->panel->window, width, 32);
		window_schedule_redraw(desktop->panel->window);
	} else if (surface == window_get_wl_surface(desktop->background)) {
		background_draw(desktop->background,
				width, height, desktop->background_path);
	}
}

static void
desktop_shell_prepare_lock_surface(void *data,
				   struct desktop_shell *desktop_shell)
{
	struct desktop *desktop = data;

	if (!desktop->unlock_dialog) {
		desktop->unlock_dialog = unlock_dialog_create(desktop);
		desktop->unlock_dialog->desktop = desktop;
	}
}

static const struct desktop_shell_listener listener = {
	desktop_shell_configure,
	desktop_shell_prepare_lock_surface
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

static void
launcher_section_done(void *data)
{
	struct desktop *desktop = data;

	if (key_launcher_icon == NULL || key_launcher_path == NULL) {
		fprintf(stderr, "invalid launcher section\n");
		return;
	}

	panel_add_item(desktop->panel, key_launcher_icon, key_launcher_path);
	free(key_launcher_icon);
	key_launcher_icon = NULL;
	free(key_launcher_path);
	key_launcher_path = NULL;
}

int main(int argc, char *argv[])
{
	struct desktop desktop = { 0 };
	char *config_file;

	desktop.unlock_task.run = unlock_dialog_finish;

	desktop.display = display_create(&argc, &argv, NULL);
	if (desktop.display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	/* The fd is our private, do not confuse our children with it. */
	unsetenv("WAYLAND_SOCKET");

	wl_display_add_global_listener(display_get_display(desktop.display),
				       global_handler, &desktop);

	desktop.panel = panel_create(desktop.display);

	config_file = config_file_path("wayland-desktop-shell.ini");
	parse_config_file(config_file,
			  config_sections, ARRAY_LENGTH(config_sections),
			  &desktop);
	free(config_file);

	desktop_shell_set_panel(desktop.shell,
				window_get_wl_surface(desktop.panel->window));

	desktop.background = window_create(desktop.display, 0, 0);
	window_set_decoration(desktop.background, 0);
	window_set_custom(desktop.background);
	desktop.background_path = key_background_image;
	desktop_shell_set_background(desktop.shell,
				     window_get_wl_surface(desktop.background));

	signal(SIGCHLD, sigchild_handler);

	display_run(desktop.display);

	return 0;
}
