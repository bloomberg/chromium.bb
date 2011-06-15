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

PanelBrowserWindowGtk::~PanelBrowserWindowGtk() {
}

void PanelBrowserWindowGtk::Init() {
  BrowserWindowGtk::Init();

  // Keep the window docked to the bottom of the screen on resizes.
  gtk_window_set_gravity(window(), GDK_GRAVITY_SOUTH_EAST);

  // Keep the window always on top.
  gtk_window_set_keep_above(window(), TRUE);

  // Show the window on all the virtual desktops.
  gtk_window_stick(window());

  // Do not show an icon in the task bar.  Window operations such as close,
  // minimize etc. can only be done from the panel UI.
  gtk_window_set_skip_taskbar_hint(window(), TRUE);
}

bool PanelBrowserWindowGtk::GetWindowEdge(int x, int y, GdkWindowEdge* edge) {
  // Since panels are not resizable or movable by the user, we should not
  // detect the window edge for behavioral purposes.  The edge, if any,
  // is present only for visual aspects.
  return FALSE;
}

bool PanelBrowserWindowGtk::HandleTitleBarLeftMousePress(
    GdkEventButton* event,
    guint32 last_click_time,
    gfx::Point last_click_position) {
  // TODO(prasadt): Minimize the panel.
  return TRUE;
}

void PanelBrowserWindowGtk::SaveWindowPosition() {
  // We don't save window position for panels as it's controlled by
  // PanelManager.
  return;
}

void PanelBrowserWindowGtk::SetGeometryHints() {
  SetBoundsImpl();
}

void PanelBrowserWindowGtk::SetBounds(const gfx::Rect& bounds) {
  SetBoundsImpl();
}

bool PanelBrowserWindowGtk::UseCustomFrame() {
  // We always use custom frame for panels.
  return TRUE;
}

void PanelBrowserWindowGtk::ShowPanel() {
  Show();
}

void PanelBrowserWindowGtk::SetPanelBounds(const gfx::Rect& bounds) {
  SetBounds(bounds);
}

void PanelBrowserWindowGtk::MinimizePanel() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowGtk::RestorePanel() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowGtk::ClosePanel() {
  Close();
}

void PanelBrowserWindowGtk::ActivatePanel() {
  Activate();
}

void PanelBrowserWindowGtk::DeactivatePanel() {
  Deactivate();
}

bool PanelBrowserWindowGtk::IsPanelActive() const {
  return IsActive();
}

gfx::NativeWindow PanelBrowserWindowGtk::GetNativePanelHandle() {
  return GetNativeHandle();
}

void PanelBrowserWindowGtk::UpdatePanelTitleBar() {
  UpdateTitleBar();
}

void PanelBrowserWindowGtk::ShowTaskManagerForPanel() {
  ShowTaskManager();
}

void PanelBrowserWindowGtk::NotifyPanelOnUserChangedTheme() {
  UserChangedTheme();
}

void PanelBrowserWindowGtk::FlashPanelFrame() {
  FlashFrame();
}

void PanelBrowserWindowGtk::DestroyPanelBrowser() {
  DestroyBrowser();
}

void PanelBrowserWindowGtk::SetBoundsImpl() {
  const gfx::Rect& bounds = panel_->GetBounds();
  gtk_window_move(window_, bounds.x(), bounds.y());
  gtk_window_resize(window(), bounds.width(), bounds.height());
}
