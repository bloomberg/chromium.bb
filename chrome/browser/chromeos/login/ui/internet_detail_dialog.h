// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_INTERNET_DETAIL_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_INTERNET_DETAIL_DIALOG_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/ui/login_web_dialog.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

class NetworkState;

// Should be used on the UI thread only, because of static |instance_count_|.
class InternetDetailDialog : public LoginWebDialog {
 public:
  // Returns whether the dialog is being shown.
  static bool IsShown();

  InternetDetailDialog(content::BrowserContext* browser_context,
                       const NetworkState& network,
                       LoginWebDialog::Delegate* delegate,
                       gfx::NativeWindow window);
  ~InternetDetailDialog() override;

  // LoginWebDialog
  base::string16 GetDialogTitle() const override;
  std::string GetDialogArgs() const override;

  static void ShowDialog(content::BrowserContext* browser_context,
                         gfx::NativeWindow window,
                         const std::string& network_id);

 private:
  // TODO(altimofeev): consider avoidance static variable by extending current
  // WebUI/login interfaces.
  static int instance_count_;

  std::string guid_;
  base::string16 name_;

  DISALLOW_COPY_AND_ASSIGN(InternetDetailDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_INTERNET_DETAIL_DIALOG_H_
