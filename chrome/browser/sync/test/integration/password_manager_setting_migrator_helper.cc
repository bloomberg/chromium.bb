// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/password_manager_setting_migrator_helper.h"

#include "base/metrics/field_trial.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/password_manager/password_manager_setting_migrator_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/password_manager/core/browser/password_manager_settings_migration_experiment.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/sync/browser/password_manager_setting_migrator_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "testing/gtest/include/gtest/gtest.h"

using preferences_helper::GetPrefs;

namespace {

const char kFieldTrialName[] = "PasswordManagerSettingsMigration";
const char kEnabledGroupName[] = "Enable";

}  // namespace

namespace password_manager_setting_migrater_helper {

bool EnsureFieldTrialSetup() {
  if (base::FieldTrialList::TrialExists(kFieldTrialName)) {
    DCHECK(password_manager::IsSettingsMigrationActive());
    return false;
  }
  base::FieldTrialList::CreateFieldTrial(kFieldTrialName, kEnabledGroupName);
  return true;
}

void InitializePreferencesMigration(Profile* profile) {
  PasswordManagerSettingMigratorServiceFactory::GetForProfile(profile)
      ->InitializeMigration(ProfileSyncServiceFactory::GetForProfile(profile));
}

void ExpectPrefValuesOnClient(int index,
                              bool new_pref_value,
                              bool old_pref_value) {
  using password_manager::prefs::kCredentialsEnableService;
  using password_manager::prefs::kPasswordManagerSavingEnabled;
  EXPECT_EQ(new_pref_value,
            GetPrefs(index)->GetBoolean(kCredentialsEnableService));
  EXPECT_EQ(old_pref_value,
            GetPrefs(index)->GetBoolean(kPasswordManagerSavingEnabled));
}

}  // namespace password_manager_setting_migrater_helper
