// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_app_window_delegate.h"

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views.h"

// static
apps::NativeAppWindow* ChromeAppWindowDelegate::CreateNativeAppWindowImpl(
    apps::AppWindow* app_window,
    const apps::AppWindow::CreateParams& params) {
  ChromeNativeAppWindowViews* window = new ChromeNativeAppWindowViews;
  window->Init(app_window, params);
  return window;
}
