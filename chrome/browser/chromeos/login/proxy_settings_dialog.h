// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_PROXY_SETTINGS_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_PROXY_SETTINGS_DIALOG_H_
#pragma once

#include "chrome/browser/chromeos/login/login_html_dialog.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

class ProxySettingsDialog : public LoginHtmlDialog {
 public:
  ProxySettingsDialog(LoginHtmlDialog::Delegate* delegate,
                      gfx::NativeWindow window);

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxySettingsDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_PROXY_SETTINGS_DIALOG_H_
