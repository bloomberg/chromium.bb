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

#include <wayland-client.h>
#include "cairo-util.h"
#include "window.h"
#include "../shared/config-parser.h"

#include "desktop-shell-client-protocol.h"

struct desktop {
	struct display *display;
	struct desktop_shell *shell;
	struct unlock_dialog *unlock_dialog;
	struct task unlock_task;
	struct wl_list outputs;
};

struct surface {
	void (*configure)(void *data,
			  struct desktop_shell *desktop_shell,
			  uint32_t time, uint32_t edges,
			  struct window *window,
			  int32_t width, int32_t height);
};

struct panel {
	struct surface base;
	struct window *window;
	struct widget *widget;
	struct wl_list launcher_list;
};

struct background {
	struct surface base;
	struct window *window;
	struct widget *widget;
};

struct output {
	struct wl_output *output;
	struct wl_list link;

	struct panel *panel;
	struct background *background;
};

struct panel_launcher {
	struct widget *widget;
	struct panel *panel;
	cairo_surface_t *icon;
	int focused, pressed;
	const char *path;
	struct wl_list link;
};

struct unlock_dialog {
	struct window *window;
	struct widget *widget;
	struct widget *button;
	int button_focused;
	int closing;

	struct desktop *desktop;
};

static char *key_background_image = DATADIR "/weston/pattern.png";
static char *key_background_type = "tile";
static uint32_t key_panel_color = 0xaa000000;
static uint32_t key_background_color = 0xff002244;
static char *key_launcher_icon;
static char *key_launcher_path;
static void launcher_section_done(void *data);
static int key_locking = 1;

static const struct config_key shell_config_keys[] = {
	{ "background-image", CONFIG_KEY_STRING, &key_background_image },
	{ "background-type", CONFIG_KEY_STRING, &key_background_type },
	{ "panel-color", CONFIG_KEY_UNSIGNED_INTEGER, &key_panel_color },
	{ "background-color", CONFIG_KEY_UNSIGNED_INTEGER, &key_background_color },
	{ "locking", CONFIG_KEY_BOOLEAN, &key_locking },
};

static const struct config_key launcher_config_keys[] = {
	{ "icon", CONFIG_KEY_STRING, &key_launcher_icon },
	{ "path", CONFIG_KEY_STRING, &key_launcher_path },
};

static const struct config_section config_sections[] = {
	{ "desktop-shell",
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
menu_func(struct window *window, int index, void *data)
{
	printf("Selected index %d from a panel menu.\n", index);
}

static void
show_menu(struct panel *panel, struct input *input, uint32_t time)
{
	int32_t x, y;
	static const char *entries[] = {
		"Roy", "Pris", "Leon", "Zhora"
	};

	input_get_position(input, &x, &y);
	window_show_menu(window_get_display(panel->window),
			 input, time, panel->window,
			 x - 10, y - 10, menu_func, entries, 4);
}

static void
panel_launcher_activate(struct panel_launcher *widget)
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
panel_launcher_redraw_handler(struct widget *widget, void *data)
{
	struct panel_launcher *launcher = data;
	cairo_surface_t *surface;
	struct rectangle allocation;
	cairo_t *cr;

	surface = window_get_surface(launcher->panel->window);
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
set_hex_color(cairo_t *cr, uint32_t color)
{
	cairo_set_source_rgba(cr, 
			      ((color >> 16) & 0xff) / 255.0,
			      ((color >>  8) & 0xff) / 255.0,
			      ((color >>  0) & 0xff) / 255.0,
			      ((color >> 24) & 0xff) / 255.0);
}

static void
panel_redraw_handler(struct widget *widget, void *data)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	struct panel *panel = data;

	surface = window_get_surface(panel->window);
	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	set_hex_color(cr, key_panel_color);
	cairo_paint(cr);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

static int
panel_launcher_enter_handler(struct widget *widget, struct input *input,
			     uint32_t time, int32_t x, int32_t y, void *data)
{
	struct panel_launcher *launcher = data;

	launcher->focused = 1;
	widget_schedule_redraw(widget);

	return POINTER_LEFT_PTR;
}

static void
panel_launcher_leave_handler(struct widget *widget,
			     struct input *input, void *data)
{
	struct panel_launcher *launcher = data;

	launcher->focused = 0;
	widget_schedule_redraw(widget);
}

static void
panel_launcher_button_handler(struct widget *widget,
			      struct input *input, uint32_t time,
			      int button, int state, void *data)
{
	struct panel_launcher *launcher;

	launcher = widget_get_user_data(widget);
	widget_schedule_redraw(widget);
	if (state == 0)
		panel_launcher_activate(launcher);
}

static void
panel_button_handler(struct widget *widget,
		     struct input *input, uint32_t time,
		     int button, int state, void *data)
{
	struct panel *panel = data;

	if (button == BTN_RIGHT && state)
		show_menu(panel, input, time);
}

static void
panel_resize_handler(struct widget *widget,
		     int32_t width, int32_t height, void *data)
{
	struct panel_launcher *launcher;
	struct panel *panel = data;
	int x, y, w, h;
	
	x = 10;
	y = 16;
	wl_list_for_each(launcher, &panel->launcher_list, link) {
		w = cairo_image_surface_get_width(launcher->icon);
		h = cairo_image_surface_get_height(launcher->icon);
		widget_set_allocation(launcher->widget,
				      x, y - h / 2, w + 1, h + 1);
		x += w + 10;
	}
}

static void
panel_configure(void *data,
		struct desktop_shell *desktop_shell,
		uint32_t time, uint32_t edges,
		struct window *window,
		int32_t width, int32_t height)
{
	struct surface *surface = window_get_user_data(window);
	struct panel *panel = container_of(surface, struct panel, base);

	window_schedule_resize(panel->window, width, 32);
}

static struct panel *
panel_create(struct display *display)
{
	struct panel *panel;

	panel = malloc(sizeof *panel);
	memset(panel, 0, sizeof *panel);

	panel->base.configure = panel_configure;
	panel->window = window_create(display, 0, 0);
	panel->widget = window_add_widget(panel->window, panel);
	wl_list_init(&panel->launcher_list);

	window_set_title(panel->window, "panel");
	window_set_custom(panel->window);
	window_set_user_data(panel->window, panel);

	widget_set_redraw_handler(panel->widget, panel_redraw_handler);
	widget_set_resize_handler(panel->widget, panel_resize_handler);
	widget_set_button_handler(panel->widget, panel_button_handler);

	return panel;
}

static void
panel_add_launcher(struct panel *panel, const char *icon, const char *path)
{
	struct panel_launcher *launcher;

	launcher = malloc(sizeof *launcher);
	memset(launcher, 0, sizeof *launcher);
	launcher->icon = cairo_image_surface_create_from_png(icon);
	launcher->path = strdup(path);
	launcher->panel = panel;
	wl_list_insert(panel->launcher_list.prev, &launcher->link);

	launcher->widget = widget_add_widget(panel->widget, launcher);
	widget_set_enter_handler(launcher->widget,
				 panel_launcher_enter_handler);
	widget_set_leave_handler(launcher->widget,
				   panel_launcher_leave_handler);
	widget_set_button_handler(launcher->widget,
				    panel_launcher_button_handler);
	widget_set_redraw_handler(launcher->widget,
				  panel_launcher_redraw_handler);
}

enum {
	BACKGROUND_SCALE,
	BACKGROUND_TILE
};

static void
background_draw(struct widget *widget, void *data)
{
	struct background *background = data;
	cairo_surface_t *surface, *image;
	cairo_pattern_t *pattern;
	cairo_matrix_t matrix;
	cairo_t *cr;
	double sx, sy;
	struct rectangle allocation;
	int type = -1;

	surface = window_get_surface(background->window);

	cr = cairo_create(surface);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.2, 1.0);
	cairo_paint(cr);

	widget_get_allocation(widget, &allocation);
	image = NULL;
	if (key_background_image)
		image = load_image(key_background_image);

	if (strcmp(key_background_type, "scale") == 0)
		type = BACKGROUND_SCALE;
	else if (strcmp(key_background_type, "tile") == 0)
		type = BACKGROUND_TILE;
	else
		fprintf(stderr, "invalid background-type: %s\n",
			key_background_type);

	if (image && type != -1) {
		pattern = cairo_pattern_create_for_surface(image);
		switch (type) {
		case BACKGROUND_SCALE:
			sx = (double) cairo_image_surface_get_width(image) /
				allocation.width;
			sy = (double) cairo_image_surface_get_height(image) /
				allocation.height;
			cairo_matrix_init_scale(&matrix, sx, sy);
			cairo_pattern_set_matrix(pattern, &matrix);
			break;
		case BACKGROUND_TILE:
			cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
			break;
		}
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy (pattern);
		cairo_surface_destroy(image);
	} else {
		set_hex_color(cr, key_background_color);
	}

	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

static void
background_configure(void *data,
		     struct desktop_shell *desktop_shell,
		     uint32_t time, uint32_t edges,
		     struct window *window,
		     int32_t width, int32_t height)
{
	struct background *background =
		(struct background *) window_get_user_data(window);

	widget_schedule_resize(background->widget, width, height);
}

static void
unlock_dialog_redraw_handler(struct widget *widget, void *data)
{
	struct unlock_dialog *dialog = data;
	struct rectangle allocation;
	cairo_t *cr;
	cairo_surface_t *surface;
	cairo_pattern_t *pat;
	double cx, cy, r, f;

	surface = window_get_surface(dialog->window);
	cr = cairo_create(surface);
	widget_get_allocation(dialog->widget, &allocation);
	cairo_rectangle(cr, allocation.x, allocation.y,
			allocation.width, allocation.height);
	cairo_clip(cr);
	cairo_push_group(cr);
	cairo_translate(cr, allocation.x, allocation.y);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
	cairo_paint(cr);

	if (dialog->button_focused)
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

	widget_set_allocation(dialog->button,
			    allocation.x + cx - r,
			    allocation.y + cy - r, 2 * r, 2 * r);
	cairo_pattern_destroy(pat);

	cairo_pop_group_to_source(cr);
	cairo_paint(cr);
	cairo_destroy(cr);

	cairo_surface_destroy(surface);
}

static void
unlock_dialog_button_handler(struct widget *widget,
			     struct input *input, uint32_t time,
			     int button, int state, void *data)
{
	struct unlock_dialog *dialog = data;
	struct desktop *desktop = dialog->desktop;

	if (button == BTN_LEFT) {
		if (state == 0 && !dialog->closing) {
			display_defer(desktop->display, &desktop->unlock_task);
			dialog->closing = 1;
		}
	}
}

static void
unlock_dialog_keyboard_focus_handler(struct window *window,
				     struct input *device, void *data)
{
	window_schedule_redraw(window);
}

static int
unlock_dialog_widget_enter_handler(struct widget *widget,
				   struct input *input, uint32_t time,
				   int32_t x, int32_t y, void *data)
{
	struct unlock_dialog *dialog = data;

	dialog->button_focused = 1;
	widget_schedule_redraw(widget);

	return POINTER_LEFT_PTR;
}

static void
unlock_dialog_widget_leave_handler(struct widget *widget,
				   struct input *input, void *data)
{
	struct unlock_dialog *dialog = data;

	dialog->button_focused = 0;
	widget_schedule_redraw(widget);
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
	dialog->widget = frame_create(dialog->window, dialog);
	window_set_title(dialog->window, "Unlock your desktop");
	window_set_custom(dialog->window);

	window_set_user_data(dialog->window, dialog);
	window_set_keyboard_focus_handler(dialog->window,
					  unlock_dialog_keyboard_focus_handler);
	dialog->button = widget_add_widget(dialog->widget, dialog);
	widget_set_redraw_handler(dialog->widget,
				  unlock_dialog_redraw_handler);
	widget_set_enter_handler(dialog->button,
				 unlock_dialog_widget_enter_handler);
	widget_set_leave_handler(dialog->button,
				 unlock_dialog_widget_leave_handler);
	widget_set_button_handler(dialog->button,
				  unlock_dialog_button_handler);

	desktop_shell_set_lock_surface(desktop->shell,
	       window_get_wl_shell_surface(dialog->window));

	window_schedule_resize(dialog->window, 260, 230);

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
			struct wl_shell_surface *shell_surface,
			int32_t width, int32_t height)
{
	struct window *window = wl_shell_surface_get_user_data(shell_surface);
	struct surface *s = window_get_user_data(window);

	s->configure(data, desktop_shell, time, edges, window, width, height);
}

static void
desktop_shell_prepare_lock_surface(void *data,
				   struct desktop_shell *desktop_shell)
{
	struct desktop *desktop = data;

	if (!key_locking) {
		desktop_shell_unlock(desktop->shell);
		return;
	}

	if (!desktop->unlock_dialog) {
		desktop->unlock_dialog = unlock_dialog_create(desktop);
		desktop->unlock_dialog->desktop = desktop;
	}
}

static const struct desktop_shell_listener listener = {
	desktop_shell_configure,
	desktop_shell_prepare_lock_surface
};

static struct background *
background_create(struct desktop *desktop)
{
	struct background *background;

	background = malloc(sizeof *background);
	memset(background, 0, sizeof *background);

	background->base.configure = background_configure;
	background->window = window_create(desktop->display, 0, 0);
	background->widget = window_add_widget(background->window, background);
	window_set_custom(background->window);
	window_set_user_data(background->window, background);
	widget_set_redraw_handler(background->widget, background_draw);

	return background;
}

static void
create_output(struct desktop *desktop, uint32_t id)
{
	struct output *output;

	output = calloc(1, sizeof *output);
	if (!output)
		return;

	output->output = wl_display_bind(display_get_display(desktop->display),
					 id, &wl_output_interface);

	wl_list_insert(&desktop->outputs, &output->link);
}

static void
global_handler(struct wl_display *display, uint32_t id,
	       const char *interface, uint32_t version, void *data)
{
	struct desktop *desktop = data;

	if (!strcmp(interface, "desktop_shell")) {
		desktop->shell =
			wl_display_bind(display, id, &desktop_shell_interface);
		desktop_shell_add_listener(desktop->shell, &listener, desktop);
	} else if (!strcmp(interface, "wl_output")) {
		create_output(desktop, id);
	}
}

static void
launcher_section_done(void *data)
{
	struct desktop *desktop = data;
	struct output *output;

	if (key_launcher_icon == NULL || key_launcher_path == NULL) {
		fprintf(stderr, "invalid launcher section\n");
		return;
	}

	wl_list_for_each(output, &desktop->outputs, link)
		panel_add_launcher(output->panel,
				   key_launcher_icon, key_launcher_path);

	free(key_launcher_icon);
	key_launcher_icon = NULL;
	free(key_launcher_path);
	key_launcher_path = NULL;
}

static void
add_default_launcher(struct desktop *desktop)
{
	struct output *output;

	wl_list_for_each(output, &desktop->outputs, link)
		panel_add_launcher(output->panel,
				   DATADIR "/weston/terminal.png",
				   "/usr/bin/weston-terminal");
}

int main(int argc, char *argv[])
{
	struct desktop desktop = { 0 };
	char *config_file;
	struct output *output;
	int ret;

	desktop.unlock_task.run = unlock_dialog_finish;
	wl_list_init(&desktop.outputs);

	desktop.display = display_create(&argc, &argv, NULL);
	if (desktop.display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}

	wl_display_add_global_listener(display_get_display(desktop.display),
				       global_handler, &desktop);

	wl_list_for_each(output, &desktop.outputs, link) {
		struct wl_shell_surface *s;

		output->panel = panel_create(desktop.display);
		s = window_get_wl_shell_surface(output->panel->window);
		desktop_shell_set_panel(desktop.shell, output->output, s);

		output->background = background_create(&desktop);
		s = window_get_wl_shell_surface(output->background->window);
		desktop_shell_set_background(desktop.shell, output->output, s);
	}

	config_file = config_file_path("weston-desktop-shell.ini");
	ret = parse_config_file(config_file,
				config_sections, ARRAY_LENGTH(config_sections),
				&desktop);
	free(config_file);
	if (ret < 0)
		add_default_launcher(&desktop);

	signal(SIGCHLD, sigchild_handler);

	display_run(desktop.display);

	return 0;
}
