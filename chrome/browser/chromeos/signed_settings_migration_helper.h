// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SIGNED_SETTINGS_MIGRATION_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_SIGNED_SETTINGS_MIGRATION_HELPER_H_
#pragma once

#include "chrome/browser/chromeos/login/ownership_status_checker.h"
#include "chrome/browser/chromeos/login/signed_settings_helper.h"
#include "chrome/browser/prefs/pref_value_map.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace base {
class Value;
}

namespace chromeos {

// This class provides the means to migrate settings to the signed settings
// store. It does one of three things - store the settings in the policy blob
// immediately if the current user is the owner. Uses the
// SignedSettingsCache if there is no owner yet, or waits for an
// OWNERSHIP_CHECKED notification to delay the storing until the owner has
// logged in.
class SignedSettingsMigrationHelper : public content::NotificationObserver {
 public:
  SignedSettingsMigrationHelper();
  virtual ~SignedSettingsMigrationHelper();

  // Adds a value to be migrated. The class takes ownership of the |value|.
  void AddMigrationValue(const std::string& path, base::Value* value);

  // Initiates values migration. If the device is already owned this will
  // happen immediately if not it will wait for ownership login and finish the
  // migration then.
  void MigrateValues(void);

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Does the actual migration when ownership has been confirmed.
  void DoMigrateValues(OwnershipService::Status status,
                       bool current_user_is_owner);

  content::NotificationRegistrar registrar_;
  scoped_ptr<OwnershipStatusChecker> ownership_checker_;
  PrefValueMap migration_values_;

  DISALLOW_COPY_AND_ASSIGN(SignedSettingsMigrationHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SIGNED_SETTINGS_MIGRATION_HELPER_H_
