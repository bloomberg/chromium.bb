// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_APP_SETUP_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_APP_SETUP_CONTROLLER_IMPL_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/android_sms/android_sms_app_setup_controller.h"
#include "url/gurl.h"

class HostContentSettingsMap;
class Profile;

namespace network {
namespace mojom {
class CookieManager;
}  // namespace mojom
}  // namespace network

namespace web_app {
enum class InstallResultCode;
class PendingAppManager;
}  // namespace web_app

namespace chromeos {

namespace android_sms {

// Concrete AndroidSmsAppSetupController implementation.
class AndroidSmsAppSetupControllerImpl : public AndroidSmsAppSetupController {
 public:
  AndroidSmsAppSetupControllerImpl(
      Profile* profile,
      web_app::PendingAppManager* pending_app_manager,
      HostContentSettingsMap* host_content_settings_map);
  ~AndroidSmsAppSetupControllerImpl() override;

 private:
  friend class AndroidSmsAppSetupControllerImplTest;

  // Thin wrapper around static PWA functions which is stubbed out for tests.
  class PwaDelegate {
   public:
    PwaDelegate();
    virtual ~PwaDelegate();

    virtual const extensions::Extension* GetPwaForUrl(const GURL& install_url,
                                                      Profile* profile);
    virtual network::mojom::CookieManager* GetCookieManager(const GURL& app_url,
                                                            Profile* profile);
  };

  // AndroidSmsAppSetupController:
  void SetUpApp(const GURL& app_url,
                const GURL& install_url,
                SuccessCallback callback) override;
  const extensions::Extension* GetPwa(const GURL& install_url) override;
  void DeleteRememberDeviceByDefaultCookie(const GURL& app_url,
                                           SuccessCallback callback) override;
  void RemoveApp(const GURL& app_url,
                 const GURL& install_url,
                 const GURL& migrated_to_app_url,
                 SuccessCallback callback) override;

  void OnSetRememberDeviceByDefaultCookieResult(const GURL& app_url,
                                                const GURL& install_url,
                                                SuccessCallback callback,
                                                bool succeeded);
  void OnSetMigrationCookieResult(const GURL& app_url,
                                  SuccessCallback callback,
                                  bool succeeded);

  void OnAppInstallResult(SuccessCallback callback,
                          const GURL& app_url,
                          const GURL& install_url,
                          web_app::InstallResultCode code);
  void OnAppUninstallResult(const base::UnguessableToken& id,
                            const GURL& app_url,
                            const GURL& migrated_to_app_url,
                            const GURL& install_url,
                            bool succeeded);
  void OnDeleteRememberDeviceByDefaultCookieResult(const GURL& app_url,
                                                   SuccessCallback callback,
                                                   uint32_t num_deleted);
  void OnDeleteMigrationCookieResult(const GURL& app_url,
                                     const GURL& install_url,
                                     SuccessCallback callback,
                                     uint32_t num_deleted);

  void SetPwaDelegateForTesting(std::unique_ptr<PwaDelegate> test_pwa_delegate);

  Profile* profile_;
  web_app::PendingAppManager* pending_app_manager_;
  HostContentSettingsMap* host_content_settings_map_;

  std::unique_ptr<PwaDelegate> pwa_delegate_;
  base::flat_map<base::UnguessableToken, SuccessCallback>
      uninstall_id_to_callback_map_;
  base::WeakPtrFactory<AndroidSmsAppSetupControllerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppSetupControllerImpl);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_APP_SETUP_CONTROLLER_IMPL_H_
