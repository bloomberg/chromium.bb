// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_VIEWS_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_VIEWS_WIN_H_

#include "chrome/browser/ui/views/apps/native_app_window_views.h"

// Windows-specific parts of the views-backed native shell window implementation
// for packaged apps.
class NativeAppWindowViewsWin : public NativeAppWindowViews {
 public:
  NativeAppWindowViewsWin(apps::ShellWindow* shell_window,
                          const apps::ShellWindow::CreateParams& params);

 private:
  void ActivateParentDesktopIfNecessary();

  // Overridden from NativeAppWindowViews:
  virtual void OnBeforeWidgetInit(views::Widget::InitParams* init_params,
                                  views::Widget* widget) OVERRIDE;

  // Overridden from ui::BaseWindow:
  virtual void Show() OVERRIDE;
  virtual void Activate() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NativeAppWindowViewsWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_VIEWS_WIN_H_
