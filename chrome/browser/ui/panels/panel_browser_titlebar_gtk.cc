// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_titlebar_gtk.h"

#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_window_gtk.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// Spacing between buttons of panel's titlebar.
const int kPanelButtonSpacing = 7;

// Spacing around outside of panel's titlebar buttons.
const int kPanelButtonOuterPadding = 7;

}  // namespace

PanelBrowserTitlebarGtk::PanelBrowserTitlebarGtk(
    PanelBrowserWindowGtk* browser_window, GtkWindow* window)
    : BrowserTitlebar(browser_window, window),
      browser_window_(browser_window) {
}

PanelBrowserTitlebarGtk::~PanelBrowserTitlebarGtk() {
}

bool PanelBrowserTitlebarGtk::BuildButton(const std::string& button_token,
                                          bool left_side) {
  // Panel only shows close and minimize/restore buttons.
  if (button_token != "close" && button_token != "minimize")
    return false;

  // Create unminimze button in order to show it to expand the minimized panel.
  if (button_token == "minimize")
    unminimize_button_.reset(CreateTitlebarButton("unminimize", left_side));

  return BrowserTitlebar::BuildButton(button_token, left_side);
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
  if (close_button() && close_button()->widget() == button)
    browser_window_->panel()->Close();
  else if (minimize_button() && minimize_button()->widget() == button)
    browser_window_->panel()->Minimize();
  else if (unminimize_button_.get() && unminimize_button_->widget() == button)
    browser_window_->panel()->Restore();
}

void PanelBrowserTitlebarGtk::ShowFaviconMenu(GdkEventButton* event) {
  // Favicon menu is not supported in panels.
}
