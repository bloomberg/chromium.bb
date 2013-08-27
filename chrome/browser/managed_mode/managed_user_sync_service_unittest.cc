// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"
#include "chrome/browser/managed_mode/managed_user_sync_service_factory.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::ManagedUserSpecifics;
using syncer::MANAGED_USERS;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncErrorFactory;
using syncer::SyncMergeResult;

namespace {

class MockChangeProcessor : public SyncChangeProcessor {
 public:
  MockChangeProcessor() {}
  virtual ~MockChangeProcessor() {}

  // SyncChangeProcessor implementation:
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE;

  const SyncChangeList& changes() const { return change_list_; }
  SyncChange GetChange(const std::string& id) const;

 private:
  SyncChangeList change_list_;
};

SyncError MockChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const SyncChangeList& change_list) {
  change_list_ = change_list;
  return SyncError();
}

SyncChange MockChangeProcessor::GetChange(const std::string& id) const {
  for (SyncChangeList::const_iterator it = change_list_.begin();
       it != change_list_.end(); ++it) {
    if (it->sync_data().GetSpecifics().managed_user().id() == id)
      return *it;
  }
  return SyncChange();
}

}  // namespace

class ManagedUserSyncServiceTest : public ::testing::Test {
 public:
  ManagedUserSyncServiceTest();
  virtual ~ManagedUserSyncServiceTest();

 protected:
  scoped_ptr<SyncChangeProcessor> CreateChangeProcessor();
  scoped_ptr<SyncErrorFactory> CreateErrorFactory();
  SyncData CreateRemoteData(const std::string& id, const std::string& name);

  SyncMergeResult StartInitialSync();

  void Acknowledge();
  void ResetService();

  PrefService* prefs() { return profile_.GetPrefs(); }
  ManagedUserSyncService* service() { return service_; }
  MockChangeProcessor* change_processor() { return change_processor_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  ManagedUserSyncService* service_;

  // Owned by the ManagedUserSyncService.
  MockChangeProcessor* change_processor_;

  // A unique ID for creating "remote" Sync data.
  int64 sync_data_id_;
};

ManagedUserSyncServiceTest::ManagedUserSyncServiceTest()
    : change_processor_(NULL),
      sync_data_id_(0) {
  service_ = ManagedUserSyncServiceFactory::GetForProfile(&profile_);
}

ManagedUserSyncServiceTest::~ManagedUserSyncServiceTest() {}

scoped_ptr<SyncChangeProcessor>
ManagedUserSyncServiceTest::CreateChangeProcessor() {
  EXPECT_FALSE(change_processor_);
  change_processor_ = new MockChangeProcessor();
  return scoped_ptr<SyncChangeProcessor>(change_processor_);
}

scoped_ptr<SyncErrorFactory>
ManagedUserSyncServiceTest::CreateErrorFactory() {
  return scoped_ptr<SyncErrorFactory>(new syncer::SyncErrorFactoryMock());
}

SyncData ManagedUserSyncServiceTest::CreateRemoteData(
    const std::string& id,
    const std::string& name) {
  ::sync_pb::EntitySpecifics specifics;
  specifics.mutable_managed_user()->set_id(id);
  specifics.mutable_managed_user()->set_name(name);
  specifics.mutable_managed_user()->set_acknowledged(true);
  return SyncData::CreateRemoteData(++sync_data_id_, specifics, base::Time());
}

SyncMergeResult ManagedUserSyncServiceTest::StartInitialSync() {
  SyncDataList initial_sync_data;
  SyncMergeResult result =
      service()->MergeDataAndStartSyncing(MANAGED_USERS,
                                          initial_sync_data,
                                          CreateChangeProcessor(),
                                          CreateErrorFactory());
  EXPECT_FALSE(result.error().IsSet());
  return result;
}

void ManagedUserSyncServiceTest::ResetService() {
  service_->StopSyncing(MANAGED_USERS);
  service_->Shutdown();
}

TEST_F(ManagedUserSyncServiceTest, MergeEmpty) {
  SyncMergeResult result = StartInitialSync();
  EXPECT_EQ(0, result.num_items_added());
  EXPECT_EQ(0, result.num_items_modified());
  EXPECT_EQ(0, result.num_items_deleted());
  EXPECT_EQ(0, result.num_items_before_association());
  EXPECT_EQ(0, result.num_items_after_association());
  EXPECT_EQ(0u, prefs()->GetDictionary(prefs::kManagedUsers)->size());
  EXPECT_EQ(0u, change_processor()->changes().size());

  ResetService();
}

TEST_F(ManagedUserSyncServiceTest, MergeExisting) {
  const char kNameKey[] = "name";
  const char kAcknowledgedKey[] = "acknowledged";

  const char kUserId1[] = "aaaaa";
  const char kUserId2[] = "bbbbb";
  const char kUserId3[] = "ccccc";
  const char kUserId4[] = "ddddd";
  const char kName1[] = "Anchor";
  const char kName2[] = "Buzz";
  const char kName3[] = "Crush";
  const char kName4[] = "Dory";
  {
    DictionaryPrefUpdate update(prefs(), prefs::kManagedUsers);
    DictionaryValue* managed_users = update.Get();
    DictionaryValue* dict = new DictionaryValue;
    dict->SetString(kNameKey, kName1);
    managed_users->Set(kUserId1, dict);
    dict = new DictionaryValue;
    dict->SetString(kNameKey, kName2);
    dict->SetBoolean(kAcknowledgedKey, true);
    managed_users->Set(kUserId2, dict);
  }

  SyncDataList initial_sync_data;
  initial_sync_data.push_back(CreateRemoteData(kUserId2, kName2));
  initial_sync_data.push_back(CreateRemoteData(kUserId3, kName3));
  initial_sync_data.push_back(CreateRemoteData(kUserId4, kName4));

  SyncMergeResult result =
      service()->MergeDataAndStartSyncing(MANAGED_USERS,
                                          initial_sync_data,
                                          CreateChangeProcessor(),
                                          CreateErrorFactory());
  EXPECT_FALSE(result.error().IsSet());
  EXPECT_EQ(2, result.num_items_added());
  EXPECT_EQ(1, result.num_items_modified());
  EXPECT_EQ(0, result.num_items_deleted());
  EXPECT_EQ(2, result.num_items_before_association());
  EXPECT_EQ(4, result.num_items_after_association());

  const DictionaryValue* managed_users =
      prefs()->GetDictionary(prefs::kManagedUsers);
  EXPECT_EQ(4u, managed_users->size());
  {
    const DictionaryValue* managed_user = NULL;
    ASSERT_TRUE(managed_users->GetDictionary(kUserId2, &managed_user));
    ASSERT_TRUE(managed_user);
    std::string name;
    EXPECT_TRUE(managed_user->GetString(kNameKey, &name));
    EXPECT_EQ(kName2, name);
    bool acknowledged = false;
    EXPECT_TRUE(managed_user->GetBoolean(kAcknowledgedKey, &acknowledged));
    EXPECT_TRUE(acknowledged);
  }
  {
    const DictionaryValue* managed_user = NULL;
    ASSERT_TRUE(managed_users->GetDictionary(kUserId3, &managed_user));
    ASSERT_TRUE(managed_user);
    std::string name;
    EXPECT_TRUE(managed_user->GetString(kNameKey, &name));
    EXPECT_EQ(kName3, name);
    bool acknowledged = false;
    EXPECT_TRUE(managed_user->GetBoolean(kAcknowledgedKey, &acknowledged));
    EXPECT_TRUE(acknowledged);
  }
  {
    const DictionaryValue* managed_user = NULL;
    ASSERT_TRUE(managed_users->GetDictionary(kUserId4, &managed_user));
    ASSERT_TRUE(managed_user);
    std::string name;
    EXPECT_TRUE(managed_user->GetString(kNameKey, &name));
    EXPECT_EQ(kName4, name);
    bool acknowledged = false;
    EXPECT_TRUE(managed_user->GetBoolean(kAcknowledgedKey, &acknowledged));
    EXPECT_TRUE(acknowledged);
  }

  EXPECT_EQ(1u, change_processor()->changes().size());
  {
    SyncChange change = change_processor()->GetChange(kUserId1);
    ASSERT_TRUE(change.IsValid());
    EXPECT_EQ(SyncChange::ACTION_ADD, change.change_type());
    const ManagedUserSpecifics& managed_user =
        change.sync_data().GetSpecifics().managed_user();
    EXPECT_EQ(kName1, managed_user.name());
    EXPECT_FALSE(managed_user.acknowledged());
  }
}
