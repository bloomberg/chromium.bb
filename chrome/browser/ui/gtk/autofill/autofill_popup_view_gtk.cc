// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autofill_popup_view_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/gtk/gtk_compat.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/gtk/gtk_windowing.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

namespace {
const GdkColor kBorderColor = GDK_COLOR_RGB(0xc7, 0xca, 0xce);
const GdkColor kHoveredBackgroundColor = GDK_COLOR_RGB(0x0CD, 0xCD, 0xCD);
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

AutofillPopupViewGtk::AutofillPopupViewGtk(
    content::WebContents* web_contents,
    AutofillExternalDelegate* external_delegate,
    GtkWidget* parent)
    : AutofillPopupView(web_contents, external_delegate),
      parent_(parent),
      window_(gtk_window_new(GTK_WINDOW_POPUP)),
      render_view_host_(web_contents->GetRenderViewHost()) {
  CHECK(parent != NULL);
  gtk_window_set_resizable(GTK_WINDOW(window_), FALSE);
  gtk_widget_set_app_paintable(window_, TRUE);
  gtk_widget_set_double_buffered(window_, TRUE);

  // Setup the window to ensure it receives the expose event.
  gtk_widget_add_events(window_, GDK_BUTTON_MOTION_MASK |
                                 GDK_BUTTON_RELEASE_MASK |
                                 GDK_EXPOSURE_MASK |
                                 GDK_POINTER_MOTION_MASK);
  g_signal_connect(window_, "expose-event",
                   G_CALLBACK(HandleExposeThunk), this);

  g_signal_connect(window_, "motion-notify-event",
                   G_CALLBACK(HandleMotionThunk), this);
  g_signal_connect(window_, "button-release-event",
                   G_CALLBACK(HandleButtonReleaseThunk), this);

  // Cache the layout so we don't have to create it for every expose.
  layout_ = gtk_widget_create_pango_layout(window_, NULL);

  row_height_ = font_.GetHeight();
}

AutofillPopupViewGtk::~AutofillPopupViewGtk() {
  g_object_unref(layout_);
  gtk_widget_destroy(window_);
}

void AutofillPopupViewGtk::ShowInternal() {
  SetBounds();
  gtk_window_move(GTK_WINDOW(window_), bounds_.x(), bounds_.y());

  ResizePopup();

  render_view_host_->AddKeyboardListener(this);

  gtk_widget_show(window_);

  GtkWidget* toplevel = gtk_widget_get_toplevel(parent_);
  CHECK(gtk_widget_is_toplevel(toplevel));
  ui::StackPopupWindow(window_, toplevel);
}

void AutofillPopupViewGtk::HideInternal() {
  render_view_host_->RemoveKeyboardListener(this);

  gtk_widget_hide(window_);
}

void AutofillPopupViewGtk::InvalidateRow(size_t row) {
  GdkRectangle row_rect = GetRectForRow(
      row, bounds_.width(), row_height_).ToGdkRectangle();
  GdkWindow* gdk_window = gtk_widget_get_window(window_);
  gdk_window_invalidate_rect(gdk_window, &row_rect, FALSE);
}

void AutofillPopupViewGtk::ResizePopup() {
  gtk_widget_set_size_request(
      window_,
      GetPopupRequiredWidth(),
      row_height_ * autofill_values().size());
}

gboolean AutofillPopupViewGtk::HandleButtonRelease(GtkWidget* widget,
                                                   GdkEventButton* event) {
  // We only care about the left click.
  if (event->button != 1)
    return FALSE;

  DCHECK_EQ(selected_line(), LineFromY(event->y));

  AcceptSelectedLine();

  return TRUE;
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

    if (IsSeparatorIndex(i)) {
      int line_y = i * row_height_;

      cairo_save(cr);
      cairo_move_to(cr, 0, line_y);
      cairo_line_to(cr, window_rect.width(), line_y);
      cairo_stroke(cr);
      cairo_restore(cr);
    }

    if (selected_line() == static_cast<int>(i)) {
      gdk_cairo_set_source_color(cr, &kHoveredBackgroundColor);
      cairo_rectangle(cr, line_rect.x(), line_rect.y(),
                      line_rect.width(), line_rect.height());
      cairo_fill(cr);
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

gboolean AutofillPopupViewGtk::HandleMotion(GtkWidget* widget,
                                            GdkEventMotion* event) {
  int line = LineFromY(event->y);

  SetSelectedLine(line);

  return TRUE;
}

bool AutofillPopupViewGtk::HandleKeyPressEvent(GdkEventKey* event) {
  // Filter modifier to only include accelerator modifiers.
  guint modifier = event->state & gtk_accelerator_get_default_mod_mask();

  switch (event->keyval) {
    case GDK_Up:
      SelectPreviousLine();
      return true;
    case GDK_Down:
      SelectNextLine();
      return true;
    case GDK_Page_Up:
      SetSelectedLine(0);
      return true;
    case GDK_Page_Down:
      SetSelectedLine(autofill_values().size() - 1);
      return true;
    case GDK_Escape:
      Hide();
      return true;
    case GDK_Delete:
    case GDK_KP_Delete:
      return (modifier == GDK_SHIFT_MASK) && RemoveSelectedLine();
    case GDK_Return:
    case GDK_KP_Enter:
      return AcceptSelectedLine();
  }

  return false;
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

void AutofillPopupViewGtk::SetBounds() {
  gint origin_x, origin_y;
  gdk_window_get_origin(gtk_widget_get_window(parent_), &origin_x, &origin_y);

  GdkScreen* screen = gtk_widget_get_screen(parent_);
  gint screen_height = gdk_screen_get_height(screen);

  int bottom_of_field = origin_y + element_bounds().y() +
      element_bounds().height();
  int popup_size =  row_height_ * autofill_values().size();

  // Find the correct top position of the popup so that is doesn't go off
  // the screen.
  int top_of_popup = 0;
  if (screen_height < bottom_of_field + popup_size) {
    // The popup must appear above the field.
    top_of_popup = origin_y + element_bounds().y() - popup_size;
  } else {
    // The popup can appear below the field.
    top_of_popup = bottom_of_field;
  }

  bounds_.SetRect(
      origin_x + element_bounds().x(),
      top_of_popup,
      GetPopupRequiredWidth(),
      popup_size);
}

int AutofillPopupViewGtk::GetPopupRequiredWidth() {
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

  return popup_width;
}

int AutofillPopupViewGtk::LineFromY(int y) {
  return y / row_height_;
}
