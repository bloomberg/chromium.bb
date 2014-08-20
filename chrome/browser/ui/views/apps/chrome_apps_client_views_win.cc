// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_apps_client.h"

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_win.h"

// static
extensions::NativeAppWindow* ChromeAppsClient::CreateNativeAppWindowImpl(
    apps::AppWindow* app_window,
    const apps::AppWindow::CreateParams& params) {
  ChromeNativeAppWindowViewsWin* window = new ChromeNativeAppWindowViewsWin;
  window->Init(app_window, params);
  return window;
}
