// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_ARC_ARC_KIOSK_APP_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_ARC_ARC_KIOSK_APP_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/signin/core/account_id/account_id.h"

class PrefRegistrySimple;

namespace chromeos {

// Keeps track of Android apps that are to be launched in kiosk mode.
// For removed apps deletes appropriate cryptohome. The information about
// kiosk apps are received from CrosSettings. For each app, the system
// creates a user in whose context the app then runs.
class ArcKioskAppManager {
 public:
  // Struct to hold full info about ARC Kiosk app. In future
  // this structure may contain many extra fields, e.g. icon.
  class ArcKioskApp {
   public:
    ArcKioskApp(const policy::ArcKioskAppBasicInfo& app_info,
                const AccountId& account_id,
                const std::string& name);
    ArcKioskApp(const ArcKioskApp& other);
    ~ArcKioskApp();

    bool operator==(const std::string& app_id) const;
    bool operator==(const policy::ArcKioskAppBasicInfo& app_info) const;

    const policy::ArcKioskAppBasicInfo& app_info() const { return app_info_; }
    const AccountId& account_id() const { return account_id_; }
    const std::string& name() const { return name_; }

   private:
    policy::ArcKioskAppBasicInfo app_info_;
    AccountId account_id_;
    std::string name_;
  };

  using ArcKioskApps = std::vector<ArcKioskApp>;

  class ArcKioskAppManagerObserver {
   public:
    virtual void OnArcKioskAppsChanged() {}

   protected:
    virtual ~ArcKioskAppManagerObserver() = default;
  };

  static ArcKioskAppManager* Get();

  ArcKioskAppManager();
  ~ArcKioskAppManager();

  // Registers kiosk app entries in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Removes cryptohomes which could not be removed during the previous session.
  static void RemoveObsoleteCryptohomes();

  // Returns auto launched account id. If there is none, account is invalid,
  // thus is_valid() returns empty AccountId.
  const AccountId& GetAutoLaunchAccountId() const;

  // Returns app that should be started for given account id.
  const ArcKioskApp* GetAppByAccountId(const AccountId& account_id);

  const ArcKioskApps& GetAllApps() const { return apps_; }

  void AddObserver(ArcKioskAppManagerObserver* observer);
  void RemoveObserver(ArcKioskAppManagerObserver* observer);

  bool current_app_was_auto_launched_with_zero_delay() const {
    return auto_launched_with_zero_delay_;
  }

 private:
  // Updates apps_ based on CrosSettings.
  void UpdateApps();

  // Removes cryptohomes of the removed apps. Terminates the session if
  // a removed app is running.
  void ClearRemovedApps(const ArcKioskApps& old_apps);

  ArcKioskApps apps_;
  AccountId auto_launch_account_id_;
  bool auto_launched_with_zero_delay_ = false;
  base::ObserverList<ArcKioskAppManagerObserver, true> observers_;

  std::unique_ptr<CrosSettings::ObserverSubscription>
      local_accounts_subscription_;
  std::unique_ptr<CrosSettings::ObserverSubscription>
      local_account_auto_login_id_subscription_;

  DISALLOW_COPY_AND_ASSIGN(ArcKioskAppManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_ARC_ARC_KIOSK_APP_MANAGER_H_
