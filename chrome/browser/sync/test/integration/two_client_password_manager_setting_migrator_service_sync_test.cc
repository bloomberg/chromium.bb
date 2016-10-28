// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/sync/test/integration/password_manager_setting_migrator_helper.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"

using password_manager_setting_migrater_helper::ExpectPrefValuesOnClient;
using preferences_helper::GetPrefs;
using password_manager::prefs::kPasswordManagerSavingEnabled;
using password_manager::prefs::kCredentialsEnableService;

class TwoClientsPasswordManagerSettingMigratorServiceSyncTest
    : public SyncTest {
 public:
  TwoClientsPasswordManagerSettingMigratorServiceSyncTest()
      : SyncTest(TWO_CLIENT) {}

  ~TwoClientsPasswordManagerSettingMigratorServiceSyncTest() override {}

  bool SetupClients() override {
    if (!SyncTest::SetupClients())
      return false;
    DisableVerifier();
    return true;
  }

  // Changes the |pref_name| preference value on the client with |index| and
  // checks that the value is the same on both clients after the change.
  void TestPrefChangeOnClient(int index, const char* pref_name) {
    ASSERT_TRUE(BooleanPrefMatchChecker(kPasswordManagerSavingEnabled).Wait());
    ASSERT_TRUE(BooleanPrefMatchChecker(kCredentialsEnableService).Wait());

    preferences_helper::ChangeBooleanPref(index, pref_name);
    // Check that changed pref has the same value on both clients
    ASSERT_TRUE(BooleanPrefMatchChecker(kPasswordManagerSavingEnabled).Wait());
    ASSERT_TRUE(BooleanPrefMatchChecker(kCredentialsEnableService).Wait());
  }

  void EnsureMigrationStartsForClient(int index) {
    if (password_manager_setting_migrater_helper::EnsureFieldTrialSetup()) {
      password_manager_setting_migrater_helper::InitializePreferencesMigration(
          GetProfile(index));
    }
  }

  void ExpectValueOnBothClientsForBothPreference(bool value) {
    for (int i = 0; i < num_clients(); ++i) {
      ExpectPrefValuesOnClient(i, value, value);
    }
  }

  Profile* profile() { return GetProfile(0); }
  bool TestUsesSelfNotifications() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      TwoClientsPasswordManagerSettingMigratorServiceSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientsPasswordManagerSettingMigratorServiceSyncTest,
    E2E_ENABLED(ChangeLegacyPrefTestBothClientsWithMigration)) {
  ASSERT_TRUE(SetupSync());
  EnsureMigrationStartsForClient(0);
  EnsureMigrationStartsForClient(1);
  TestPrefChangeOnClient(1, kPasswordManagerSavingEnabled);
}

IN_PROC_BROWSER_TEST_F(TwoClientsPasswordManagerSettingMigratorServiceSyncTest,
                       E2E_ENABLED(ChangeNewPrefTestBothClientsWithMigration)) {
  ASSERT_TRUE(SetupClients());
  EnsureMigrationStartsForClient(0);
  EnsureMigrationStartsForClient(1);
  ASSERT_TRUE(SetupSync());
  TestPrefChangeOnClient(1, kCredentialsEnableService);
}

IN_PROC_BROWSER_TEST_F(
    TwoClientsPasswordManagerSettingMigratorServiceSyncTest,
    E2E_ENABLED(ChangeLegacyPrefOnMigratedClientOneClientsWithMigration)) {
  ASSERT_TRUE(SetupSync());
  EnsureMigrationStartsForClient(0);
  TestPrefChangeOnClient(0, kPasswordManagerSavingEnabled);
  ExpectValueOnBothClientsForBothPreference(false);
}

IN_PROC_BROWSER_TEST_F(
    TwoClientsPasswordManagerSettingMigratorServiceSyncTest,
    E2E_ENABLED(ChangeLegacyPrefOnNonMigratedClientOneClientsWithMigration)) {
  ASSERT_TRUE(SetupSync());
  EnsureMigrationStartsForClient(0);
  TestPrefChangeOnClient(1, kPasswordManagerSavingEnabled);
  ExpectValueOnBothClientsForBothPreference(false);
}

IN_PROC_BROWSER_TEST_F(TwoClientsPasswordManagerSettingMigratorServiceSyncTest,
    E2E_ENABLED(ChangeNewPrefOnMigratedClientOneClientsWithMigration)) {
  ASSERT_TRUE(SetupSync());
  EnsureMigrationStartsForClient(0);
  TestPrefChangeOnClient(0, kCredentialsEnableService);
  ExpectValueOnBothClientsForBothPreference(false);
}

IN_PROC_BROWSER_TEST_F(
    TwoClientsPasswordManagerSettingMigratorServiceSyncTest,
    E2E_ENABLED(ChangeNewPrefOnNonMigratedClientOneClientsWithMigration)) {
  ASSERT_TRUE(SetupSync());
  EnsureMigrationStartsForClient(0);
  TestPrefChangeOnClient(1, kCredentialsEnableService);
  ASSERT_TRUE(BooleanPrefMatchChecker(kPasswordManagerSavingEnabled).Wait());
  ExpectValueOnBothClientsForBothPreference(false);
}
