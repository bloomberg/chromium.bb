// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_APP_INSTALL_DELEGATE_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_APP_INSTALL_DELEGATE_H_

#include "base/macros.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_install_delegate.h"

namespace chromeos {
namespace multidevice_setup {

class FakeAndroidSmsAppInstallDelegate : public AndroidSmsAppInstallDelegate {
 public:
  FakeAndroidSmsAppInstallDelegate();
  ~FakeAndroidSmsAppInstallDelegate() override;
  bool HasInstalledApp();

  // AndroidSmsAppInstallDelegate:
  void InstallAndroidSmsApp() override;

 private:
  bool has_installed_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeAndroidSmsAppInstallDelegate);
};

}  // namespace multidevice_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_ANDROID_SMS_APP_INSTALL_DELEGATE_H_
