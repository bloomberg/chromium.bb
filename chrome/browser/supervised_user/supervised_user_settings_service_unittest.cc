// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/prefs/testing_pref_store.h"
#include "base/strings/string_util.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor_wrapper_for_test.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockSyncErrorFactory : public syncer::SyncErrorFactory {
 public:
  explicit MockSyncErrorFactory(syncer::ModelType type);
  virtual ~MockSyncErrorFactory();

  // SyncErrorFactory implementation:
  virtual syncer::SyncError CreateAndUploadError(
      const tracked_objects::Location& location,
      const std::string& message) OVERRIDE;

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
  return syncer::SyncError(location,
                           syncer::SyncError::DATATYPE_ERROR,
                           message,
                           type_);
}

}  // namespace

const char kAtomicItemName[] = "X-Wombat";
const char kSettingsName[] = "TestingSetting";
const char kSettingsValue[] = "SettingsValue";
const char kSplitItemName[] = "X-SuperMoosePowers";

class SupervisedUserSettingsServiceTest : public ::testing::Test {
 protected:
  SupervisedUserSettingsServiceTest() {}
  virtual ~SupervisedUserSettingsServiceTest() {}

  scoped_ptr<syncer::SyncChangeProcessor> CreateSyncProcessor() {
    sync_processor_.reset(new syncer::FakeSyncChangeProcessor);
    return scoped_ptr<syncer::SyncChangeProcessor>(
        new syncer::SyncChangeProcessorWrapperForTest(sync_processor_.get()));
  }

  void StartSyncing(const syncer::SyncDataList& initial_sync_data) {
    scoped_ptr<syncer::SyncErrorFactory> error_handler(
        new MockSyncErrorFactory(syncer::SUPERVISED_USER_SETTINGS));
    syncer::SyncMergeResult result = settings_service_.MergeDataAndStartSyncing(
        syncer::SUPERVISED_USER_SETTINGS,
        initial_sync_data,
        CreateSyncProcessor(),
        error_handler.Pass());
    EXPECT_FALSE(result.error().IsSet());
  }

  void UploadSplitItem(const std::string& key, const std::string& value) {
    split_items_.SetStringWithoutPathExpansion(key, value);
    settings_service_.UploadItem(
        SupervisedUserSettingsService::MakeSplitSettingKey(kSplitItemName,
                                                           key),
        scoped_ptr<base::Value>(new base::StringValue(value)));
  }

  void UploadAtomicItem(const std::string& value) {
    atomic_setting_value_.reset(new base::StringValue(value));
    settings_service_.UploadItem(
        kAtomicItemName,
        scoped_ptr<base::Value>(new base::StringValue(value)));
  }

  void VerifySyncDataItem(syncer::SyncData sync_data) {
    const sync_pb::ManagedUserSettingSpecifics& supervised_user_setting =
        sync_data.GetSpecifics().managed_user_setting();
    base::Value* expected_value = NULL;
    if (supervised_user_setting.name() == kAtomicItemName) {
      expected_value = atomic_setting_value_.get();
    } else {
      EXPECT_TRUE(StartsWithASCII(supervised_user_setting.name(),
                                  std::string(kSplitItemName) + ':',
                                  true));
      std::string key =
          supervised_user_setting.name().substr(strlen(kSplitItemName) + 1);
      EXPECT_TRUE(split_items_.GetWithoutPathExpansion(key, &expected_value));
    }

    scoped_ptr<base::Value> value(
        base::JSONReader::Read(supervised_user_setting.value()));
    EXPECT_TRUE(expected_value->Equals(value.get()));
  }

  void OnNewSettingsAvailable(const base::DictionaryValue* settings) {
    if (!settings)
      settings_.reset();
    else
      settings_.reset(settings->DeepCopy());
  }

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    TestingPrefStore* pref_store = new TestingPrefStore;
    settings_service_.Init(pref_store);
    settings_service_.Subscribe(
        base::Bind(&SupervisedUserSettingsServiceTest::OnNewSettingsAvailable,
                   base::Unretained(this)));
    pref_store->SetInitializationCompleted();
    ASSERT_FALSE(settings_);
    settings_service_.SetActive(true);
    ASSERT_TRUE(settings_);
  }

  virtual void TearDown() OVERRIDE {
    settings_service_.Shutdown();
  }

  base::DictionaryValue split_items_;
  scoped_ptr<base::Value> atomic_setting_value_;
  SupervisedUserSettingsService settings_service_;
  scoped_ptr<base::DictionaryValue> settings_;

  scoped_ptr<syncer::FakeSyncChangeProcessor> sync_processor_;
};

TEST_F(SupervisedUserSettingsServiceTest, ProcessAtomicSetting) {
  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(settings_);
  const base::Value* value = NULL;
  EXPECT_FALSE(settings_->GetWithoutPathExpansion(kSettingsName, &value));

  settings_.reset();
  syncer::SyncData data =
      SupervisedUserSettingsService::CreateSyncDataForSetting(
          kSettingsName, base::StringValue(kSettingsValue));
  syncer::SyncChangeList change_list;
  change_list.push_back(
      syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_ADD, data));
  syncer::SyncError error =
      settings_service_.ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet()) << error.ToString();
  ASSERT_TRUE(settings_);
  ASSERT_TRUE(settings_->GetWithoutPathExpansion(kSettingsName, &value));
  std::string string_value;
  EXPECT_TRUE(value->GetAsString(&string_value));
  EXPECT_EQ(kSettingsValue, string_value);
}

TEST_F(SupervisedUserSettingsServiceTest, ProcessSplitSetting) {
  StartSyncing(syncer::SyncDataList());
  ASSERT_TRUE(settings_);
  const base::Value* value = NULL;
  EXPECT_FALSE(settings_->GetWithoutPathExpansion(kSettingsName, &value));

  base::DictionaryValue dict;
  dict.SetString("foo", "bar");
  dict.SetBoolean("awesomesauce", true);
  dict.SetInteger("eaudecologne", 4711);

  settings_.reset();
  syncer::SyncChangeList change_list;
  for (base::DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    syncer::SyncData data =
        SupervisedUserSettingsService::CreateSyncDataForSetting(
            SupervisedUserSettingsService::MakeSplitSettingKey(kSettingsName,
                                                               it.key()),
            it.value());
    change_list.push_back(
        syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_ADD, data));
  }
  syncer::SyncError error =
      settings_service_.ProcessSyncChanges(FROM_HERE, change_list);
  EXPECT_FALSE(error.IsSet()) << error.ToString();
  ASSERT_TRUE(settings_);
  ASSERT_TRUE(settings_->GetWithoutPathExpansion(kSettingsName, &value));
  const base::DictionaryValue* dict_value = NULL;
  ASSERT_TRUE(value->GetAsDictionary(&dict_value));
  EXPECT_TRUE(dict_value->Equals(&dict));
}

TEST_F(SupervisedUserSettingsServiceTest, SetLocalSetting) {
  const base::Value* value = NULL;
  EXPECT_FALSE(settings_->GetWithoutPathExpansion(kSettingsName, &value));

  settings_.reset();
  settings_service_.SetLocalSettingForTesting(
      kSettingsName,
      scoped_ptr<base::Value>(new base::StringValue(kSettingsValue)));
  ASSERT_TRUE(settings_);
  ASSERT_TRUE(settings_->GetWithoutPathExpansion(kSettingsName, &value));
  std::string string_value;
  EXPECT_TRUE(value->GetAsString(&string_value));
  EXPECT_EQ(kSettingsValue, string_value);
}

TEST_F(SupervisedUserSettingsServiceTest, UploadItem) {
  UploadSplitItem("foo", "bar");
  UploadSplitItem("blurp", "baz");
  UploadAtomicItem("hurdle");

  // Uploading should produce changes when we start syncing.
  StartSyncing(syncer::SyncDataList());
  ASSERT_EQ(3u, sync_processor_->changes().size());
  for (syncer::SyncChangeList::const_iterator it =
           sync_processor_->changes().begin();
       it != sync_processor_->changes().end();
       ++it) {
    ASSERT_TRUE(it->IsValid());
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD, it->change_type());
    VerifySyncDataItem(it->sync_data());
  }

  // It should also show up in local Sync data.
  syncer::SyncDataList sync_data =
      settings_service_.GetAllSyncData(syncer::SUPERVISED_USER_SETTINGS);
  EXPECT_EQ(3u, sync_data.size());
  for (syncer::SyncDataList::const_iterator it = sync_data.begin();
       it != sync_data.end(); ++it) {
    VerifySyncDataItem(*it);
  }

  // Uploading after we have started syncing should work too.
  sync_processor_->changes().clear();
  UploadSplitItem("froodle", "narf");
  ASSERT_EQ(1u, sync_processor_->changes().size());
  syncer::SyncChange change = sync_processor_->changes()[0];
  ASSERT_TRUE(change.IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change.change_type());
  VerifySyncDataItem(change.sync_data());

  sync_data = settings_service_.GetAllSyncData(
      syncer::SUPERVISED_USER_SETTINGS);
  EXPECT_EQ(4u, sync_data.size());
  for (syncer::SyncDataList::const_iterator it = sync_data.begin();
       it != sync_data.end(); ++it) {
    VerifySyncDataItem(*it);
  }

  // Uploading an item with a previously seen key should create an UPDATE
  // action.
  sync_processor_->changes().clear();
  UploadSplitItem("blurp", "snarl");
  ASSERT_EQ(1u, sync_processor_->changes().size());
  change = sync_processor_->changes()[0];
  ASSERT_TRUE(change.IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change.change_type());
  VerifySyncDataItem(change.sync_data());

  sync_data = settings_service_.GetAllSyncData(
      syncer::SUPERVISED_USER_SETTINGS);
  EXPECT_EQ(4u, sync_data.size());
  for (syncer::SyncDataList::const_iterator it = sync_data.begin();
       it != sync_data.end(); ++it) {
    VerifySyncDataItem(*it);
  }

  sync_processor_->changes().clear();
  UploadAtomicItem("fjord");
  ASSERT_EQ(1u, sync_processor_->changes().size());
  change = sync_processor_->changes()[0];
  ASSERT_TRUE(change.IsValid());
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change.change_type());
  VerifySyncDataItem(change.sync_data());

  sync_data = settings_service_.GetAllSyncData(
      syncer::SUPERVISED_USER_SETTINGS);
  EXPECT_EQ(4u, sync_data.size());
  for (syncer::SyncDataList::const_iterator it = sync_data.begin();
       it != sync_data.end(); ++it) {
    VerifySyncDataItem(*it);
  }

  // The uploaded items should not show up as settings.
  const base::Value* value = NULL;
  EXPECT_FALSE(settings_->GetWithoutPathExpansion(kAtomicItemName, &value));
  EXPECT_FALSE(settings_->GetWithoutPathExpansion(kSplitItemName, &value));

  // Restarting sync should not create any new changes.
  settings_service_.StopSyncing(syncer::SUPERVISED_USER_SETTINGS);
  StartSyncing(sync_data);
  ASSERT_EQ(0u, sync_processor_->changes().size());
}
