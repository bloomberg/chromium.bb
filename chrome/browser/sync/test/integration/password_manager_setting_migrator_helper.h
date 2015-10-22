// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORD_MANAGER_SETTING_MIGRATOR_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORD_MANAGER_SETTING_MIGRATOR_HELPER_H_

namespace base {
class FieldTrial;
}

class Profile;

namespace password_manager_setting_migrater_helper {

// Enables the password manager setting migration field trial.
void SetupFieldTrial();

// Triggers Initalization of the PasswordManagerSettingMigrator service.
// The service registers observes which are required in order to perform
// migration.
void InitializePreferencesMigration(Profile* profile);

// Checks that on the client number |index| the value for
// kCredentialsEnableService is equal to |new_pref_value| and the value for
// kPasswordManagerSavingEnabled is equal to |old_pref_value|.
void ExpectPrefValuesOnClient(int index,
                              bool new_pref_value,
                              bool old_pref_value);

}  // namespace password_manager_setting_migrater_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PASSWORD_MANAGER_SETTING_MIGRATOR_HELPER_H_
