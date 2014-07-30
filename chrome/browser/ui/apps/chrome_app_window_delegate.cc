// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_app_window_delegate.h"

ChromeAppWindowDelegate::ChromeAppWindowDelegate() {
}

ChromeAppWindowDelegate::~ChromeAppWindowDelegate() {
}

apps::NativeAppWindow* ChromeAppWindowDelegate::CreateNativeAppWindow(
    apps::AppWindow* window,
    const apps::AppWindow::CreateParams& params) {
  return CreateNativeAppWindowImpl(window, params);
}
