// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_APP_HELPER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_APP_HELPER_DELEGATE_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "extensions/common/extension.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {
enum class InstallResultCode;
class PendingAppManager;
}  // namespace web_app

namespace chromeos {

namespace android_sms {

class AndroidSmsAppHelperDelegateImpl
    : public multidevice_setup::AndroidSmsAppHelperDelegate {
 public:
  AndroidSmsAppHelperDelegateImpl(
      Profile* profile,
      web_app::PendingAppManager* pending_app_manager,
      HostContentSettingsMap* host_content_settings_map);
  ~AndroidSmsAppHelperDelegateImpl() override;

 private:
  friend class AndroidSmsAppHelperDelegateImplTest;

  // Thin wrapper around static PWA functions which is stubbed out for tests.
  // Specifically, this class wraps extensions::util::GetInstalledPwaForUrl()
  // and OpenApplication().
  class PwaDelegate {
   public:
    PwaDelegate();
    virtual ~PwaDelegate();

    virtual const extensions::Extension* GetPwaForUrl(Profile* profile,
                                                      GURL gurl);
    virtual content::WebContents* OpenApp(const AppLaunchParams& params);
    virtual network::mojom::CookieManager* GetCookieManager(Profile* profile);
  };

  // AndroidSmsAppHelperDelegate:
  void SetUpAndroidSmsApp() override;
  void SetUpAndLaunchAndroidSmsApp() override;
  void TearDownAndroidSmsApp() override;

  void OnAppInstalled(bool launch_on_install,
                      const GURL& app_url,
                      web_app::InstallResultCode code);
  void SetUpAndroidSmsApp(bool launch_on_install);
  void SetUpAndroidSmsAppWithNoOldApp(bool launch_on_install);
  void LaunchAndroidSmsApp();
  void OnSetDefaultToPersistCookieForInstall(bool launch_on_install,
                                             bool set_cookie_success);
  void TearDownAndroidSmsAppAtUrl(GURL pwa_url);

  void SetPwaDelegateForTesting(std::unique_ptr<PwaDelegate> test_pwa_delegate);

  static const char kMessagesWebAppUrl[];

  Profile* profile_;
  web_app::PendingAppManager* pending_app_manager_;
  HostContentSettingsMap* host_content_settings_map_;

  std::unique_ptr<PwaDelegate> pwa_delegate_;
  base::WeakPtrFactory<AndroidSmsAppHelperDelegateImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppHelperDelegateImpl);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_APP_HELPER_DELEGATE_IMPL_H_
