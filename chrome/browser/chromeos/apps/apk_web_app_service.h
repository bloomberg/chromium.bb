// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APPS_APK_WEB_APP_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_APPS_APK_WEB_APP_SERVICE_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/apps/apk_web_app_installer.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

class ArcAppListPrefs;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace chromeos {

class ApkWebAppService : public KeyedService,
                         public ApkWebAppInstaller::Owner,
                         public ArcAppListPrefs::Observer,
                         public extensions::ExtensionRegistryObserver {
 public:
  static ApkWebAppService* Get(Profile* profile);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit ApkWebAppService(Profile* profile);
  ~ApkWebAppService() override;

  void SetArcAppListPrefsForTesting(ArcAppListPrefs* prefs);

 private:
  FRIEND_TEST_ALL_PREFIXES(ApkWebAppInstallerDelayedArcStartBrowserTest,
                           DelayedUninstall);

  // Uninstalls a web app with id |web_app_id| iff it was installed via calling
  // ApkWebAppInstaller::Install().
  void UninstallWebApp(const extensions::ExtensionId& web_app_id);

  // KeyedService:
  void Shutdown() override;

  // ArcAppListPrefs::Observer:
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;
  void OnPackageListInitialRefreshed() override;

  // extensions::ExtensionRegistryObserver overrides.
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  void OnDidGetWebAppIcon(const std::string& package_name,
                          arc::mojom::WebAppInfoPtr web_app_info,
                          const std::vector<uint8_t>& icon_png_data);
  void OnDidFinishInstall(const std::string& package_name,
                          const extensions::ExtensionId& web_app_id);

  Profile* profile_;
  ArcAppListPrefs* arc_app_list_prefs_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      observer_;

  // Must go last.
  base::WeakPtrFactory<ApkWebAppService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ApkWebAppService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APPS_APK_WEB_APP_SERVICE_H_
