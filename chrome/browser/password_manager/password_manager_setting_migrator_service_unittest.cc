// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/internal_api/public/attachments/attachment_service_proxy_for_test.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFieldTrialName[] = "PasswordManagerSettingsMigration";
const char kEnabledGroupName[] = "Enable";

enum BooleanPrefState {
  OFF,
  ON,
  EMPTY,  // datatype bucket is empty
};

syncer::SyncData CreatePrefSyncData(const std::string& name, bool value) {
  base::FundamentalValue bool_value(value);
  std::string serialized;
  base::JSONWriter::Write(bool_value, &serialized);
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref = nullptr;
  if (name == password_manager::prefs::kPasswordManagerSavingEnabled)
    pref = specifics.mutable_preference();
  else if (name == password_manager::prefs::kCredentialsEnableService)
    pref = specifics.mutable_priority_preference()->mutable_preference();
  else
    NOTREACHED() << "Wrong preference name: " << name;
  pref->set_name(name);
  pref->set_value(serialized);
  return syncer::SyncData::CreateRemoteData(
      1, specifics, base::Time(), syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create());
}

// Emulates start of the syncing for the specific sync type. If |name| is
// kPasswordManagerSavingEnabled preference, then it's PREFERENCE data type.
// If |name| is kCredentialsEnableService  pref, then it's PRIORITY_PREFERENCE
// data type.
void StartSyncingPref(syncable_prefs::PrefServiceSyncable* prefs,
                      const std::string& name,
                      BooleanPrefState pref_state_in_sync) {
  syncer::SyncDataList sync_data_list;
  if (pref_state_in_sync == EMPTY) {
    sync_data_list = syncer::SyncDataList();
  } else {
    sync_data_list.push_back(
        CreatePrefSyncData(name, pref_state_in_sync == ON));
  }

  syncer::ModelType type = syncer::UNSPECIFIED;
  if (name == password_manager::prefs::kPasswordManagerSavingEnabled)
    type = syncer::PREFERENCES;
  else if (name == password_manager::prefs::kCredentialsEnableService)
    type = syncer::PRIORITY_PREFERENCES;
  ASSERT_NE(syncer::UNSPECIFIED, type) << "Wrong preference name: " << name;
  syncer::SyncableService* sync = prefs->GetSyncableService(type);
  sync->MergeDataAndStartSyncing(
      type, sync_data_list, scoped_ptr<syncer::SyncChangeProcessor>(
                                new syncer::FakeSyncChangeProcessor),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock));
}

}  // namespace

namespace password_manager {

class PasswordManagerSettingMigratorServiceTest : public testing::Test {
 protected:
  PasswordManagerSettingMigratorServiceTest() : field_trial_list_(nullptr) {}

  void SetUp() override { ResetProfile(); }

  void SetupLocalPrefState(const std::string& name, BooleanPrefState state) {
    PrefService* prefs = profile()->GetPrefs();
    if (state == ON)
      prefs->SetBoolean(name, true);
    else if (state == OFF)
      prefs->SetBoolean(name, false);
    else if (state == EMPTY)
      ASSERT_TRUE(prefs->FindPreference(name)->IsDefaultValue());
  }

  Profile* profile() { return profile_.get(); }

  void ResetProfile() {
    profile_ = TestingProfile::Builder().Build();
    ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &ProfileSyncServiceMock::BuildMockProfileSyncService);
    ON_CALL(*profile_sync_service(), CanSyncStart())
        .WillByDefault(testing::Return(true));
    base::FieldTrialList::CreateFieldTrial(kFieldTrialName, kEnabledGroupName);
  }

  void ExpectValuesForBothPrefValues(bool new_pref_value, bool old_pref_value) {
    PrefService* prefs = profile()->GetPrefs();
    EXPECT_EQ(new_pref_value,
              prefs->GetBoolean(prefs::kCredentialsEnableService));
    EXPECT_EQ(old_pref_value,
              prefs->GetBoolean(prefs::kPasswordManagerSavingEnabled));
  }

  ProfileSyncServiceMock* profile_sync_service() {
    return static_cast<ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile()));
  }

  void NotifyProfileAdded() {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_ADDED, content::Source<Profile>(profile()),
        content::NotificationService::NoDetails());
  }

  void TestOnLocalChange(const std::string& name, bool value) {
    PrefService* prefs = profile()->GetPrefs();
    prefs->SetBoolean(prefs::kCredentialsEnableService, !value);
    prefs->SetBoolean(prefs::kPasswordManagerSavingEnabled, !value);
    NotifyProfileAdded();
    prefs->SetBoolean(name, value);
    ExpectValuesForBothPrefValues(value, value);
  }

 private:
  base::FieldTrialList field_trial_list_;
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerSettingMigratorServiceTest);
};

TEST_F(PasswordManagerSettingMigratorServiceTest,
       ReconcileOnLocalChangeOfPasswordManagerSavingEnabledOn) {
  TestOnLocalChange(prefs::kPasswordManagerSavingEnabled, true);
}

TEST_F(PasswordManagerSettingMigratorServiceTest,
       ReconcileOnLocalChangeOfPasswordManagerSavingEnabledOff) {
  TestOnLocalChange(prefs::kPasswordManagerSavingEnabled, false);
}

TEST_F(PasswordManagerSettingMigratorServiceTest,
       ReconcileOnLocalChangeOfCredentialsEnableServiceOn) {
  TestOnLocalChange(prefs::kCredentialsEnableService, true);
}

TEST_F(PasswordManagerSettingMigratorServiceTest,
       ReconcileOnLocalChangeOfCredentialsEnableServiceOff) {
  TestOnLocalChange(prefs::kCredentialsEnableService, false);
}

TEST_F(PasswordManagerSettingMigratorServiceTest,
       ReconcileWhenWhenBothPrefsTypesArrivesFromSync) {
  const struct {
    BooleanPrefState new_pref_local_value;
    BooleanPrefState old_pref_local_value;
    BooleanPrefState new_pref_sync_value;
    BooleanPrefState old_pref_sync_value;
    bool result_value;
  } kTestingTable[] = {
      {EMPTY, EMPTY, EMPTY, EMPTY, true},
      {OFF, OFF, EMPTY, EMPTY, false},
      {OFF, OFF, OFF, ON, true},
      {OFF, OFF, OFF, OFF, false},
      {OFF, OFF, ON, OFF, true},
      {ON, OFF, ON, ON, true},
      {ON, OFF, ON, OFF, false},
      {ON, OFF, OFF, ON, false},
      {ON, ON, OFF, EMPTY, false},
      {ON, ON, ON, EMPTY, true},
      {ON, ON, EMPTY, OFF, false},
      {ON, ON, EMPTY, ON, true},
      {ON, ON, OFF, OFF, false},
      {ON, ON, OFF, ON, false},
      {ON, ON, ON, OFF, false},
      {ON, ON, ON, ON, true},
      {EMPTY, EMPTY, ON, EMPTY, true},
      {EMPTY, EMPTY, OFF, EMPTY, false},
      {EMPTY, EMPTY, EMPTY, ON, true},
      {EMPTY, EMPTY, EMPTY, OFF, false},
      {OFF, ON, ON, OFF, false},
      {OFF, ON, OFF, ON, false},
      {OFF, ON, ON, ON, true},
      {ON, OFF, EMPTY, EMPTY, false},
  };

  for (const auto& test_case : kTestingTable) {
    ResetProfile();
    SCOPED_TRACE(testing::Message("Local data = ")
                 << test_case.new_pref_local_value << " "
                 << test_case.old_pref_local_value);
    SCOPED_TRACE(testing::Message("Sync data = ")
                 << test_case.new_pref_sync_value << " "
                 << test_case.old_pref_sync_value);
    SetupLocalPrefState(prefs::kPasswordManagerSavingEnabled,
                        test_case.old_pref_local_value);
    SetupLocalPrefState(prefs::kCredentialsEnableService,
                        test_case.new_pref_local_value);
    NotifyProfileAdded();
    syncable_prefs::PrefServiceSyncable* prefs =
        PrefServiceSyncableFromProfile(profile());
    StartSyncingPref(prefs, prefs::kCredentialsEnableService,
                     test_case.new_pref_sync_value);
    StartSyncingPref(prefs, prefs::kPasswordManagerSavingEnabled,
                     test_case.old_pref_sync_value);
    ExpectValuesForBothPrefValues(test_case.result_value,
                                  test_case.result_value);
  }
}

TEST_F(PasswordManagerSettingMigratorServiceTest,
       ReconcileWhenSyncIsNotExpectedPasswordManagerEnabledOff) {
  syncable_prefs::PrefServiceSyncable* prefs =
      PrefServiceSyncableFromProfile(profile());
  prefs->SetBoolean(prefs::kPasswordManagerSavingEnabled, false);
  ON_CALL(*profile_sync_service(), CanSyncStart())
      .WillByDefault(testing::Return(false));
  NotifyProfileAdded();
  ExpectValuesForBothPrefValues(false, false);
}

TEST_F(PasswordManagerSettingMigratorServiceTest,
       ReconcileWhenSyncIsNotExpectedPasswordManagerEnabledOn) {
  syncable_prefs::PrefServiceSyncable* prefs =
      PrefServiceSyncableFromProfile(profile());
  prefs->SetBoolean(prefs::kPasswordManagerSavingEnabled, true);
  ASSERT_EQ(prefs->GetBoolean(prefs::kCredentialsEnableService), true);
  ON_CALL(*profile_sync_service(), CanSyncStart())
      .WillByDefault(testing::Return(false));
  NotifyProfileAdded();
  ExpectValuesForBothPrefValues(true, true);
}

TEST_F(PasswordManagerSettingMigratorServiceTest,
       ReconcileWhenSyncIsNotExpectedDefaultValuesForPrefs) {
  syncable_prefs::PrefServiceSyncable* prefs =
      PrefServiceSyncableFromProfile(profile());
  ASSERT_EQ(prefs->GetBoolean(prefs::kCredentialsEnableService), true);
  ON_CALL(*profile_sync_service(), CanSyncStart())
      .WillByDefault(testing::Return(false));
  NotifyProfileAdded();
  ExpectValuesForBothPrefValues(true, true);
}

}  // namespace password_manager
