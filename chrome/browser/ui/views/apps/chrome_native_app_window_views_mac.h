// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_MAC_H_

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views.h"

// Mac-specific parts of ChromeNativeAppWindowViews.
class ChromeNativeAppWindowViewsMac : public ChromeNativeAppWindowViews {
 public:
  ChromeNativeAppWindowViewsMac();
  ~ChromeNativeAppWindowViewsMac() override;

 protected:
  // ChromeNativeAppWindowViews implementation.
  void OnBeforeWidgetInit(
      const extensions::AppWindow::CreateParams& create_params,
      views::Widget::InitParams* init_params,
      views::Widget* widget) override;

  // ui::BaseWindow implementation.
  void Show() override;
  void ShowInactive() override;

  // NativeAppWindow implementation.
  // These are used to simulate Mac-style hide/show. Since windows can be hidden
  // and shown using the app.window API, this sets is_hidden_with_app_ to
  // differentiate the reason a window was hidden.
  void ShowWithApp() override;
  void HideWithApp() override;

 private:
  // Whether this window last became hidden due to a request to hide the entire
  // app, e.g. via the dock menu or Cmd+H. This is set by Hide/ShowWithApp.
  bool is_hidden_with_app_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNativeAppWindowViewsMac);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_MAC_H_
