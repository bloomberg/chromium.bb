// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_titlebar_gtk.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_window_gtk.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/gtk_compat.h"
#include "ui/gfx/skia_utils_gtk.h"

namespace {

// Spacing between buttons of panel's titlebar.
const int kPanelButtonSpacing = 7;

// Spacing around outside of panel's titlebar buttons.
const int kPanelButtonOuterPadding = 7;

// Markup for painting title as bold.
const char* const kTitleMarkupPrefix = "<span font_weight='bold'>";
const char* const kTitleMarkupSuffix = "</span>";

// Colors used to draw title in attention mode.
const SkColor kAttentionTitleTextDefaultColor = SK_ColorWHITE;

// Alpha value used in drawing inactive titlebar under non-default theme.
const U8CPU kInactiveAlphaBlending = 0x80;

SkColor BlendSkColorWithAlpha(SkColor fg_color, SkColor bg_color, U8CPU alpha) {
  if (alpha == 255)
      return fg_color;
  double fg_ratio = alpha / 255.0;
  double bg_ratio = 1.0 - fg_ratio;
  return SkColorSetRGB(
      (SkColorGetR(fg_color) * fg_ratio + SkColorGetR(bg_color) * bg_ratio),
      (SkColorGetG(fg_color) * fg_ratio + SkColorGetG(bg_color) * bg_ratio),
      (SkColorGetB(fg_color) * fg_ratio + SkColorGetB(bg_color) * bg_ratio));
}

}  // namespace

PanelBrowserTitlebarGtk::PanelBrowserTitlebarGtk(
    PanelBrowserWindowGtk* browser_window, GtkWindow* window)
    : BrowserTitlebar(browser_window, window),
      browser_window_(browser_window) {
}

PanelBrowserTitlebarGtk::~PanelBrowserTitlebarGtk() {
}

SkColor PanelBrowserTitlebarGtk::GetTextColor() const {
  PanelBrowserWindowGtk::PaintState paint_state =
      browser_window_->GetPaintState();
  if (paint_state == PanelBrowserWindowGtk::PAINT_FOR_ATTENTION)
    return kAttentionTitleTextDefaultColor;

  return paint_state == PanelBrowserWindowGtk::PAINT_AS_ACTIVE ?
      theme_service()->GetColor(ThemeService::COLOR_TAB_TEXT) :
      BlendSkColorWithAlpha(
          theme_service()->GetColor(ThemeService::COLOR_BACKGROUND_TAB_TEXT),
          theme_service()->GetColor(ThemeService::COLOR_TOOLBAR),
          kInactiveAlphaBlending);
}

void PanelBrowserTitlebarGtk::UpdateButtonBackground(CustomDrawButton* button) {
  // Don't need to update background since we're using transparent background.
}

void PanelBrowserTitlebarGtk::UpdateTitleAndIcon() {
  DCHECK(app_mode_title());

  std::string title =
      UTF16ToUTF8(browser_window_->browser()->GetWindowTitleForCurrentTab());

  // Add the markup to show the title as bold.
  gchar* escaped_title = g_markup_escape_text(title.c_str(), -1);
  gchar* title_with_markup = g_strconcat(kTitleMarkupPrefix,
                                         escaped_title,
                                         kTitleMarkupSuffix,
                                         NULL);
  gtk_label_set_markup(GTK_LABEL(app_mode_title()), title_with_markup);
  g_free(escaped_title);
  g_free(title_with_markup);
}

bool PanelBrowserTitlebarGtk::BuildButton(const std::string& button_token,
                                          bool left_side) {
  // Panel only shows close and minimize/restore buttons.
  if (button_token != "close" && button_token != "minimize")
    return false;

  if (!BrowserTitlebar::BuildButton(button_token, left_side))
    return false;

  if (button_token == "minimize") {
    // Create unminimze button, used to expand the minimized panel.
    unminimize_button_.reset(CreateTitlebarButton("unminimize", left_side));

    // We control visibility of minimize and unminimize buttons.
    gtk_widget_set_no_show_all(minimize_button()->widget(), TRUE);
    gtk_widget_set_no_show_all(unminimize_button_->widget(), TRUE);
  }
  return true;
}

void PanelBrowserTitlebarGtk::GetButtonResources(const std::string& button_name,
                                                 int* normal_image_id,
                                                 int* pressed_image_id,
                                                 int* hover_image_id,
                                                 int* tooltip_id) const {
  if (button_name == "close") {
    *normal_image_id = IDR_PANEL_CLOSE;
    *pressed_image_id = 0;
    *hover_image_id = IDR_PANEL_CLOSE_H;
    *tooltip_id = IDS_PANEL_CLOSE_TOOLTIP;
  } else if (button_name == "minimize") {
    *normal_image_id = IDR_PANEL_MINIMIZE;
    *pressed_image_id = 0;
    *hover_image_id = IDR_PANEL_MINIMIZE_H;
    *tooltip_id = IDS_PANEL_MINIMIZE_TOOLTIP;
  } else if (button_name == "unminimize") {
    *normal_image_id = IDR_PANEL_RESTORE;
    *pressed_image_id = 0;
    *hover_image_id = IDR_PANEL_RESTORE_H;
    *tooltip_id = IDS_PANEL_RESTORE_TOOLTIP;
  }
}

int PanelBrowserTitlebarGtk::GetButtonOuterPadding() const {
  return kPanelButtonOuterPadding;
}

int PanelBrowserTitlebarGtk::GetButtonSpacing() const {
  return kPanelButtonSpacing;
}

void PanelBrowserTitlebarGtk::UpdateMinimizeRestoreButtonVisibility() {
  if (!unminimize_button_.get() || !minimize_button())
    return;

  Panel* panel = browser_window_->panel();
  gtk_widget_set_visible(minimize_button()->widget(), panel->CanMinimize());
  gtk_widget_set_visible(unminimize_button_->widget(), panel->CanRestore());
}

void PanelBrowserTitlebarGtk::HandleButtonClick(GtkWidget* button) {
  if (close_button() && close_button()->widget() == button) {
    browser_window_->panel()->Close();
    return;
  }

  GdkEvent* event = gtk_get_current_event();
  DCHECK(event && event->type == GDK_BUTTON_RELEASE);

  if (minimize_button() && minimize_button()->widget() == button) {
    browser_window_->panel()->OnMinimizeButtonClicked(
        (event->button.state & GDK_CONTROL_MASK) ?
            panel::APPLY_TO_ALL : panel::NO_MODIFIER);
  } else if (unminimize_button_.get() &&
             unminimize_button_->widget() == button) {
    browser_window_->panel()->OnRestoreButtonClicked(
        (event->button.state & GDK_CONTROL_MASK) ?
            panel::APPLY_TO_ALL : panel::NO_MODIFIER);
  }

  gdk_event_free(event);
}

void PanelBrowserTitlebarGtk::ShowFaviconMenu(GdkEventButton* event) {
  // Favicon menu is not supported in panels.
}

void PanelBrowserTitlebarGtk::UpdateTextColor() {
  DCHECK(app_mode_title());

  GdkColor text_color = gfx::SkColorToGdkColor(GetTextColor());
  gtk_util::SetLabelColor(app_mode_title(), &text_color);
}

void PanelBrowserTitlebarGtk::SendEnterNotifyToCloseButtonIfUnderMouse() {
  if (!close_button())
    return;

  gint x;
  gint y;
  GtkAllocation widget_allocation = close_button()->WidgetAllocation();
  gtk_widget_get_pointer(GTK_WIDGET(close_button()->widget()), &x, &y);

  gfx::Rect button_rect(0, 0, widget_allocation.width,
                        widget_allocation.height);
  if (!button_rect.Contains(x, y)) {
    // Mouse is not over the close button.
    return;
  }

  // Create and emit an enter-notify-event on close button.
  GValue return_value;
  return_value.g_type = G_TYPE_BOOLEAN;
  g_value_set_boolean(&return_value, false);

  GdkEvent* event = gdk_event_new(GDK_ENTER_NOTIFY);
  event->crossing.window =
      gtk_button_get_event_window(GTK_BUTTON(close_button()->widget()));
  event->crossing.send_event = FALSE;
  event->crossing.subwindow = gtk_widget_get_window(close_button()->widget());
  event->crossing.time = gtk_util::XTimeNow();
  event->crossing.x = x;
  event->crossing.y = y;
  event->crossing.x_root = widget_allocation.x;
  event->crossing.y_root = widget_allocation.y;
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
  event->crossing.focus = true;
  event->crossing.state = 0;

  g_signal_emit_by_name(GTK_OBJECT(close_button()->widget()),
                        "enter-notify-event", event,
                        &return_value);
}
