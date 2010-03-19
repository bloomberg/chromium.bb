// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/preference_change_processor.h"
#include "chrome/browser/sync/glue/preference_data_type_controller.h"
#include "chrome/browser/sync/glue/preference_model_associator.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/sync_backend_host_mock.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::JSONReader;
using browser_sync::PreferenceChangeProcessor;
using browser_sync::PreferenceDataTypeController;
using browser_sync::PreferenceModelAssociator;
using browser_sync::SyncBackendHost;
using browser_sync::SyncBackendHostMock;
using sync_api::SyncManager;
using testing::_;
using testing::Return;

class ProfileSyncServicePreferenceTest : public testing::Test {
 protected:
  ProfileSyncServicePreferenceTest()
      : ui_thread_(ChromeThread::UI, &message_loop_) {
  }

  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->set_has_history_service(true);
  }

  virtual void TearDown() {
    service_.reset();
    profile_.reset();
    MessageLoop::current()->RunAllPending();
  }

  void StartSyncService() {
    if (!service_.get()) {
      service_.reset(new TestProfileSyncService(&factory_,
                                                profile_.get(),
                                                false));

      // Register the preference data type.
      model_associator_ =
          new TestModelAssociator<PreferenceModelAssociator>(service_.get(),
                                                             service_.get());
      change_processor_ = new PreferenceChangeProcessor(model_associator_,
                                                        service_.get());
      EXPECT_CALL(factory_, CreatePreferenceSyncComponents(_, _)).
          WillOnce(Return(ProfileSyncFactory::SyncComponents(
              model_associator_, change_processor_)));
      EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
          WillOnce(MakeDataTypeManager(&backend_mock_));

      service_->RegisterDataTypeController(
          new PreferenceDataTypeController(&factory_,
                                           service_.get()));
      service_->Initialize();
    }
    // The service may have already started sync automatically if it's already
    // enabled by user once.
    if (!service_->HasSyncSetupCompleted())
      service_->EnableForUser();
  }

  SyncBackendHost* backend() { return service_->backend_.get(); }

  const Value& GetPreferenceValue(const std::wstring& name) {
    const PrefService::Preference* preference =
        profile_->GetPrefs()->FindPreference(name.c_str());
    return *preference->GetValue();
  }

  // Caller gets ownership of the returned value.
  const Value* GetSyncedValue(const std::wstring& name) {
    sync_api::ReadTransaction trans(backend()->GetUserShareHandle());
    sync_api::ReadNode node(&trans);

    int64 node_id = model_associator_->GetSyncIdFromChromeId(name);
    EXPECT_NE(sync_api::kInvalidId, node_id);
    EXPECT_TRUE(node.InitByIdLookup(node_id));
    EXPECT_EQ(name, node.GetTitle());

    const sync_pb::PreferenceSpecifics& specifics(
        node.GetPreferenceSpecifics());

    JSONReader reader;
    return reader.JsonToValue(specifics.value(), false, false);
  }

  // Caller gets ownership of the returned change record.
  SyncManager::ChangeRecord* SetSyncedValue(const std::wstring& name,
                                            const Value& value) {
    sync_api::WriteTransaction trans(backend()->GetUserShareHandle());
    sync_api::WriteNode node(&trans);

    int64 node_id = model_associator_->GetSyncIdFromChromeId(name);
    EXPECT_NE(sync_api::kInvalidId, node_id);
    EXPECT_TRUE(node.InitByIdLookup(node_id));

    std::string serialized;
    JSONStringValueSerializer json(&serialized);
    EXPECT_TRUE(json.Serialize(value));

    sync_pb::PreferenceSpecifics preference;
    preference.set_name(WideToUTF8(name));
    preference.set_value(serialized);
    node.SetPreferenceSpecifics(preference);
    node.SetTitle(name);

    SyncManager::ChangeRecord* record = new SyncManager::ChangeRecord();
    record->action = SyncManager::ChangeRecord::ACTION_UPDATE;
    record->id = node_id;
    return record;
  }

  scoped_ptr<TestProfileSyncService> service_;
  scoped_ptr<TestingProfile> profile_;
  ProfileSyncFactoryMock factory_;
  SyncBackendHostMock backend_mock_;

  PreferenceModelAssociator* model_associator_;
  PreferenceChangeProcessor* change_processor_;

  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;
};

TEST_F(ProfileSyncServicePreferenceTest, ModelAssociation) {
  StartSyncService();
  const std::set<std::wstring>& names = model_associator_->synced_preferences();
  ASSERT_LT(0U, names.size());

  for (std::set<std::wstring>::const_iterator it = names.begin();
       it != names.end(); ++it) {
    const Value& expected = GetPreferenceValue(*it);
    scoped_ptr<const Value> actual(GetSyncedValue(*it));
    EXPECT_TRUE(expected.Equals(actual.get())) << *it;
  }
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedPreference) {
  StartSyncService();

  scoped_ptr<Value> expected(
      Value::CreateStringValue("http://example.com/test/foo"));
  profile_->GetPrefs()->Set(prefs::kHomePage, *expected);

  scoped_ptr<const Value> actual(GetSyncedValue(prefs::kHomePage));
  ASSERT_TRUE(expected->Equals(actual.get()));
}

TEST_F(ProfileSyncServicePreferenceTest, UpdatedSyncNode) {
  StartSyncService();

  scoped_ptr<Value> expected(
      Value::CreateStringValue("http://example.com/test/bar"));
  scoped_ptr<SyncManager::ChangeRecord> record(
      SetSyncedValue(prefs::kHomePage, *expected));
  {
    sync_api::WriteTransaction trans(backend()->GetUserShareHandle());
    change_processor_->ApplyChangesFromSyncModel(&trans, record.get(), 1);
  }

  const Value& actual = GetPreferenceValue(prefs::kHomePage);
  ASSERT_TRUE(expected->Equals(&actual));
}
