// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_CERTIFICATE_MANAGER_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_CERTIFICATE_MANAGER_DIALOG_H_

#include "chrome/browser/chromeos/login/ui/login_web_dialog.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace chromeos {

// This dialog is used to manage user certificates from the kiosk launch screen.
class CertificateManagerDialog : public LoginWebDialog {
 public:
  CertificateManagerDialog(Profile* profile,
                           LoginWebDialog::Delegate* delegate,
                           gfx::NativeWindow window);
  virtual ~CertificateManagerDialog();

 private:
  DISALLOW_COPY_AND_ASSIGN(CertificateManagerDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_CERTIFICATE_MANAGER_DIALOG_H_
