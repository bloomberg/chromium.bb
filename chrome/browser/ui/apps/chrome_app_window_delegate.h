// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_CHROME_APP_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_UI_APPS_CHROME_APP_WINDOW_DELEGATE_H_

#include "apps/app_window.h"

class ChromeAppWindowDelegate : public apps::AppWindow::Delegate {
 public:
  ChromeAppWindowDelegate();
  virtual ~ChromeAppWindowDelegate();

 private:
  // apps::AppWindow::Delegate:
  virtual apps::NativeAppWindow* CreateNativeAppWindow(
      apps::AppWindow* window,
      const apps::AppWindow::CreateParams& params) OVERRIDE;

  // Implemented in platform specific code.
  static apps::NativeAppWindow* CreateNativeAppWindowImpl(
      apps::AppWindow* window,
      const apps::AppWindow::CreateParams& params);

  DISALLOW_COPY_AND_ASSIGN(ChromeAppWindowDelegate);
};

#endif  // CHROME_BROWSER_UI_APPS_CHROME_APP_WINDOW_DELEGATE_H_
