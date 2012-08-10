/*
 * Copyright © 2012 Openismus GmbH
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/input.h>
#include <cairo.h>

#include "window.h"
#include "text-client-protocol.h"

struct text_entry {
	struct widget *widget;
	char *text;
	int active;
	struct rectangle allocation;
	struct text_model *model;
};

struct editor {
	struct text_model_manager *text_model_manager;
	struct display *display;
	struct window *window;
	struct widget *widget;
	struct text_entry *entry;
	struct text_entry *editor;
};

static void
text_entry_append(struct text_entry *entry, const char *text)
{
	entry->text = realloc(entry->text, strlen(entry->text) + strlen(text) + 1);
	strcat(entry->text, text);
}


static void
text_model_commit_string(void *data,
			 struct text_model *text_model,
			 const char *text,
			 uint32_t index)
{
	struct text_entry *entry = data;

	text_entry_append(entry, text);	

	widget_schedule_redraw(entry->widget);
}

static void
text_model_preedit_string(void *data,
			  struct text_model *text_model,
			  const char *text,
			  uint32_t index)
{
}

static void
text_model_preedit_styling(void *data,
			   struct text_model *text_model)
{
}

static void
text_model_key(void *data,
	       struct text_model *text_model)
{
}

static void
text_model_selection_replacement(void *data,
				 struct text_model *text_model)
{
}

static void
text_model_direction(void *data,
		     struct text_model *text_model)
{
}

static void
text_model_locale(void *data,
		  struct text_model *text_model)
{
}

static const struct text_model_listener text_model_listener = {
	text_model_commit_string,
	text_model_preedit_string,
	text_model_preedit_styling,
	text_model_key,
	text_model_selection_replacement,
	text_model_direction,
	text_model_locale
};

static struct text_entry*
text_entry_create(struct editor *editor, const char *text)
{
	struct text_entry *entry;
	struct wl_surface *surface;

	entry = malloc(sizeof *entry);

	surface = window_get_wl_surface(editor->window);

	entry->widget = editor->widget;
	entry->text = strdup(text);
	entry->active = 0;
	entry->model = text_model_manager_create_text_model(editor->text_model_manager, surface);
	text_model_add_listener(entry->model, &text_model_listener, entry);

	return entry;
}

static void
text_entry_destroy(struct text_entry *entry)
{
	text_model_destroy(entry->model);
	free(entry->text);
	free(entry);
}

static void
text_entry_draw(struct text_entry *entry, cairo_t *cr)
{
	cairo_save(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	cairo_rectangle(cr, entry->allocation.x, entry->allocation.y, entry->allocation.width, entry->allocation.height);
	cairo_clip(cr);

	cairo_translate(cr, entry->allocation.x, entry->allocation.y);
	cairo_rectangle(cr, 0, 0, entry->allocation.width, entry->allocation.height);
	cairo_set_source_rgba(cr, 1, 1, 1, 0.5);
	cairo_fill(cr);
	if (entry->active) {
		cairo_rectangle(cr, 0, 0, entry->allocation.width, entry->allocation.height);
		cairo_set_source_rgba(cr, 0, 0, 1, 0.5);
		cairo_stroke(cr);
	}

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_select_font_face(cr, "sans",
			       CAIRO_FONT_SLANT_NORMAL,
			       CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 14);

	cairo_translate(cr, 10, entry->allocation.height / 2);
	cairo_show_text(cr, entry->text);

	cairo_restore(cr);
}

static void
redraw_handler(struct widget *widget, void *data)
{
	struct editor *editor = data;
	cairo_surface_t *surface;
	struct rectangle allocation;
	cairo_t *cr;

	surface = window_get_surface(editor->window);
	widget_get_allocation(editor->widget, &allocation);

	cr = cairo_create(surface);
	cairo_rectangle(cr, allocation.x, allocation.y, allocation.width, allocation.height);
	cairo_clip(cr);

	cairo_translate(cr, allocation.x, allocation.y);

	/* Draw background */
	cairo_push_group(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);	
	cairo_set_source_rgba(cr, 1, 1, 1, 1);
	cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
	cairo_fill(cr);

	/* Entry */
	text_entry_draw(editor->entry, cr);

	/* Editor */
	text_entry_draw(editor->editor, cr);

	cairo_pop_group_to_source(cr);
	cairo_paint(cr);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

static void
text_entry_allocate(struct text_entry *entry, int32_t x, int32_t y,
		    int32_t width, int32_t height)
{
	entry->allocation.x = x;
	entry->allocation.y = y;
	entry->allocation.width = width;
	entry->allocation.height = height;
}

static void
resize_handler(struct widget *widget,
	       int32_t width, int32_t height, void *data)
{
	struct editor *editor = data;

	text_entry_allocate(editor->entry, 20, 20, width - 40, height / 2 - 40);
	text_entry_allocate(editor->editor, 20, height / 2 + 20, width - 40, height / 2 - 40);
}

static int32_t
rectangle_contains(struct rectangle *rectangle, int32_t x, int32_t y)
{
	if (x < rectangle->x || x > rectangle->x + rectangle->width) {
		return 0;
	}

	if (y < rectangle->y || y > rectangle->y + rectangle->height) {
		return 0;
	}

	return 1;
}

static void
text_entry_activate(struct text_entry *entry)
{
	text_model_activate(entry->model);
}

static void
text_entry_deactivate(struct text_entry *entry)
{
	text_model_deactivate(entry->model);
}

static void
button_handler(struct widget *widget,
	       struct input *input, uint32_t time,
	       uint32_t button,
	       enum wl_pointer_button_state state, void *data)
{
	struct editor *editor = data;
	struct rectangle allocation;
	int32_t x, y;

	if (state != WL_POINTER_BUTTON_STATE_PRESSED || button != BTN_LEFT) {
		return;
	}

	input_get_position(input, &x, &y);

	widget_get_allocation(editor->widget, &allocation);
	x -= allocation.x;
	y -= allocation.y;

	int32_t activate_entry = rectangle_contains(&editor->entry->allocation, x, y);
	int32_t activate_editor = rectangle_contains(&editor->editor->allocation, x, y);
	assert(!(activate_entry && activate_editor));

	if (activate_entry) {
		if (editor->editor->active)
			text_entry_deactivate(editor->editor);
		if (!editor->entry->active)
			text_entry_activate(editor->entry);
	} else if (activate_editor) {
		if (editor->entry->active)
			text_entry_deactivate(editor->entry);
		if (!editor->editor->active)
			text_entry_activate(editor->editor);
	} else {
		if (editor->entry->active)
			text_entry_deactivate(editor->entry);
		if (editor->editor->active)
			text_entry_deactivate(editor->editor);
	}
	editor->entry->active = activate_entry;
	editor->editor->active = activate_editor;
	assert(!(editor->entry->active && editor->editor->active));

	widget_schedule_redraw(widget);
}

static void
global_handler(struct wl_display *display, uint32_t id,
	       const char *interface, uint32_t version, void *data)
{
	struct editor *editor = data;

	if (!strcmp(interface, "text_model_manager")) {
		editor->text_model_manager = wl_display_bind(display, id,
							     &text_model_manager_interface);
	}
}

int
main(int argc, char *argv[])
{
	struct editor editor;

	editor.display = display_create(argc, argv);
	if (editor.display == NULL) {
		fprintf(stderr, "failed to create display: %m\n");
		return -1;
	}
	wl_display_add_global_listener(display_get_display(editor.display),
				       global_handler, &editor);


	editor.window = window_create(editor.display);
	editor.widget = frame_create(editor.window, &editor);

	editor.entry = text_entry_create(&editor, "Entry");
	editor.editor = text_entry_create(&editor, "Editor");

	window_set_title(editor.window, "Text Editor");

	widget_set_redraw_handler(editor.widget, redraw_handler);
	widget_set_resize_handler(editor.widget, resize_handler);
	widget_set_button_handler(editor.widget, button_handler);

	window_schedule_resize(editor.window, 500, 400);

	display_run(editor.display);

	text_entry_destroy(editor.entry);
	text_entry_destroy(editor.editor);

	return 0;
}
