/*
 * Copyright © 2012 Openismus GmbH
 * Copyright © 2012 Intel Corporation
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

static const char *font_name = "sans-serif";
static int font_size = 14;

struct text_layout {
	cairo_glyph_t *glyphs;
	int num_glyphs;
	cairo_text_cluster_t *clusters;
	int num_clusters;
	cairo_text_cluster_flags_t cluster_flags;
	cairo_scaled_font_t *font;
};

struct text_entry {
	struct widget *widget;
	struct window *window;
	char *text;
	int active;
	uint32_t cursor;
	uint32_t anchor;
	char *preedit_text;
	uint32_t preedit_cursor;
	struct text_model *model;
	struct text_layout *layout;
};

struct editor {
	struct text_model_factory *text_model_factory;
	struct display *display;
	struct window *window;
	struct widget *widget;
	struct text_entry *entry;
	struct text_entry *editor;
};

static struct text_layout *
text_layout_create(void)
{
	struct text_layout *layout;
	cairo_surface_t *surface;
	cairo_t *cr;

	layout = malloc(sizeof *layout);
	if (!layout)
		return NULL;

	layout->glyphs = NULL;
	layout->num_glyphs = 0;

	layout->clusters = NULL;
	layout->num_clusters = 0;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
	cr = cairo_create(surface);
	cairo_set_font_size(cr, font_size);
	cairo_select_font_face(cr, font_name, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	layout->font = cairo_get_scaled_font(cr);
	cairo_scaled_font_reference(layout->font);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	return layout;
}

static void
text_layout_destroy(struct text_layout *layout)
{
	if (layout->glyphs)
		cairo_glyph_free(layout->glyphs);

	if (layout->clusters)
		cairo_text_cluster_free(layout->clusters);

	cairo_scaled_font_destroy(layout->font);

	free(layout);
}

static void
text_layout_set_text(struct text_layout *layout,
		     const char *text)
{
	if (layout->glyphs)
		cairo_glyph_free(layout->glyphs);

	if (layout->clusters)
		cairo_text_cluster_free(layout->clusters);

	layout->glyphs = NULL;
	layout->num_glyphs = 0;
	layout->clusters = NULL;
	layout->num_clusters = 0;

	cairo_scaled_font_text_to_glyphs(layout->font, 0, 0, text, -1,
					 &layout->glyphs, &layout->num_glyphs,
					 &layout->clusters, &layout->num_clusters,
					 &layout->cluster_flags);
}

static void
text_layout_draw(struct text_layout *layout, cairo_t *cr)
{
	cairo_save(cr);
	cairo_set_scaled_font(cr, layout->font);
	cairo_show_glyphs(cr, layout->glyphs, layout->num_glyphs);
	cairo_restore(cr);
}

static void
text_layout_extents(struct text_layout *layout, cairo_text_extents_t *extents)
{
	cairo_scaled_font_glyph_extents(layout->font,
					layout->glyphs, layout->num_glyphs,
					extents);
}

static int
text_layout_xy_to_index(struct text_layout *layout, double x, double y)
{
	cairo_text_extents_t extents;
	int i;

	cairo_scaled_font_glyph_extents(layout->font,
					layout->glyphs, layout->num_glyphs,
					&extents);

	for (i = 1; i < layout->num_glyphs; i++) {
		if (layout->glyphs[i].x >= x) {
			return i - 1;
		}
	}

	if (x >= layout->glyphs[layout->num_glyphs - 1].x && x < extents.width)
		return layout->num_glyphs - 1;

	return layout->num_glyphs;
}

static void
text_layout_index_to_pos(struct text_layout *layout, uint32_t index, cairo_rectangle_t *pos)
{
	cairo_text_extents_t extents;

	if (!pos)
		return;

	cairo_scaled_font_glyph_extents(layout->font,
					layout->glyphs, layout->num_glyphs,
					&extents);

	if ((int)index >= layout->num_glyphs) {
		pos->x = extents.x_advance;
		pos->y = layout->num_glyphs ? layout->glyphs[layout->num_glyphs - 1].y : 0;
		pos->width = 1;
		pos->height = extents.height;
		return;
	}

	pos->x = layout->glyphs[index].x;
	pos->y = layout->glyphs[index].y;
	pos->width = (int)index < layout->num_glyphs - 1 ? layout->glyphs[index + 1].x : extents.x_advance - pos->x;
	pos->height = extents.height;
}

static void
text_layout_get_cursor_pos(struct text_layout *layout, int index, cairo_rectangle_t *pos)
{
	text_layout_index_to_pos(layout, index, pos);
	pos->width = 1;
}

static void text_entry_redraw_handler(struct widget *widget, void *data);
static void text_entry_button_handler(struct widget *widget,
				      struct input *input, uint32_t time,
				      uint32_t button,
				      enum wl_pointer_button_state state, void *data);
static void text_entry_insert_at_cursor(struct text_entry *entry, const char *text);

static void
text_model_commit_string(void *data,
			 struct text_model *text_model,
			 const char *text,
			 uint32_t index)
{
	struct text_entry *entry = data;

	if (index > strlen(text)) {
		fprintf(stderr, "Invalid cursor index %d\n", index);
		index = strlen(text);
	}

	text_entry_insert_at_cursor(entry, text);

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

static void
text_model_activated(void *data,
		     struct text_model *text_model)
{
	struct text_entry *entry = data;

	entry->active = 1;

	widget_schedule_redraw(entry->widget);
}

static void
text_model_deactivated(void *data,
		       struct text_model *text_model)
{
	struct text_entry *entry = data;

	entry->active = 0;

	widget_schedule_redraw(entry->widget);
}

static const struct text_model_listener text_model_listener = {
	text_model_commit_string,
	text_model_preedit_string,
	text_model_preedit_styling,
	text_model_key,
	text_model_selection_replacement,
	text_model_direction,
	text_model_locale,
	text_model_activated,
	text_model_deactivated
};

static struct text_entry*
text_entry_create(struct editor *editor, const char *text)
{
	struct text_entry *entry;

	entry = malloc(sizeof *entry);

	entry->widget = widget_add_widget(editor->widget, entry);
	entry->window = editor->window;
	entry->text = strdup(text);
	entry->active = 0;
	entry->cursor = strlen(text);
	entry->anchor = entry->cursor;
	entry->preedit_text = NULL;
	entry->preedit_cursor = 0;
	entry->model = text_model_factory_create_text_model(editor->text_model_factory);
	text_model_add_listener(entry->model, &text_model_listener, entry);

	entry->layout = text_layout_create();
	text_layout_set_text(entry->layout, entry->text);

	widget_set_redraw_handler(entry->widget, text_entry_redraw_handler);
	widget_set_button_handler(entry->widget, text_entry_button_handler);

	return entry;
}

static void
text_entry_destroy(struct text_entry *entry)
{
	widget_destroy(entry->widget);
	text_model_destroy(entry->model);
	text_layout_destroy(entry->layout);
	free(entry->text);
	free(entry);
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

	cairo_pop_group_to_source(cr);
	cairo_paint(cr);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

static void
text_entry_allocate(struct text_entry *entry, int32_t x, int32_t y,
		    int32_t width, int32_t height)
{
	widget_set_allocation(entry->widget, x, y, width, height);
}

static void
resize_handler(struct widget *widget,
	       int32_t width, int32_t height, void *data)
{
	struct editor *editor = data;
	struct rectangle allocation;

	widget_get_allocation(editor->widget, &allocation);

	text_entry_allocate(editor->entry,
			    allocation.x + 20, allocation.y + 20,
			    width - 40, height / 2 - 40);
	text_entry_allocate(editor->editor,
			    allocation.x + 20, allocation.y + height / 2 + 20,
			    width - 40, height / 2 - 40);
}

static void
text_entry_activate(struct text_entry *entry,
		    struct wl_seat *seat)
{
	struct wl_surface *surface = window_get_wl_surface(entry->window);

	text_model_activate(entry->model,
			    seat,
			    surface);
}

static void
text_entry_deactivate(struct text_entry *entry,
		      struct wl_seat *seat)
{
	text_model_deactivate(entry->model,
			      seat);
}

static void
text_entry_update_layout(struct text_entry *entry)
{
	char *text;

	assert(((unsigned int)entry->cursor) <= strlen(entry->text));

	if (!entry->preedit_text) {
		text_layout_set_text(entry->layout, entry->text);
		return;
	}

	text = malloc(strlen(entry->text) + strlen(entry->preedit_text) + 1);
	strncpy(text, entry->text, entry->cursor);
	strcpy(text + entry->cursor, entry->preedit_text);
	strcpy(text + entry->cursor + strlen(entry->preedit_text),
	       entry->text + entry->cursor);

	text_layout_set_text(entry->layout, text);
	free(text);

	widget_schedule_redraw(entry->widget);

	text_model_set_surrounding_text(entry->model,
					entry->text,
					entry->cursor,
					entry->anchor);
}

static void
text_entry_insert_at_cursor(struct text_entry *entry, const char *text)
{
	char *new_text = malloc(strlen(entry->text) + strlen(text) + 1);

	strncpy(new_text, entry->text, entry->cursor);
	strcpy(new_text + entry->cursor, text);
	strcpy(new_text + entry->cursor + strlen(text),
	       entry->text + entry->cursor);

	free(entry->text);
	entry->text = new_text;
	entry->cursor += strlen(text);
	entry->anchor += strlen(text);

	text_entry_update_layout(entry);
}

static void
text_entry_set_preedit(struct text_entry *entry,
		       const char *preedit_text,
		       int preedit_cursor)
{
	if (entry->preedit_text) {
		free(entry->preedit_text);
		entry->preedit_text = NULL;
		entry->preedit_cursor = 0;
	}

	if (!preedit_text)
		return;

	entry->preedit_text = strdup(preedit_text);
	entry->preedit_cursor = preedit_cursor;

	text_entry_update_layout(entry);
}

static void
text_entry_set_cursor_position(struct text_entry *entry,
			       int32_t x, int32_t y)
{
	entry->cursor = text_layout_xy_to_index(entry->layout, x, y);

	if (entry->cursor >= entry->preedit_cursor) {
		entry->cursor -= entry->preedit_cursor;
	}

	text_entry_update_layout(entry);

	widget_schedule_redraw(entry->widget);
}

static void
text_entry_set_anchor_position(struct text_entry *entry,
			       int32_t x, int32_t y)
{
	entry->anchor = text_layout_xy_to_index(entry->layout, x, y);

	widget_schedule_redraw(entry->widget);
}

static void
text_entry_draw_selection(struct text_entry *entry, cairo_t *cr)
{
	cairo_text_extents_t extents;
	uint32_t start_index = entry->anchor < entry->cursor ? entry->anchor : entry->cursor;
	uint32_t end_index = entry->anchor < entry->cursor ? entry->cursor : entry->anchor;
	cairo_rectangle_t start;
	cairo_rectangle_t end;

	if (entry->anchor == entry->cursor)
		return;

	text_layout_extents(entry->layout, &extents);

	text_layout_index_to_pos(entry->layout, start_index, &start);
	text_layout_index_to_pos(entry->layout, end_index, &end);

	cairo_save (cr);

	cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 1.0);
	cairo_rectangle(cr,
			start.x, extents.y_bearing + extents.height + 2,
			end.x - start.x, -extents.height - 4);
	cairo_fill(cr);

	cairo_rectangle(cr,
			start.x, extents.y_bearing + extents.height,
			end.x - start.x, -extents.height);
	cairo_clip(cr);
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
	text_layout_draw(entry->layout, cr);

	cairo_restore (cr);
}

static void
text_entry_draw_cursor(struct text_entry *entry, cairo_t *cr)
{
	cairo_text_extents_t extents;
	cairo_rectangle_t cursor_pos;

	text_layout_extents(entry->layout, &extents);
	text_layout_get_cursor_pos(entry->layout,
				   entry->cursor + entry->preedit_cursor,
				   &cursor_pos);

	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, cursor_pos.x, extents.y_bearing + extents.height + 2);
	cairo_line_to(cr, cursor_pos.x, extents.y_bearing - 2);
	cairo_stroke(cr);
}

static void
text_entry_draw_preedit(struct text_entry *entry, cairo_t *cr)
{
	cairo_text_extents_t extents;
	cairo_rectangle_t start;
	cairo_rectangle_t end;

	if (!entry->preedit_text)
		return;

	text_layout_extents(entry->layout, &extents);

	text_layout_index_to_pos(entry->layout, entry->cursor, &start);
	text_layout_index_to_pos(entry->layout,
				 entry->cursor + strlen(entry->preedit_text),
				 &end);

	cairo_save (cr);

	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
	cairo_rectangle(cr,
			start.x, 0,
			end.x - start.x, 1);
	cairo_fill(cr);

	cairo_restore (cr);
}

static void
text_entry_redraw_handler(struct widget *widget, void *data)
{
	struct text_entry *entry = data;
	cairo_surface_t *surface;
	struct rectangle allocation;
	cairo_t *cr;

	surface = window_get_surface(entry->window);
	widget_get_allocation(entry->widget, &allocation);

	cr = cairo_create(surface);
	cairo_rectangle(cr, allocation.x, allocation.y, allocation.width, allocation.height);
	cairo_clip(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

	cairo_push_group(cr);
	cairo_translate(cr, allocation.x, allocation.y);

	cairo_set_source_rgba(cr, 1, 1, 1, 1);
	cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
	cairo_fill(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	if (entry->active) {
		cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
		cairo_set_line_width (cr, 3);
		cairo_set_source_rgba(cr, 0, 0, 1, 1.0);
		cairo_stroke(cr);
	}

	cairo_set_source_rgba(cr, 0, 0, 0, 1);

	cairo_translate(cr, 10, allocation.height / 2);
	text_layout_draw(entry->layout, cr);

	text_entry_draw_selection(entry, cr);

	text_entry_draw_cursor(entry, cr);

	text_entry_draw_preedit(entry, cr);

	cairo_pop_group_to_source(cr);
	cairo_paint(cr);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

static int
text_entry_motion_handler(struct widget *widget,
			  struct input *input, uint32_t time,
			  float x, float y, void *data)
{
	struct text_entry *entry = data;
	struct rectangle allocation;

	widget_get_allocation(entry->widget, &allocation);

	text_entry_set_cursor_position(entry,
				       x - allocation.x,
				       y - allocation.y);

	return CURSOR_IBEAM;
}

static void
text_entry_button_handler(struct widget *widget,
			  struct input *input, uint32_t time,
			  uint32_t button,
			  enum wl_pointer_button_state state, void *data)
{
	struct text_entry *entry = data;
	struct rectangle allocation;
	int32_t x, y;

	widget_get_allocation(entry->widget, &allocation);
	input_get_position(input, &x, &y);

	if (button != BTN_LEFT) {
		return;
	}

	text_entry_set_cursor_position(entry,
				       x - allocation.x,
				       y - allocation.y);

	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		struct wl_seat *seat = input_get_seat(input);

		text_entry_activate(entry, seat);

		text_entry_set_anchor_position(entry,
					       x - allocation.x,
					       y - allocation.y);

		widget_set_motion_handler(entry->widget, text_entry_motion_handler);
	} else {
		widget_set_motion_handler(entry->widget, NULL);
	}
}

static void
editor_button_handler(struct widget *widget,
		      struct input *input, uint32_t time,
		      uint32_t button,
		      enum wl_pointer_button_state state, void *data)
{
	struct editor *editor = data;

	if (button != BTN_LEFT) {
		return;
	}

	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		struct wl_seat *seat = input_get_seat(input);

		text_entry_deactivate(editor->entry, seat);
		text_entry_deactivate(editor->editor, seat);
	}
}

static void
global_handler(struct wl_display *display, uint32_t id,
	       const char *interface, uint32_t version, void *data)
{
	struct editor *editor = data;

	if (!strcmp(interface, "text_model_factory")) {
		editor->text_model_factory = wl_display_bind(display, id,
							     &text_model_factory_interface);
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
	text_entry_set_preedit(editor.editor, "preedit", strlen("preedit"));

	window_set_title(editor.window, "Text Editor");

	widget_set_redraw_handler(editor.widget, redraw_handler);
	widget_set_resize_handler(editor.widget, resize_handler);
	widget_set_button_handler(editor.widget, editor_button_handler);

	window_schedule_resize(editor.window, 500, 400);

	display_run(editor.display);

	text_entry_destroy(editor.entry);
	text_entry_destroy(editor.editor);

	return 0;
}
