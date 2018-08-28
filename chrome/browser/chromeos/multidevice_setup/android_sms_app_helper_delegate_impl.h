// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_ANDROID_SMS_APP_HELPER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_ANDROID_SMS_APP_HELPER_DELEGATE_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"
#include "url/gurl.h"

class Profile;

namespace web_app {
class PendingAppManager;
}  // namespace web_app

namespace chromeos {
namespace multidevice_setup {

class AndroidSmsAppHelperDelegateImpl : public AndroidSmsAppHelperDelegate {
 public:
  explicit AndroidSmsAppHelperDelegateImpl(Profile* profile);
  ~AndroidSmsAppHelperDelegateImpl() override;

 private:
  friend class AndroidSmsAppHelperDelegateImplTest;

  explicit AndroidSmsAppHelperDelegateImpl(
      web_app::PendingAppManager* pending_app_manager);
  void OnAppInstalled(const GURL& app_url,
                      const base::Optional<std::string>& app_id);

  // AndroidSmsAppHelperDelegate:
  void InstallAndroidSmsApp() override;

  static const char kMessagesWebAppUrl[];
  web_app::PendingAppManager* pending_app_manager_;
  base::WeakPtrFactory<AndroidSmsAppHelperDelegateImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppHelperDelegateImpl);
};

}  // namespace multidevice_setup
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_ANDROID_SMS_APP_HELPER_DELEGATE_IMPL_H_
