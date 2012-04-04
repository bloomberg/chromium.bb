// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_PROXY_SETTINGS_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_PROXY_SETTINGS_DIALOG_H_
#pragma once

#include "chrome/browser/chromeos/login/login_html_dialog.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

// Should be used on the UI thread only, because of static |instance_count_|.
class ProxySettingsDialog : public LoginHtmlDialog {
 public:
  // Returns whether the dialog is being shown.
  static bool IsShown();

  ProxySettingsDialog(LoginHtmlDialog::Delegate* delegate,
                      gfx::NativeWindow window);
  virtual ~ProxySettingsDialog();

 protected:
  // HtmlDialogUIDelegate implementation.
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;

 private:
  // TODO(altimofeev): consider avoidance static variable by extending current
  // WebUI/login interfaces.
  static int instance_count_;

  DISALLOW_COPY_AND_ASSIGN(ProxySettingsDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_PROXY_SETTINGS_DIALOG_H_
