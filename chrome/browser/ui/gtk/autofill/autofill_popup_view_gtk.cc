// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autofill_popup_view_gtk.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "ui/base/gtk/gtk_compat.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/gtk/gtk_windowing.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

namespace {
const GdkColor kBorderColor = GDK_COLOR_RGB(0xc7, 0xca, 0xce);
const GdkColor kTextColor = GDK_COLOR_RGB(0x00, 0x00, 0x00);

// The amount of minimum padding between the Autofill value and label in pixels.
const int kMiddlePadding = 10;

// We have a 1 pixel border around the entire results popup.
const int kBorderThickness = 1;

gfx::Rect GetWindowRect(GdkWindow* window) {
  return gfx::Rect(gdk_window_get_width(window),
                   gdk_window_get_height(window));
}

// Returns the rectangle containing the item at position |index| in the popup.
gfx::Rect GetRectForRow(size_t index, int width, int height) {
  return gfx::Rect(0, (index * height), width, height);
}

}  // namespace

AutofillPopupViewGtk::AutofillPopupViewGtk(content::WebContents* web_contents,
                                           GtkWidget* parent)
    : AutofillPopupView(web_contents),
      parent_(parent),
      window_(gtk_window_new(GTK_WINDOW_POPUP)) {
  CHECK(parent != NULL);
  gtk_window_set_resizable(GTK_WINDOW(window_), FALSE);
  gtk_widget_set_app_paintable(window_, TRUE);
  gtk_widget_set_double_buffered(window_, TRUE);

  // Setup the window to ensure it recieves the expose event.
  gtk_widget_add_events(window_, GDK_EXPOSURE_MASK);
  g_signal_connect(window_, "expose-event",
                   G_CALLBACK(HandleExposeThunk), this);

  // Cache the layout so we don't have to create it for every expose.
  layout_ = gtk_widget_create_pango_layout(window_, NULL);

  row_height_ = font_.GetHeight();
}

AutofillPopupViewGtk::~AutofillPopupViewGtk() {
  g_object_unref(layout_);
  gtk_widget_destroy(window_);
}

void AutofillPopupViewGtk::Hide() {
  gtk_widget_hide(window_);
}

void AutofillPopupViewGtk::ShowInternal() {
  gint origin_x, origin_y;
  gdk_window_get_origin(gtk_widget_get_window(parent_), &origin_x, &origin_y);

  // Move the popup to appear right below the text field it is using.
  gtk_window_move(GTK_WINDOW(window_),
                  origin_x + element_bounds().x(),
                  origin_y + element_bounds().y() + element_bounds().height());

  // Find out the maximum bounds required by the popup.
  // TODO(csharp): Once the icon is also displayed it will affect the required
  // size so it will need to be included in the calculation.
  int popup_width = element_bounds().width();
  DCHECK_EQ(autofill_values().size(), autofill_labels().size());
  for (size_t i = 0; i < autofill_values().size(); ++i) {
    popup_width = std::max(popup_width,
                           font_.GetStringWidth(autofill_values()[i]) +
                           kMiddlePadding +
                           font_.GetStringWidth(autofill_labels()[i]));
  }

  gtk_widget_set_size_request(
      window_,
      popup_width,
      row_height_ * autofill_values().size());

  gtk_widget_show(window_);

  GtkWidget* toplevel = gtk_widget_get_toplevel(parent_);
  CHECK(gtk_widget_is_toplevel(toplevel));
  ui::StackPopupWindow(window_, toplevel);
}

gboolean AutofillPopupViewGtk::HandleExpose(GtkWidget* widget,
                                            GdkEventExpose* event) {
  gfx::Rect window_rect = GetWindowRect(event->window);
  gfx::Rect damage_rect = gfx::Rect(event->area);

  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(gtk_widget_get_window(widget)));
  gdk_cairo_rectangle(cr, &event->area);
  cairo_clip(cr);

  // This assert is kinda ugly, but it would be more currently unneeded work
  // to support painting a border that isn't 1 pixel thick.  There is no point
  // in writing that code now, and explode if that day ever comes.
  COMPILE_ASSERT(kBorderThickness == 1, border_1px_implied);
  // Draw the 1px border around the entire window.
  gdk_cairo_set_source_color(cr, &kBorderColor);
  cairo_rectangle(cr, 0, 0, window_rect.width(), window_rect.height());
  cairo_stroke(cr);

  SetupLayout(window_rect, kTextColor);

  int actual_content_width, actual_content_height;
  pango_layout_get_size(layout_, &actual_content_width, &actual_content_height);
  actual_content_width /= PANGO_SCALE;
  actual_content_height /= PANGO_SCALE;

  for (size_t i = 0; i < autofill_values().size(); ++i) {
    gfx::Rect line_rect = GetRectForRow(i, window_rect.width(), row_height_);
    // Only repaint and layout damaged lines.
    if (!line_rect.Intersects(damage_rect))
      continue;

    if (separator_index() == static_cast<int>(i)) {
      int line_y = i * row_height_;

      cairo_save(cr);
      cairo_move_to(cr, 0, line_y);
      cairo_line_to(cr, window_rect.width(), line_y);
      cairo_stroke(cr);
      cairo_restore(cr);
    }

    // Center the text within the line.
    int content_y = std::max(
        line_rect.y(),
        line_rect.y() + ((row_height_ - actual_content_height) / 2));

    // Draw the value.
    gtk_util::SetLayoutText(layout_, autofill_values()[i]);

    cairo_save(cr);
    cairo_move_to(cr, 0, content_y);
    pango_cairo_show_layout(cr, layout_);
    cairo_restore(cr);

    // Draw the label.
    int x_align_left = window_rect.width() -
        font_.GetStringWidth(autofill_labels()[i]);
    gtk_util::SetLayoutText(layout_, autofill_labels()[i]);

    cairo_save(cr);
    cairo_move_to(cr, x_align_left, line_rect.y());
    pango_cairo_show_layout(cr, layout_);
    cairo_restore(cr);
  }

  cairo_destroy(cr);

  return TRUE;
}

void AutofillPopupViewGtk::SetupLayout(const gfx::Rect& window_rect,
                                       const GdkColor& text_color) {
  int allocated_content_width = window_rect.width();
  pango_layout_set_width(layout_, allocated_content_width * PANGO_SCALE);
  pango_layout_set_height(layout_, row_height_ * PANGO_SCALE);

  PangoAttrList* attrs = pango_attr_list_new();

  PangoAttribute* fg_attr = pango_attr_foreground_new(text_color.red,
                                                      text_color.green,
                                                      text_color.blue);
  pango_attr_list_insert(attrs, fg_attr);  // Ownership taken.


  pango_layout_set_attributes(layout_, attrs);  // Ref taken.
  pango_attr_list_unref(attrs);
}
