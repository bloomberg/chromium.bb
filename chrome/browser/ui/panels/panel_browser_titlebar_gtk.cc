// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_titlebar_gtk.h"

PanelBrowserTitlebarGtk::PanelBrowserTitlebarGtk(
    BrowserWindowGtk* browser_window, GtkWindow* window)
    : BrowserTitlebar(browser_window, window) {

}

PanelBrowserTitlebarGtk::~PanelBrowserTitlebarGtk() {
}

bool PanelBrowserTitlebarGtk::BuildButton(const std::string& button_token,
                                          bool left_side) {
  // Panel only shows close button.
  if (button_token != "close")
    return false;

  return BrowserTitlebar::BuildButton(button_token, left_side);
}

void PanelBrowserTitlebarGtk::ShowFaviconMenu(GdkEventButton* event) {
  // Favicon menu is not supported in panels.
}
