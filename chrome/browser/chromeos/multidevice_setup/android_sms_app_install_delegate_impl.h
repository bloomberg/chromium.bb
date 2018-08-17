// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_ANDROID_SMS_APP_INSTALL_DELEGATE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_ANDROID_SMS_APP_INSTALL_DELEGATE_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_install_delegate.h"
#include "url/gurl.h"

class Profile;

namespace web_app {
class PendingAppManager;
}  // namespace web_app

namespace chromeos {
namespace multidevice_setup {

class AndroidSmsAppInstallDelegateImpl : public AndroidSmsAppInstallDelegate {
 public:
  explicit AndroidSmsAppInstallDelegateImpl(Profile* profile);
  ~AndroidSmsAppInstallDelegateImpl() override;

 private:
  friend class AndroidSmsAppInstallDelegateImplTest;

  explicit AndroidSmsAppInstallDelegateImpl(
      web_app::PendingAppManager* pending_app_manager);
  void OnAppInstalled(const GURL& app_url, const std::string& app_id);

  // AndroidSmsAppInstallDelegate:
  void InstallAndroidSmsApp() override;

  static const char kMessagesWebAppUrl[];
  web_app::PendingAppManager* pending_app_manager_;
  base::WeakPtrFactory<AndroidSmsAppInstallDelegateImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppInstallDelegateImpl);
};

}  // namespace multidevice_setup
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_ANDROID_SMS_APP_INSTALL_DELEGATE_IMPL_H_
