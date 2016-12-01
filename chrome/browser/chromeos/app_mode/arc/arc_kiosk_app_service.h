// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_ARC_ARC_KIOSK_APP_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_ARC_ARC_KIOSK_APP_SERVICE_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_launcher.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

// Keeps track of ARC session state and auto-launches kiosk app when it's ready.
// App is started when the following conditions are satisfied:
// 1. App id is registered in ArcAppListPrefs and set to "ready" state.
// 2. Got empty policy compliance report from Android
// 3. App is not yet started
// Also, the app is stopped when one of above conditions changes.
class ArcKioskAppService
    : public KeyedService,
      public ArcAppListPrefs::Observer,
      public ArcKioskAppManager::ArcKioskAppManagerObserver {
 public:
  static ArcKioskAppService* Create(Profile* profile, ArcAppListPrefs* prefs);
  static ArcKioskAppService* Get(content::BrowserContext* context);

  // ArcAppListPrefs::Observer overrides
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppReadyChanged(const std::string& id, bool ready) override;
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent) override;
  void OnTaskDestroyed(int32_t task_id) override;
  void OnPackageListInitialRefreshed() override;

  // ArcKioskAppManager::Observer overrides
  void OnArcKioskAppsChanged() override;

 private:
  ArcKioskAppService(Profile* profile, ArcAppListPrefs* prefs);
  ~ArcKioskAppService() override;

  std::string GetAppId();
  // Called when app should be started or stopped.
  void PreconditionsChanged();

  Profile* const profile_;
  ArcAppListPrefs* const prefs_;
  ArcKioskAppManager* app_manager_;
  std::string app_id_;
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info_;
  int32_t task_id_ = -1;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  // Keeps track whether the app is already launched
  std::unique_ptr<ArcKioskAppLauncher> app_launcher_;

  DISALLOW_COPY_AND_ASSIGN(ArcKioskAppService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_ARC_ARC_KIOSK_APP_SERVICE_H_
