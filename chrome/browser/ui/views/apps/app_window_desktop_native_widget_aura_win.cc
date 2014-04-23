// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_window_desktop_native_widget_aura_win.h"

#include "chrome/browser/ui/views/apps/app_window_desktop_window_tree_host_win.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_win.h"
#include "ui/aura/window.h"

AppWindowDesktopNativeWidgetAuraWin::AppWindowDesktopNativeWidgetAuraWin(
    ChromeNativeAppWindowViewsWin* app_window)
    : views::DesktopNativeWidgetAura(app_window->widget()),
      app_window_(app_window) {
  GetNativeWindow()->SetName("AppWindowAura");
}

AppWindowDesktopNativeWidgetAuraWin::~AppWindowDesktopNativeWidgetAuraWin() {
}

void AppWindowDesktopNativeWidgetAuraWin::InitNativeWidget(
    const views::Widget::InitParams& params) {
  views::Widget::InitParams modified_params = params;
  modified_params.desktop_window_tree_host =
      new AppWindowDesktopWindowTreeHostWin(app_window_, this);
  DesktopNativeWidgetAura::InitNativeWidget(modified_params);
}
