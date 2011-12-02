// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/signed_settings_migration_helper.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

SignedSettingsMigrationHelper::SignedSettingsMigrationHelper() {
  registrar_.Add(this, chrome::NOTIFICATION_OWNERSHIP_CHECKED,
                 content::NotificationService::AllSources());
}

SignedSettingsMigrationHelper::~SignedSettingsMigrationHelper() {
  registrar_.RemoveAll();
  migration_values_.Clear();
}

void SignedSettingsMigrationHelper::AddMigrationValue(const std::string& path,
                                                      base::Value* value) {
  migration_values_.SetValue(path, value);
}

void SignedSettingsMigrationHelper::MigrateValues(void) {
  ownership_checker_.reset(new OwnershipStatusChecker(
      base::Bind(&SignedSettingsMigrationHelper::DoMigrateValues,
                 base::Unretained(this))));
}

// NotificationObserver overrides:
void SignedSettingsMigrationHelper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_OWNERSHIP_CHECKED)
    MigrateValues();
}

void SignedSettingsMigrationHelper::DoMigrateValues(
    OwnershipService::Status status,
    bool current_user_is_owner) {
  ownership_checker_.reset(NULL);

  // We can call StartStorePropertyOp in two cases - either if the owner is
  // currently logged in and the policy can be updated immediately or if there
  // is no owner yet in which case the value will be temporarily stored in the
  // SignedSettingsCache until the device is owned. If none of these
  // cases is met then we will wait for user change notification and retry.
  if (current_user_is_owner || status != OwnershipService::OWNERSHIP_TAKEN) {
    std::map<std::string, base::Value*>::const_iterator i;
    for (i = migration_values_.begin(); i != migration_values_.end(); ++i) {
      // Queue all values for storing.
      CrosSettings::Get()->Set(i->first, *i->second);
    }
    migration_values_.Clear();
  }
}

}  // namespace chromeos

