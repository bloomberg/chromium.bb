// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/password_manager_setting_migrator_helper.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"

using password_manager_setting_migrater_helper::ExpectPrefValuesOnClient;
using preferences_helper::AwaitBooleanPrefMatches;
using preferences_helper::ChangeBooleanPref;

class MultipleClientPasswordManagerSettingMigratorServiceSyncTest
    : public SyncTest {
 public:
  MultipleClientPasswordManagerSettingMigratorServiceSyncTest()
      : SyncTest(MULTIPLE_CLIENT) {}
  ~MultipleClientPasswordManagerSettingMigratorServiceSyncTest() override {}

  bool SetupClients() override {
    password_manager_setting_migrater_helper::SetupFieldTrial();
    if (!SyncTest::SetupClients())
      return false;
    DisableVerifier();
    return true;
  }

  void EnsureMigrationStartsForClient(int index) {
    password_manager_setting_migrater_helper::InitializePreferencesMigration(
        GetProfile(index));
  }

  bool TestUsesSelfNotifications() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      MultipleClientPasswordManagerSettingMigratorServiceSyncTest);
};

IN_PROC_BROWSER_TEST_F(
    MultipleClientPasswordManagerSettingMigratorServiceSyncTest,
    TwoClientsWithMigrationOneClientWithoutChangeFromOnToOff) {
  DisableVerifier();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EnsureMigrationStartsForClient(0);
  EnsureMigrationStartsForClient(1);
  // Client without migration changes the legacy pref.
  ChangeBooleanPref(2, password_manager::prefs::kPasswordManagerSavingEnabled);
  ASSERT_TRUE(AwaitBooleanPrefMatches(
      password_manager::prefs::kPasswordManagerSavingEnabled));
  // Check that new and legacy preferences have correct value on client with
  // migration.
  for (int i = 0; i < num_clients() - 1; ++i) {
    ExpectPrefValuesOnClient(i, false, false);
  }
}

IN_PROC_BROWSER_TEST_F(
    MultipleClientPasswordManagerSettingMigratorServiceSyncTest,
    TwoClientsWithMigrationOneClientWithoutChangeFromOffToOn) {
  DisableVerifier();
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EnsureMigrationStartsForClient(0);
  EnsureMigrationStartsForClient(1);
  ChangeBooleanPref(2, password_manager::prefs::kPasswordManagerSavingEnabled);
  ASSERT_TRUE(AwaitBooleanPrefMatches(
      password_manager::prefs::kPasswordManagerSavingEnabled));
  ChangeBooleanPref(2, password_manager::prefs::kPasswordManagerSavingEnabled);
  ASSERT_TRUE(AwaitBooleanPrefMatches(
      password_manager::prefs::kPasswordManagerSavingEnabled));
  for (int i = 0; i < num_clients() - 1; ++i) {
    ExpectPrefValuesOnClient(i, true, true);
  }
}
