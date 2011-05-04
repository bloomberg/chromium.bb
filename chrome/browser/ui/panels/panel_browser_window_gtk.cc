// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_window_gtk.h"

#include "chrome/browser/ui/panels/panel.h"

BrowserWindow* Panel::CreateNativePanel(Browser* browser, Panel* panel) {
  PanelBrowserWindowGtk* panel_browser_window_gtk =
      new PanelBrowserWindowGtk(browser, panel);
  panel_browser_window_gtk->Init();
  return panel_browser_window_gtk;
}

PanelBrowserWindowGtk::PanelBrowserWindowGtk(Browser* browser, Panel* panel)
    : BrowserWindowGtk(browser), panel_(panel) {
}

void PanelBrowserWindowGtk::Init() {
  BrowserWindowGtk::Init();

  // Keep the window docked to the bottom of the screen on resizes.
  gtk_window_set_gravity(window(), GDK_GRAVITY_SOUTH_EAST);

  // Keep the window always on top.
  gtk_window_set_keep_above(window(), true);

  // Show the window on all the virtual desktops.
  gtk_window_stick(window());

  // Do not show an icon in the task bar.  Window operations such as close,
  // minimize etc. can only be done from the panel UI.
  gtk_window_set_skip_taskbar_hint(window(), true);
}

bool PanelBrowserWindowGtk::HandleTitleBarLeftMousePress(
    GdkEventButton* event,
    guint32 last_click_time,
    gfx::Point last_click_position) {
  // TODO(prasadt): Minimize the panel.
  return TRUE;
}

void PanelBrowserWindowGtk::SetGeometryHints() {
  SetBoundsImpl();
}

void PanelBrowserWindowGtk::SetBounds(const gfx::Rect& bounds) {
  SetBoundsImpl();
}

void PanelBrowserWindowGtk::SetBoundsImpl() {
  const gfx::Rect& bounds = panel_->GetBounds();
  gtk_window_move(window_, bounds.x(), bounds.y());
  gtk_window_resize(window(), bounds.width(), bounds.height());
}
