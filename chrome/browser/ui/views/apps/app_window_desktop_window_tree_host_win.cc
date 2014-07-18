// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_window_desktop_window_tree_host_win.h"

#include <dwmapi.h>

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_win.h"
#include "chrome/browser/ui/views/apps/glass_app_window_frame_view_win.h"
#include "grit/theme_resources.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/win/dpi.h"
#include "ui/views/controls/menu/native_menu_win.h"

#pragma comment(lib, "dwmapi.lib")

AppWindowDesktopWindowTreeHostWin::AppWindowDesktopWindowTreeHostWin(
    ChromeNativeAppWindowViewsWin* app_window,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura)
    : DesktopWindowTreeHostWin(app_window->widget(),
                               desktop_native_widget_aura),
      app_window_(app_window) {
}

AppWindowDesktopWindowTreeHostWin::~AppWindowDesktopWindowTreeHostWin() {
}

bool AppWindowDesktopWindowTreeHostWin::GetClientAreaInsets(
    gfx::Insets* insets) const {
  // Use the default client insets for an opaque frame or a glass popup/app
  // frame.
  if (!app_window_->glass_frame_view())
    return false;

  // This tells Windows that most of the window is a client area, meaning Chrome
  // will draw it. Windows still fills in the glass bits because of the
  // DwmExtendFrameIntoClientArea call in |UpdateDWMFrame|.
  // The 1 pixel edge is left on the sides and bottom as without this
  //   * windows paint in a more standard way, and
  //   * get weird black bars at the top when maximized in multiple monitor
  //     configurations.
  int border_thickness = 1;
  insets->Set(0, border_thickness, border_thickness, border_thickness);
  return true;
}

void AppWindowDesktopWindowTreeHostWin::HandleFrameChanged() {
  // We need to update the glass region on or off before the base class adjusts
  // the window region.
  app_window_->OnCanHaveAlphaEnabledChanged();
  UpdateDWMFrame();
  DesktopWindowTreeHostWin::HandleFrameChanged();
}

void AppWindowDesktopWindowTreeHostWin::PostHandleMSG(UINT message,
                                                      WPARAM w_param,
                                                      LPARAM l_param) {
  switch (message) {
    case WM_WINDOWPOSCHANGED: {
      UpdateDWMFrame();
      break;
    }
  }
}

void AppWindowDesktopWindowTreeHostWin::UpdateDWMFrame() {
  if (!GetWidget()->client_view() || !app_window_->glass_frame_view())
    return;

  MARGINS margins = {0};

  // If the opaque frame is visible, we use the default (zero) margins.
  // Otherwise, we need to figure out how to extend the glass in.
  if (app_window_->glass_frame_view()) {
    gfx::Insets insets = app_window_->glass_frame_view()->GetGlassInsets();
    // The DWM API's expect values in pixels. We need to convert from DIP to
    // pixels here.
    insets = insets.Scale(gfx::win::GetDeviceScaleFactor());
    margins.cxLeftWidth = insets.left();
    margins.cxRightWidth = insets.right();
    margins.cyBottomHeight = insets.bottom();
    margins.cyTopHeight = insets.top();
  }

  DwmExtendFrameIntoClientArea(GetHWND(), &margins);
}
