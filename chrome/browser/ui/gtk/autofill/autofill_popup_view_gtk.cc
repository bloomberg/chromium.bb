// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autofill_popup_view_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/ui_resources_standard.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAutofillClient.h"
#include "ui/base/gtk/gtk_compat.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/gtk/gtk_windowing.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/pango_util.h"
#include "ui/gfx/rect.h"

using WebKit::WebAutofillClient;

namespace {
const GdkColor kBorderColor = GDK_COLOR_RGB(0xc7, 0xca, 0xce);
const GdkColor kHoveredBackgroundColor = GDK_COLOR_RGB(0xcd, 0xcd, 0xcd);
const GdkColor kValueTextColor = GDK_COLOR_RGB(0x00, 0x00, 0x00);
const GdkColor kLabelTextColor = GDK_COLOR_RGB(0x7f, 0x7f, 0x7f);

// The vertical height of each row in pixels.
const int kRowHeight = 24;

// The vertical height of a separator in pixels.
const int kSeparatorHeight = 1;

// The amount of minimum padding between the Autofill value and label in pixels.
const int kLabelPadding = 15;

// The amount of padding between icons in pixels.
const int kIconPadding = 5;

// The amount of padding at the end of the popup in pixels.
const int kEndPadding = 3;

// We have a 1 pixel border around the entire results popup.
const int kBorderThickness = 1;

// Width of the Autofill icons in pixels.
const int kAutofillIconWidth = 25;

// Height of the Autofill icons in pixels.
const int kAutofillIconHeight = 16;

// Width of the delete icon in pixels.
const int kDeleteIconWidth = 16;

// Height of the delete icon in pixels.
const int kDeleteIconHeight = 16;

// Size difference between value text and label text in pixels.
const int kLabelFontSizeDelta = -2;

gfx::Rect GetWindowRect(GdkWindow* window) {
  return gfx::Rect(gdk_window_get_width(window),
                   gdk_window_get_height(window));
}

int GetRowHeight(int unique_id) {
  if (unique_id == WebAutofillClient::MenuItemIDSeparator)
    return kSeparatorHeight;

  return kRowHeight;
}

}  // namespace

AutofillPopupViewGtk::AutofillPopupViewGtk(
    content::WebContents* web_contents,
    GtkThemeService* theme_service,
    AutofillExternalDelegate* external_delegate,
    GtkWidget* parent)
    : AutofillPopupView(web_contents, external_delegate),
      parent_(parent),
      window_(gtk_window_new(GTK_WINDOW_POPUP)),
      theme_service_(theme_service),
      render_view_host_(web_contents->GetRenderViewHost()),
      delete_icon_selected_(false) {
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
  g_signal_connect(window_, "leave-notify-event",
                   G_CALLBACK(HandleLeaveThunk), this);
  g_signal_connect(window_, "motion-notify-event",
                   G_CALLBACK(HandleMotionThunk), this);
  g_signal_connect(window_, "button-release-event",
                   G_CALLBACK(HandleButtonReleaseThunk), this);

  label_font_ = value_font_.DeriveFont(kLabelFontSizeDelta);

  // Cache the layout so we don't have to create it for every expose.
  layout_ = gtk_widget_create_pango_layout(window_, NULL);
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
  GdkRectangle row_rect = GetRectForRow(row, bounds_.width()).ToGdkRectangle();
  GdkWindow* gdk_window = gtk_widget_get_window(window_);
  gdk_window_invalidate_rect(gdk_window, &row_rect, FALSE);
}

void AutofillPopupViewGtk::ResizePopup() {
  bounds_.set_width(GetPopupRequiredWidth());
  bounds_.set_height(GetPopupRequiredHeight());

  gtk_widget_set_size_request(window_, bounds_.width(), bounds_.height());
}

gboolean AutofillPopupViewGtk::HandleButtonRelease(GtkWidget* widget,
                                                   GdkEventButton* event) {
  // We only care about the left click.
  if (event->button != 1)
    return FALSE;

  DCHECK_EQ(selected_line(), LineFromY(event->y));

  if (DeleteIconIsSelected(event->x, event->y))
    RemoveSelectedLine();
  else
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

  SetupLayout(window_rect);

  for (size_t i = 0; i < autofill_values().size(); ++i) {
    gfx::Rect line_rect = GetRectForRow(i, window_rect.width());
    // Only repaint and layout damaged lines.
    if (!line_rect.Intersects(damage_rect))
      continue;

    if (autofill_unique_ids()[i] == WebAutofillClient::MenuItemIDSeparator)
      DrawSeparator(cr, line_rect);
    else
      DrawAutofillEntry(cr, i, line_rect);
  }

  cairo_destroy(cr);

  return TRUE;
}

gboolean AutofillPopupViewGtk::HandleLeave(GtkWidget* widget,
                                           GdkEventCrossing* event) {
  ClearSelectedLine();

  return FALSE;
}

gboolean AutofillPopupViewGtk::HandleMotion(GtkWidget* widget,
                                            GdkEventMotion* event) {
  // TODO(csharp): Only select a line if the motion is still inside the popup.
  // http://www.crbug.com/129559
  int line = LineFromY(event->y);

  SetSelectedLine(line);

  bool delete_icon_selected = DeleteIconIsSelected(event->x, event->y);
  if (delete_icon_selected != delete_icon_selected_) {
    delete_icon_selected_ = delete_icon_selected;
    InvalidateRow(selected_line());
  }

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

void AutofillPopupViewGtk::SetupLayout(const gfx::Rect& window_rect) {
  int allocated_content_width = window_rect.width();
  pango_layout_set_width(layout_, allocated_content_width * PANGO_SCALE);
  pango_layout_set_height(layout_, kRowHeight * PANGO_SCALE);
}

void AutofillPopupViewGtk::SetLayoutText(const string16& text,
                                         const gfx::Font& font,
                                         const GdkColor text_color) {
  PangoAttrList* attrs = pango_attr_list_new();

  PangoAttribute* fg_attr = pango_attr_foreground_new(text_color.red,
                                                      text_color.green,
                                                      text_color.blue);
  pango_attr_list_insert(attrs, fg_attr);  // Ownership taken.

  pango_layout_set_attributes(layout_, attrs);  // Ref taken.
  pango_attr_list_unref(attrs);

  gfx::ScopedPangoFontDescription font_description(font.GetNativeFont());
  pango_layout_set_font_description(layout_, font_description.get());

  gtk_util::SetLayoutText(layout_, text);

  // We add one pixel to the width because if the text fills up the width
  // pango will try to split it over 2 lines.
  int required_width = font.GetStringWidth(text) + 1;

  pango_layout_set_width(layout_, required_width * PANGO_SCALE);
}

void AutofillPopupViewGtk::DrawSeparator(cairo_t* cairo_context,
                                         const gfx::Rect& separator_rect) {
  cairo_save(cairo_context);
  cairo_move_to(cairo_context, 0, separator_rect.y());
  cairo_line_to(cairo_context,
                separator_rect.width(),
                separator_rect.y() + separator_rect.height());
  cairo_stroke(cairo_context);
  cairo_restore(cairo_context);
}

void AutofillPopupViewGtk::DrawAutofillEntry(cairo_t* cairo_context,
                                             size_t index,
                                             const gfx::Rect& entry_rect) {
  if (selected_line() == static_cast<int>(index)) {
    gdk_cairo_set_source_color(cairo_context, &kHoveredBackgroundColor);
    cairo_rectangle(cairo_context, entry_rect.x(), entry_rect.y(),
                    entry_rect.width(), entry_rect.height());
    cairo_fill(cairo_context);
  }

  // Draw the value.
  SetLayoutText(autofill_values()[index], value_font_, kValueTextColor);
  int value_text_width = value_font_.GetStringWidth(autofill_values()[index]);

  // Center the text within the line.
  int value_content_y = std::max(
      entry_rect.y(),
      entry_rect.y() + ((kRowHeight - value_font_.GetHeight()) / 2));

  bool is_rtl = base::i18n::IsRTL();
  cairo_save(cairo_context);
  cairo_move_to(cairo_context,
                is_rtl ? entry_rect.width() - value_text_width - kEndPadding :
                    kEndPadding,
                value_content_y);
  pango_cairo_show_layout(cairo_context, layout_);
  cairo_restore(cairo_context);

  // Use this to figure out where all the other Autofill items should be placed.
  int x_align_left = is_rtl ? kEndPadding : entry_rect.width() - kEndPadding;

  // Draw the delete icon, if one is needed.
  if (CanDelete(autofill_unique_ids()[index])) {
    x_align_left += is_rtl ? 0 : -kDeleteIconWidth;

    const gfx::Image* delete_icon;
    if (static_cast<int>(index) == selected_line() && delete_icon_selected_)
      delete_icon = theme_service_->GetImageNamed(IDR_CLOSE_BAR_H);
    else
      delete_icon = theme_service_->GetImageNamed(IDR_CLOSE_BAR);

    // TODO(csharp): Create a custom resource for the delete icon.
    // http://www.crbug.com/131801
    cairo_save(cairo_context);
    gtk_util::DrawFullImage(
        cairo_context,
        window_,
        delete_icon,
        x_align_left,
        entry_rect.y() + ((kRowHeight - kDeleteIconHeight) / 2));
    cairo_restore(cairo_context);
    cairo_save(cairo_context);

    x_align_left += is_rtl ? kDeleteIconWidth + kIconPadding : -kIconPadding;
  }

  // Draw the Autofill icon, if one exists
  if (!autofill_icons()[index].empty()) {
    int icon = GetIconResourceID(autofill_icons()[index]);
    DCHECK_NE(-1, icon);
    int icon_y = entry_rect.y() + ((kRowHeight - kAutofillIconHeight) / 2);

    x_align_left += is_rtl ? 0 : -kAutofillIconWidth;

    cairo_save(cairo_context);
    gtk_util::DrawFullImage(cairo_context,
                            window_,
                            theme_service_->GetImageNamed(icon),
                            x_align_left,
                            icon_y);
    cairo_restore(cairo_context);

    x_align_left += is_rtl ? kAutofillIconWidth + kIconPadding : -kIconPadding;
  }

  // Draw the label text.
  SetLayoutText(autofill_labels()[index], label_font_, kLabelTextColor);
  x_align_left +=
      is_rtl ? 0 : -label_font_.GetStringWidth(autofill_labels()[index]);

  // Center the text within the line.
  int label_content_y = std::max(
      entry_rect.y(),
      entry_rect.y() + ((kRowHeight - label_font_.GetHeight()) / 2));

  cairo_save(cairo_context);
  cairo_move_to(cairo_context, x_align_left, label_content_y);
  pango_cairo_show_layout(cairo_context, layout_);
  cairo_restore(cairo_context);
}

void AutofillPopupViewGtk::SetBounds() {
  gint origin_x, origin_y;
  gdk_window_get_origin(gtk_widget_get_window(parent_), &origin_x, &origin_y);

  GdkScreen* screen = gtk_widget_get_screen(parent_);
  gint screen_height = gdk_screen_get_height(screen);

  int bottom_of_field = origin_y + element_bounds().y() +
      element_bounds().height();
  int popup_height =  GetPopupRequiredHeight();

  // Find the correct top position of the popup so that is doesn't go off
  // the screen.
  int top_of_popup = 0;
  if (screen_height < bottom_of_field + popup_height) {
    // The popup must appear above the field.
    top_of_popup = origin_y + element_bounds().y() - popup_height;
  } else {
    // The popup can appear below the field.
    top_of_popup = bottom_of_field;
  }

  bounds_.SetRect(
      origin_x + element_bounds().x(),
      top_of_popup,
      GetPopupRequiredWidth(),
      popup_height);
}

int AutofillPopupViewGtk::GetPopupRequiredWidth() {
  int popup_width = element_bounds().width();
  DCHECK_EQ(autofill_values().size(), autofill_labels().size());
  for (size_t i = 0; i < autofill_values().size(); ++i) {
    int row_size = kEndPadding +
        value_font_.GetStringWidth(autofill_values()[i]) +
        kLabelPadding +
        label_font_.GetStringWidth(autofill_labels()[i]);

    // Add the Autofill icon size, if required.
    if (!autofill_icons()[i].empty())
      row_size += kAutofillIconWidth + kIconPadding;

    // Add delete icon, if required.
    if (CanDelete(autofill_unique_ids()[i]))
      row_size += kDeleteIconWidth + kIconPadding;

    // Add the padding at the end
    row_size += kEndPadding;

    popup_width = std::max(popup_width, row_size);
  }

  return popup_width;
}

int AutofillPopupViewGtk::GetPopupRequiredHeight() {
  int popup_height = 0;

  for (size_t i = 0; i < autofill_unique_ids().size(); ++i) {
    popup_height += GetRowHeight(autofill_unique_ids()[i]);
  }

  return popup_height;
}

int AutofillPopupViewGtk::LineFromY(int y) {
  int current_height = 0;

  for (size_t i = 0; i < autofill_unique_ids().size(); ++i) {
    current_height += GetRowHeight(autofill_unique_ids()[i]);

    if (y <= current_height)
      return i;
  }

  // The y value goes beyond the popup so stop the selection at the last line.
  return autofill_unique_ids().size() - 1;
}

gfx::Rect AutofillPopupViewGtk::GetRectForRow(size_t row, int width) {
  int top = 0;
  for (size_t i = 0; i < row; ++i) {
    top += GetRowHeight(autofill_unique_ids()[i]);
  }

  return gfx::Rect(0, top, width, GetRowHeight(autofill_unique_ids()[row]));
}

bool AutofillPopupViewGtk::DeleteIconIsSelected(int x, int y) {
  if (!CanDelete(selected_line()))
    return false;

  int row_start_y = 0;
  for (int i = 0; i < selected_line(); ++i) {
    row_start_y += GetRowHeight(autofill_unique_ids()[i]);
  }

  gfx::Rect delete_icon_bounds = gfx::Rect(
      GetPopupRequiredWidth() - kDeleteIconWidth - kIconPadding,
      row_start_y + ((kRowHeight - kDeleteIconHeight) / 2),
      kDeleteIconWidth,
      kDeleteIconHeight);

  return delete_icon_bounds.Contains(x, y);
}
