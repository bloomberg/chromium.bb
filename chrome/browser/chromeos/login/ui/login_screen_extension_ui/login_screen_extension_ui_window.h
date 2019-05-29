// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_LOGIN_SCREEN_EXTENSION_UI_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_LOGIN_SCREEN_EXTENSION_UI_WINDOW_H_

#include <string>

#include "base/callback_forward.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace views {
class WebDialogView;
class Widget;
}  // namespace views

namespace chromeos {

class LoginScreenExtensionUiDialogDelegate;

// This class represents the window that can be created by the
// chrome.loginScreenUi API. It manages the window's widget, view and delegate,
// which are all automatically deleted when the widget closes.
class LoginScreenExtensionUiWindow {
 public:
  struct CreateOptions {
    CreateOptions(const std::string& extension_name,
                  const GURL& content_url,
                  bool can_be_closed_by_user,
                  base::OnceClosure close_callback);
    ~CreateOptions();

    const std::string extension_name;
    const GURL content_url;
    bool can_be_closed_by_user;
    base::OnceClosure close_callback;
  };

  explicit LoginScreenExtensionUiWindow(CreateOptions* create_options);
  ~LoginScreenExtensionUiWindow();

 private:
  LoginScreenExtensionUiDialogDelegate* dialog_delegate_ = nullptr;
  views::WebDialogView* dialog_view_ = nullptr;
  views::Widget* dialog_widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LoginScreenExtensionUiWindow);
};

class LoginScreenExtensionUiWindowFactory {
 public:
  LoginScreenExtensionUiWindowFactory();
  virtual ~LoginScreenExtensionUiWindowFactory();

  virtual std::unique_ptr<LoginScreenExtensionUiWindow> Create(
      LoginScreenExtensionUiWindow::CreateOptions* create_options);

  DISALLOW_COPY_AND_ASSIGN(LoginScreenExtensionUiWindowFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_SCREEN_EXTENSION_UI_LOGIN_SCREEN_EXTENSION_UI_WINDOW_H_
