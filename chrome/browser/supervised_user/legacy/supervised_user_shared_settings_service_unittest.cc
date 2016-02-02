// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor_wrapper_for_test.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using base::FundamentalValue;
using base::StringValue;
using base::Value;
using sync_pb::ManagedUserSharedSettingSpecifics;
using syncer::SUPERVISED_USER_SHARED_SETTINGS;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncChangeProcessorWrapperForTest;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncErrorFactory;
using syncer::SyncMergeResult;

namespace {

class MockSyncErrorFactory : public syncer::SyncErrorFactory {
 public:
  explicit MockSyncErrorFactory(syncer::ModelType type);
  ~MockSyncErrorFactory() override;

  // SyncErrorFactory implementation:
  syncer::SyncError CreateAndUploadError(
      const tracked_objects::Location& location,
      const std::string& message) override;

 private:
  syncer::ModelType type_;

  DISALLOW_COPY_AND_ASSIGN(MockSyncErrorFactory);
};

MockSyncErrorFactory::MockSyncErrorFactory(syncer::ModelType type)
    : type_(type) {}

MockSyncErrorFactory::~MockSyncErrorFactory() {}

syncer::SyncError MockSyncErrorFactory::CreateAndUploadError(
    const tracked_objects::Location& location,
    const std::string& message) {
  return syncer::SyncError(location, SyncError::DATATYPE_ERROR, message, type_);
}

// Convenience method to allow us to use EXPECT_EQ to compare values.
std::string ToJson(const Value* value) {
  if (!value)
    return std::string();

  std::string json_value;
  base::JSONWriter::Write(*value, &json_value);
  return json_value;
}

}  // namespace

class SupervisedUserSharedSettingsServiceTest : public ::testing::Test {
 protected:
  typedef base::CallbackList<void(const std::string&, const std::string&)>
      CallbackList;

  SupervisedUserSharedSettingsServiceTest()
      : settings_service_(profile_.GetPrefs()) {}
  ~SupervisedUserSharedSettingsServiceTest() override {}

  SyncMergeResult StartSyncing(const syncer::SyncDataList& initial_sync_data) {
    sync_processor_.reset(new syncer::FakeSyncChangeProcessor);
    scoped_ptr<syncer::SyncErrorFactory> error_handler(
        new MockSyncErrorFactory(SUPERVISED_USER_SHARED_SETTINGS));
    SyncMergeResult result = settings_service_.MergeDataAndStartSyncing(
        SUPERVISED_USER_SHARED_SETTINGS, initial_sync_data,
        scoped_ptr<SyncChangeProcessor>(
            new SyncChangeProcessorWrapperForTest(sync_processor_.get())),
        std::move(error_handler));
    EXPECT_FALSE(result.error().IsSet());
    return result;
  }

  const base::DictionaryValue* GetAllSettings() {
    return profile_.GetPrefs()->GetDictionary(
        prefs::kSupervisedUserSharedSettings);
  }

  void VerifySyncChangesAndClear() {
    SyncChangeList& changes = sync_processor_->changes();
    for (const SyncChange& sync_change : changes) {
      const sync_pb::ManagedUserSharedSettingSpecifics& setting =
          sync_change.sync_data().GetSpecifics().managed_user_shared_setting();
      EXPECT_EQ(
          setting.value(),
          ToJson(settings_service_.GetValue(setting.mu_id(), setting.key())));
    }
    changes.clear();
  }

  // testing::Test overrides:
  void SetUp() override {
    subscription_ = settings_service_.Subscribe(
        base::Bind(&SupervisedUserSharedSettingsServiceTest::OnSettingChanged,
                   base::Unretained(this)));
  }

  void TearDown() override { settings_service_.Shutdown(); }

  void OnSettingChanged(const std::string& su_id, const std::string& key) {
    const Value* value = settings_service_.GetValue(su_id, key);
    ASSERT_TRUE(value);
    changed_settings_.push_back(
        SupervisedUserSharedSettingsService::CreateSyncDataForSetting(
            su_id, key, *value, true));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  SupervisedUserSharedSettingsService settings_service_;
  SyncDataList changed_settings_;

  scoped_ptr<CallbackList::Subscription> subscription_;

  scoped_ptr<syncer::FakeSyncChangeProcessor> sync_processor_;
};

TEST_F(SupervisedUserSharedSettingsServiceTest, Empty) {
  StartSyncing(SyncDataList());
  EXPECT_EQ(0u, sync_processor_->changes().size());
  EXPECT_EQ(0u, changed_settings_.size());
  EXPECT_EQ(
      0u,
      settings_service_.GetAllSyncData(SUPERVISED_USER_SHARED_SETTINGS).size());
  EXPECT_EQ(0u, GetAllSettings()->size());
}

TEST_F(SupervisedUserSharedSettingsServiceTest, SetAndGet) {
  StartSyncing(SyncDataList());

  const char kIdA[] = "aaaaaa";
  const char kIdB[] = "bbbbbb";
  const char kIdC[] = "cccccc";

  StringValue name("Jack");
  FundamentalValue age(8);
  StringValue bar("bar");
  settings_service_.SetValue(kIdA, "name", name);
  ASSERT_EQ(1u, sync_processor_->changes().size());
  VerifySyncChangesAndClear();
  settings_service_.SetValue(kIdA, "age", FundamentalValue(6));
  ASSERT_EQ(1u, sync_processor_->changes().size());
  VerifySyncChangesAndClear();
  settings_service_.SetValue(kIdA, "age", age);
  ASSERT_EQ(1u, sync_processor_->changes().size());
  VerifySyncChangesAndClear();
  settings_service_.SetValue(kIdB, "foo", bar);
  ASSERT_EQ(1u, sync_processor_->changes().size());
  VerifySyncChangesAndClear();

  EXPECT_EQ(
      3u,
      settings_service_.GetAllSyncData(SUPERVISED_USER_SHARED_SETTINGS).size());

  EXPECT_EQ(ToJson(&name), ToJson(settings_service_.GetValue(kIdA, "name")));
  EXPECT_EQ(ToJson(&age), ToJson(settings_service_.GetValue(kIdA, "age")));
  EXPECT_EQ(ToJson(&bar), ToJson(settings_service_.GetValue(kIdB, "foo")));
  EXPECT_FALSE(settings_service_.GetValue(kIdA, "foo"));
  EXPECT_FALSE(settings_service_.GetValue(kIdB, "name"));
  EXPECT_FALSE(settings_service_.GetValue(kIdC, "name"));
}

TEST_F(SupervisedUserSharedSettingsServiceTest, Merge) {
  // Set initial values, then stop syncing so we can restart.
  SyncMergeResult result = StartSyncing(SyncDataList());
  EXPECT_EQ(0, result.num_items_added());
  EXPECT_EQ(0, result.num_items_deleted());
  EXPECT_EQ(0, result.num_items_modified());
  EXPECT_EQ(0, result.num_items_before_association());
  EXPECT_EQ(0, result.num_items_after_association());

  const char kIdA[] = "aaaaaa";
  const char kIdB[] = "bbbbbb";
  const char kIdC[] = "cccccc";

  FundamentalValue age(8);
  StringValue bar("bar");
  settings_service_.SetValue(kIdA, "name", StringValue("Jack"));
  settings_service_.SetValue(kIdA, "age", age);
  settings_service_.SetValue(kIdB, "foo", bar);

  settings_service_.StopSyncing(SUPERVISED_USER_SHARED_SETTINGS);

  StringValue name("Jill");
  StringValue blurp("blurp");
  SyncDataList sync_data;
  sync_data.push_back(
      SupervisedUserSharedSettingsService::CreateSyncDataForSetting(
          kIdA, "name", name, true));
  sync_data.push_back(
      SupervisedUserSharedSettingsService::CreateSyncDataForSetting(
          kIdC, "baz", blurp, true));

  result = StartSyncing(sync_data);

  EXPECT_EQ(1, result.num_items_added());
  EXPECT_EQ(0, result.num_items_deleted());
  EXPECT_EQ(1, result.num_items_modified());
  EXPECT_EQ(3, result.num_items_before_association());
  EXPECT_EQ(4, result.num_items_after_association());

  EXPECT_EQ(2u, sync_processor_->changes().size());
  VerifySyncChangesAndClear();
  EXPECT_EQ(2u, changed_settings_.size());

  EXPECT_EQ(
      4u,
      settings_service_.GetAllSyncData(SUPERVISED_USER_SHARED_SETTINGS).size());
  EXPECT_EQ(ToJson(&name),
            ToJson(settings_service_.GetValue(kIdA, "name")));
  EXPECT_EQ(ToJson(&age), ToJson(settings_service_.GetValue(kIdA, "age")));
  EXPECT_EQ(ToJson(&bar), ToJson(settings_service_.GetValue(kIdB, "foo")));
  EXPECT_EQ(ToJson(&blurp), ToJson(settings_service_.GetValue(kIdC, "baz")));
  EXPECT_FALSE(settings_service_.GetValue(kIdA, "foo"));
  EXPECT_FALSE(settings_service_.GetValue(kIdB, "name"));
  EXPECT_FALSE(settings_service_.GetValue(kIdC, "name"));
}

TEST_F(SupervisedUserSharedSettingsServiceTest, ProcessChanges) {
  StartSyncing(SyncDataList());

  const char kIdA[] = "aaaaaa";
  const char kIdB[] = "bbbbbb";
  const char kIdC[] = "cccccc";

  FundamentalValue age(8);
  StringValue bar("bar");
  settings_service_.SetValue(kIdA, "name", StringValue("Jack"));
  settings_service_.SetValue(kIdA, "age", age);
  settings_service_.SetValue(kIdB, "foo", bar);

  StringValue name("Jill");
  StringValue blurp("blurp");
  SyncChangeList changes;
  changes.push_back(
      SyncChange(FROM_HERE,
                 SyncChange::ACTION_UPDATE,
                 SupervisedUserSharedSettingsService::CreateSyncDataForSetting(
                     kIdA, "name", name, true)));
  changes.push_back(
      SyncChange(FROM_HERE,
                 SyncChange::ACTION_ADD,
                 SupervisedUserSharedSettingsService::CreateSyncDataForSetting(
                     kIdC, "baz", blurp, true)));
  SyncError error = settings_service_.ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_FALSE(error.IsSet()) << error.ToString();
  EXPECT_EQ(2u, changed_settings_.size());

  EXPECT_EQ(
      4u,
      settings_service_.GetAllSyncData(SUPERVISED_USER_SHARED_SETTINGS).size());
  EXPECT_EQ(ToJson(&name),
            ToJson(settings_service_.GetValue(kIdA, "name")));
  EXPECT_EQ(ToJson(&age), ToJson(settings_service_.GetValue(kIdA, "age")));
  EXPECT_EQ(ToJson(&bar), ToJson(settings_service_.GetValue(kIdB, "foo")));
  EXPECT_EQ(ToJson(&blurp), ToJson(settings_service_.GetValue(kIdC, "baz")));
  EXPECT_FALSE(settings_service_.GetValue(kIdA, "foo"));
  EXPECT_FALSE(settings_service_.GetValue(kIdB, "name"));
  EXPECT_FALSE(settings_service_.GetValue(kIdC, "name"));
}
