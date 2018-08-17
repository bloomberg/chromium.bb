// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_ANDROID_SMS_APP_INSTALL_DELEGATE_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_ANDROID_SMS_APP_INSTALL_DELEGATE_H_

#include "base/macros.h"

namespace chromeos {
namespace multidevice_setup {

// A delegate class used to install the Messages for Web PWA.
class AndroidSmsAppInstallDelegate {
 public:
  virtual ~AndroidSmsAppInstallDelegate() = default;

  // Installs the Messages for Web PWA. Handles retries and errors internally.
  virtual void InstallAndroidSmsApp() = 0;

 protected:
  AndroidSmsAppInstallDelegate() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppInstallDelegate);
};

}  // namespace multidevice_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_ANDROID_SMS_APP_INSTALL_DELEGATE_H_
