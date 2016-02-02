// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/macros.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/sync/test/integration/password_manager_setting_migrator_helper.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/syncable_prefs/pref_service_syncable_observer.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/preference_specifics.pb.h"
#include "sync/protocol/priority_preference_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/test/fake_server/unique_client_entity.h"

using password_manager_setting_migrater_helper::ExpectPrefValuesOnClient;
using preferences_helper::AwaitBooleanPrefMatches;
using preferences_helper::BooleanPrefMatches;
using preferences_helper::ChangeBooleanPref;
using preferences_helper::GetPrefs;

namespace {

void InjectPreferenceValueToFakeServer(fake_server::FakeServer* fake_server,
                                       const std::string& name,
                                       bool value) {
  std::string serialized;
  base::FundamentalValue bool_value(value);
  base::JSONWriter::Write(bool_value, &serialized);
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref = nullptr;
  if (name == password_manager::prefs::kPasswordManagerSavingEnabled) {
    pref = specifics.mutable_preference();
  } else if (name == password_manager::prefs::kCredentialsEnableService) {
    pref = specifics.mutable_priority_preference()->mutable_preference();
  } else {
    NOTREACHED() << "Wrong preference name: " << name;
  }
  pref->set_name(name);
  pref->set_value(serialized);
  const std::string id = "settings_reconciliation";
  fake_server->InjectEntity(
      fake_server::UniqueClientEntity::CreateForInjection(id, specifics));
}

class SyncStateObserver : public syncable_prefs::PrefServiceSyncableObserver {
 public:
  SyncStateObserver() : profile_(nullptr), prefs_(nullptr) {}

  ~SyncStateObserver() override {}

  void InitWithProfile(Profile* profile) {
    profile_ = profile;
    prefs_ = PrefServiceSyncableFromProfile(profile_);
    prefs_->AddObserver(this);
  }

  void OnIsSyncingChanged() override {
    if (!prefs_->IsSyncing() && !prefs_->IsPrioritySyncing()) {
      // TODO(melandory): postpone observer removal until destruction.
      prefs_->RemoveObserver(this);
      password_manager_setting_migrater_helper::InitializePreferencesMigration(
          profile_);
    }
  }

 private:
  Profile* profile_;
  syncable_prefs::PrefServiceSyncable* prefs_;

  DISALLOW_COPY_AND_ASSIGN(SyncStateObserver);
};

}  // namespace

class SingleClientPasswordManagerSettingMigratorServiceSyncTest
    : public SyncTest {
 public:
  SingleClientPasswordManagerSettingMigratorServiceSyncTest()
      : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientPasswordManagerSettingMigratorServiceSyncTest() override {}

  void AssertPrefValues(bool new_pref_value, bool old_pref_value) {
    ExpectPrefValuesOnClient(0, new_pref_value, old_pref_value);
  }

  void SetLocalPrefValues(bool new_pref_local_value,
                          bool old_pref_local_value) {
    using password_manager::prefs::kCredentialsEnableService;
    using password_manager::prefs::kPasswordManagerSavingEnabled;
    PrefService* prefs = GetPrefs(0);
    prefs->SetBoolean(kCredentialsEnableService, new_pref_local_value);
    prefs->SetBoolean(kPasswordManagerSavingEnabled, old_pref_local_value);
    AssertPrefValues(new_pref_local_value, old_pref_local_value);
  }

  void InjectNewValues(bool new_pref_sync_value, bool old_pref_sync_value) {
    using namespace password_manager::prefs;
    InjectPreferenceValueToFakeServer(
        GetFakeServer(), kCredentialsEnableService, new_pref_sync_value);
    InjectPreferenceValueToFakeServer(
        GetFakeServer(), kPasswordManagerSavingEnabled, old_pref_sync_value);
  }

  void InitMigrationService() {
    password_manager_setting_migrater_helper::SetupFieldTrial();
    sync_state_observer_.InitWithProfile(GetProfile(0));
  }

 private:
  SyncStateObserver sync_state_observer_;

  DISALLOW_COPY_AND_ASSIGN(
      SingleClientPasswordManagerSettingMigratorServiceSyncTest);
};

IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordManagerSettingMigratorServiceSyncTest,
    LocalOnOnSyncOffOff) {
  ASSERT_TRUE(SetupClients());
  SetLocalPrefValues(true /* kCredentialsEnableService */,
                     true /* kPasswordManagerSavingEnabled */);
  InjectNewValues(false /* kCredentialsEnableService */,
                  false /* kPasswordManagerSavingEnabled */);
  InitMigrationService();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_integration_test_util::AwaitCommitActivityCompletion(
      GetSyncService((0))));
  AssertPrefValues(false /* kCredentialsEnableService */,
                   false /* kPasswordManagerSavingEnabled */);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordManagerSettingMigratorServiceSyncTest,
    LocalOnOnSyncOnOff) {
  ASSERT_TRUE(SetupClients());
  InjectNewValues(true /* kCredentialsEnableService */,
                  false /* kPasswordManagerSavingEnabled */);
  InitMigrationService();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_integration_test_util::AwaitCommitActivityCompletion(
      GetSyncService((0))));
  AssertPrefValues(false /* kCredentialsEnableService */,
                   false /* kPasswordManagerSavingEnabled */);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordManagerSettingMigratorServiceSyncTest,
    LocalOnOnSyncOffOn) {
  ASSERT_TRUE(SetupClients());
  SetLocalPrefValues(true /* kCredentialsEnableService */,
                     true /* kPasswordManagerSavingEnabled */);
  InjectNewValues(false /* kCredentialsEnableService */,
                  true /* kPasswordManagerSavingEnabled */);
  InitMigrationService();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_integration_test_util::AwaitCommitActivityCompletion(
      GetSyncService((0))));
  AssertPrefValues(false /* kCredentialsEnableService */,
                   false /* kPasswordManagerSavingEnabled */);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordManagerSettingMigratorServiceSyncTest,
    LocalOnOffSyncOnOn) {
  ASSERT_TRUE(SetupClients());
  SetLocalPrefValues(true /* kCredentialsEnableService */,
                     false /* kPasswordManagerSavingEnabled */);
  InjectNewValues(true /* kCredentialsEnableService */,
                  true /* kPasswordManagerSavingEnabled */);
  InitMigrationService();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_integration_test_util::AwaitCommitActivityCompletion(
      GetSyncService((0))));
  AssertPrefValues(true /* kCredentialsEnableService */,
                   true /* kPasswordManagerSavingEnabled */);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordManagerSettingMigratorServiceSyncTest,
    LocalOnOffSyncOnOff) {
  ASSERT_TRUE(SetupClients());
  SetLocalPrefValues(true /* kCredentialsEnableService */,
                     false /* kPasswordManagerSavingEnabled */);
  InjectNewValues(true /* kCredentialsEnableService */,
                  false /* kPasswordManagerSavingEnabled */);
  InitMigrationService();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_integration_test_util::AwaitCommitActivityCompletion(
      GetSyncService((0))));
  AssertPrefValues(false /* kCredentialsEnableService */,
                   false /* kPasswordManagerSavingEnabled */);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordManagerSettingMigratorServiceSyncTest,
    LocalOnOffSyncOffOn) {
  ASSERT_TRUE(SetupClients());
  SetLocalPrefValues(true /* kCredentialsEnableService */,
                     false /* kPasswordManagerSavingEnabled */);
  InjectNewValues(false /* kCredentialsEnableService */,
                  true /* kPasswordManagerSavingEnabled */);
  InitMigrationService();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_integration_test_util::AwaitCommitActivityCompletion(
      GetSyncService((0))));
  AssertPrefValues(false /* kCredentialsEnableService */,
                   false /* kPasswordManagerSavingEnabled */);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordManagerSettingMigratorServiceSyncTest,
    LocalOffOffSyncOffOn) {
  ASSERT_TRUE(SetupClients());
  SetLocalPrefValues(false /* kCredentialsEnableService */,
                     false /* kPasswordManagerSavingEnabled */);
  InjectNewValues(false /* kCredentialsEnableService */,
                  true /* kPasswordManagerSavingEnabled */);
  InitMigrationService();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_integration_test_util::AwaitCommitActivityCompletion(
      GetSyncService((0))));
  AssertPrefValues(true /* kCredentialsEnableService */,
                   true /* kPasswordManagerSavingEnabled */);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordManagerSettingMigratorServiceSyncTest,
    LocalOffOffSyncOnOff) {
  ASSERT_TRUE(SetupClients());
  SetLocalPrefValues(false /* kCredentialsEnableService */,
                     false /* kPasswordManagerSavingEnabled */);
  InjectNewValues(true /* kCredentialsEnableService */,
                  false /* kPasswordManagerSavingEnabled */);
  InitMigrationService();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_integration_test_util::AwaitCommitActivityCompletion(
      GetSyncService((0))));
  AssertPrefValues(true /* kCredentialsEnableService */,
                   true /* kPasswordManagerSavingEnabled */);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordManagerSettingMigratorServiceSyncTest,
    LocalOffOnSyncOffOn) {
  ASSERT_TRUE(SetupClients());
  SetLocalPrefValues(false /* kCredentialsEnableService */,
                     true /* kPasswordManagerSavingEnabled */);
  InjectNewValues(false /* kCredentialsEnableService */,
                  true /* kPasswordManagerSavingEnabled */);
  InitMigrationService();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_integration_test_util::AwaitCommitActivityCompletion(
      GetSyncService((0))));
  AssertPrefValues(false /* kCredentialsEnableService */,
                   false /* kPasswordManagerSavingEnabled */);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordManagerSettingMigratorServiceSyncTest,
    LocalOffOffSyncOffOff) {
  ASSERT_TRUE(SetupClients());
  SetLocalPrefValues(false /* kCredentialsEnableService */,
                     false /* kPasswordManagerSavingEnabled */);
  InjectNewValues(false /* kCredentialsEnableService */,
                  false /* kPasswordManagerSavingEnabled */);
  InitMigrationService();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_integration_test_util::AwaitCommitActivityCompletion(
      GetSyncService((0))));
  AssertPrefValues(false /* kCredentialsEnableService */,
                   false /* kPasswordManagerSavingEnabled */);
}

IN_PROC_BROWSER_TEST_F(
    SingleClientPasswordManagerSettingMigratorServiceSyncTest,
    LocalOnOnSyncOnOn) {
  ASSERT_TRUE(SetupClients());
  SetLocalPrefValues(true /* kCredentialsEnableService */,
                     true /* kPasswordManagerSavingEnabled */);
  InjectNewValues(true /* kCredentialsEnableService */,
                  true /* kPasswordManagerSavingEnabled */);
  InitMigrationService();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(sync_integration_test_util::AwaitCommitActivityCompletion(
      GetSyncService((0))));
  AssertPrefValues(true /* kCredentialsEnableService */,
                   true /* kPasswordManagerSavingEnabled */);
}
