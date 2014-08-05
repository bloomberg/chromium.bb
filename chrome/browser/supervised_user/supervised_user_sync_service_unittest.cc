// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/internal_api/public/attachments/attachment_service_proxy_for_test.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_image/default_user_images.h"
#endif

using sync_pb::ManagedUserSpecifics;
using syncer::SUPERVISED_USERS;
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

  virtual SyncDataList GetAllSyncData(syncer::ModelType type) const
      OVERRIDE {
    return SyncDataList();
  }

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

// Callback for SupervisedUserSyncService::GetSupervisedUsersAsync().
void GetSupervisedUsersCallback(const base::DictionaryValue** dict,
                                const base::DictionaryValue* supervised_users) {
  *dict = supervised_users;
}

}  // namespace

class SupervisedUserSyncServiceTest : public ::testing::Test {
 public:
  SupervisedUserSyncServiceTest();
  virtual ~SupervisedUserSyncServiceTest();

 protected:
  scoped_ptr<SyncChangeProcessor> CreateChangeProcessor();
  scoped_ptr<SyncErrorFactory> CreateErrorFactory();
  SyncData CreateRemoteData(const std::string& id,
                            const std::string& name,
                            const std::string& avatar);

  PrefService* prefs() { return profile_.GetPrefs(); }
  SupervisedUserSyncService* service() { return service_; }
  MockChangeProcessor* change_processor() { return change_processor_; }

 private:
  base::MessageLoop message_loop;
  TestingProfile profile_;
  SupervisedUserSyncService* service_;

  // Owned by the SupervisedUserSyncService.
  MockChangeProcessor* change_processor_;

  // A unique ID for creating "remote" Sync data.
  int64 sync_data_id_;
};

SupervisedUserSyncServiceTest::SupervisedUserSyncServiceTest()
    : change_processor_(NULL),
      sync_data_id_(0) {
  service_ = SupervisedUserSyncServiceFactory::GetForProfile(&profile_);
}

SupervisedUserSyncServiceTest::~SupervisedUserSyncServiceTest() {}

scoped_ptr<SyncChangeProcessor>
SupervisedUserSyncServiceTest::CreateChangeProcessor() {
  EXPECT_FALSE(change_processor_);
  change_processor_ = new MockChangeProcessor();
  return scoped_ptr<SyncChangeProcessor>(change_processor_);
}

scoped_ptr<SyncErrorFactory>
SupervisedUserSyncServiceTest::CreateErrorFactory() {
  return scoped_ptr<SyncErrorFactory>(new syncer::SyncErrorFactoryMock());
}

SyncData SupervisedUserSyncServiceTest::CreateRemoteData(
    const std::string& id,
    const std::string& name,
    const std::string& chrome_avatar) {
  ::sync_pb::EntitySpecifics specifics;
  specifics.mutable_managed_user()->set_id(id);
  specifics.mutable_managed_user()->set_name(name);
  specifics.mutable_managed_user()->set_acknowledged(true);
  if (!chrome_avatar.empty())
    specifics.mutable_managed_user()->set_chrome_avatar(chrome_avatar);

  return SyncData::CreateRemoteData(
      ++sync_data_id_,
      specifics,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create());
}

TEST_F(SupervisedUserSyncServiceTest, MergeEmpty) {
  SyncMergeResult result =
      service()->MergeDataAndStartSyncing(SUPERVISED_USERS,
                                          SyncDataList(),
                                          CreateChangeProcessor(),
                                          CreateErrorFactory());
  EXPECT_FALSE(result.error().IsSet());
  EXPECT_EQ(0, result.num_items_added());
  EXPECT_EQ(0, result.num_items_modified());
  EXPECT_EQ(0, result.num_items_deleted());
  EXPECT_EQ(0, result.num_items_before_association());
  EXPECT_EQ(0, result.num_items_after_association());
  EXPECT_EQ(0u, service()->GetSupervisedUsers()->size());
  EXPECT_EQ(0u, change_processor()->changes().size());

  service()->StopSyncing(SUPERVISED_USERS);
  service()->Shutdown();
}

TEST_F(SupervisedUserSyncServiceTest, MergeExisting) {
  const char kNameKey[] = "name";
  const char kAcknowledgedKey[] = "acknowledged";
  const char kChromeAvatarKey[] = "chromeAvatar";

  const char kUserId1[] = "aaaaa";
  const char kUserId2[] = "bbbbb";
  const char kUserId3[] = "ccccc";
  const char kUserId4[] = "ddddd";
  const char kName1[] = "Anchor";
  const char kName2[] = "Buzz";
  const char kName3[] = "Crush";
  const char kName4[] = "Dory";
  const char kAvatar1[] = "";
#if defined(OS_CHROMEOS)
  const char kAvatar2[] = "chromeos-avatar-index:0";
  const char kAvatar3[] = "chromeos-avatar-index:20";
#else
  const char kAvatar2[] = "chrome-avatar-index:0";
  const char kAvatar3[] = "chrome-avatar-index:20";
#endif
  const char kAvatar4[] = "";
  {
    DictionaryPrefUpdate update(prefs(), prefs::kSupervisedUsers);
    base::DictionaryValue* supervised_users = update.Get();
    base::DictionaryValue* dict = new base::DictionaryValue;
    dict->SetString(kNameKey, kName1);
    supervised_users->Set(kUserId1, dict);
    dict = new base::DictionaryValue;
    dict->SetString(kNameKey, kName2);
    dict->SetBoolean(kAcknowledgedKey, true);
    supervised_users->Set(kUserId2, dict);
  }

  const base::DictionaryValue* async_supervised_users = NULL;
  service()->GetSupervisedUsersAsync(
      base::Bind(&GetSupervisedUsersCallback, &async_supervised_users));

  SyncDataList initial_sync_data;
  initial_sync_data.push_back(CreateRemoteData(kUserId2, kName2, kAvatar2));
  initial_sync_data.push_back(CreateRemoteData(kUserId3, kName3, kAvatar3));
  initial_sync_data.push_back(CreateRemoteData(kUserId4, kName4, kAvatar4));

  SyncMergeResult result =
      service()->MergeDataAndStartSyncing(SUPERVISED_USERS,
                                          initial_sync_data,
                                          CreateChangeProcessor(),
                                          CreateErrorFactory());
  EXPECT_FALSE(result.error().IsSet());
  EXPECT_EQ(2, result.num_items_added());
  EXPECT_EQ(1, result.num_items_modified());
  EXPECT_EQ(0, result.num_items_deleted());
  EXPECT_EQ(2, result.num_items_before_association());
  EXPECT_EQ(4, result.num_items_after_association());

  const base::DictionaryValue* supervised_users =
      service()->GetSupervisedUsers();
  EXPECT_EQ(4u, supervised_users->size());
  EXPECT_TRUE(async_supervised_users);
  EXPECT_TRUE(supervised_users->Equals(async_supervised_users));

  {
    const base::DictionaryValue* supervised_user = NULL;
    ASSERT_TRUE(supervised_users->GetDictionary(kUserId2, &supervised_user));
    ASSERT_TRUE(supervised_user);
    std::string name;
    EXPECT_TRUE(supervised_user->GetString(kNameKey, &name));
    EXPECT_EQ(kName2, name);
    bool acknowledged = false;
    EXPECT_TRUE(supervised_user->GetBoolean(kAcknowledgedKey, &acknowledged));
    EXPECT_TRUE(acknowledged);
    std::string avatar;
    EXPECT_TRUE(supervised_user->GetString(kChromeAvatarKey, &avatar));
    EXPECT_EQ(kAvatar2, avatar);
  }
  {
    const base::DictionaryValue* supervised_user = NULL;
    ASSERT_TRUE(supervised_users->GetDictionary(kUserId3, &supervised_user));
    ASSERT_TRUE(supervised_user);
    std::string name;
    EXPECT_TRUE(supervised_user->GetString(kNameKey, &name));
    EXPECT_EQ(kName3, name);
    bool acknowledged = false;
    EXPECT_TRUE(supervised_user->GetBoolean(kAcknowledgedKey, &acknowledged));
    EXPECT_TRUE(acknowledged);
    std::string avatar;
    EXPECT_TRUE(supervised_user->GetString(kChromeAvatarKey, &avatar));
    EXPECT_EQ(kAvatar3, avatar);
  }
  {
    const base::DictionaryValue* supervised_user = NULL;
    ASSERT_TRUE(supervised_users->GetDictionary(kUserId4, &supervised_user));
    ASSERT_TRUE(supervised_user);
    std::string name;
    EXPECT_TRUE(supervised_user->GetString(kNameKey, &name));
    EXPECT_EQ(kName4, name);
    bool acknowledged = false;
    EXPECT_TRUE(supervised_user->GetBoolean(kAcknowledgedKey, &acknowledged));
    EXPECT_TRUE(acknowledged);
    std::string avatar;
    EXPECT_TRUE(supervised_user->GetString(kChromeAvatarKey, &avatar));
    EXPECT_EQ(kAvatar4, avatar);
  }

  EXPECT_EQ(1u, change_processor()->changes().size());
  {
    SyncChange change = change_processor()->GetChange(kUserId1);
    ASSERT_TRUE(change.IsValid());
    EXPECT_EQ(SyncChange::ACTION_ADD, change.change_type());
    const ManagedUserSpecifics& supervised_user =
        change.sync_data().GetSpecifics().managed_user();
    EXPECT_EQ(kName1, supervised_user.name());
    EXPECT_FALSE(supervised_user.acknowledged());
    EXPECT_EQ(kAvatar1, supervised_user.chrome_avatar());
  }
}

TEST_F(SupervisedUserSyncServiceTest, GetAvatarIndex) {
  int avatar = 100;
  EXPECT_TRUE(SupervisedUserSyncService::GetAvatarIndex(std::string(),
                                                        &avatar));
  EXPECT_EQ(SupervisedUserSyncService::kNoAvatar, avatar);

  int avatar_index = 4;
#if defined(OS_CHROMEOS)
  avatar_index += user_manager::kFirstDefaultImageIndex;
#endif
  std::string avatar_str =
      SupervisedUserSyncService::BuildAvatarString(avatar_index);
#if defined(OS_CHROMEOS)
  EXPECT_EQ(base::StringPrintf("chromeos-avatar-index:%d", avatar_index),
            avatar_str);
#else
  EXPECT_EQ(base::StringPrintf("chrome-avatar-index:%d", avatar_index),
            avatar_str);
#endif
  EXPECT_TRUE(SupervisedUserSyncService::GetAvatarIndex(avatar_str, &avatar));
  EXPECT_EQ(avatar_index, avatar);

  avatar_index = 0;
#if defined(OS_CHROMEOS)
  avatar_index += user_manager::kFirstDefaultImageIndex;
#endif
  avatar_str = SupervisedUserSyncService::BuildAvatarString(avatar_index);
#if defined(OS_CHROMEOS)
  EXPECT_EQ(base::StringPrintf("chromeos-avatar-index:%d", avatar_index),
            avatar_str);
#else
  EXPECT_EQ(base::StringPrintf("chrome-avatar-index:%d", avatar_index),
            avatar_str);
#endif
  EXPECT_TRUE(SupervisedUserSyncService::GetAvatarIndex(avatar_str, &avatar));
  EXPECT_EQ(avatar_index, avatar);

  EXPECT_FALSE(SupervisedUserSyncService::GetAvatarIndex("wrong-prefix:5",
                                                         &avatar));
#if defined(OS_CHROMEOS)
  EXPECT_FALSE(SupervisedUserSyncService::GetAvatarIndex(
      "chromeos-avatar-indes:2",
      &avatar));

  EXPECT_FALSE(
      SupervisedUserSyncService::GetAvatarIndex("chromeos-avatar-indexxx:2",
                                                &avatar));

  EXPECT_FALSE(SupervisedUserSyncService::GetAvatarIndex(
      "chromeos-avatar-index:",
      &avatar));

  EXPECT_FALSE(SupervisedUserSyncService::GetAvatarIndex(
      "chromeos-avatar-index:x",
      &avatar));

  EXPECT_FALSE(SupervisedUserSyncService::GetAvatarIndex(
      "chrome-avatar-index:5",
      &avatar));
#else
  EXPECT_FALSE(SupervisedUserSyncService::GetAvatarIndex(
      "chrome-avatar-indes:2",
      &avatar));

  EXPECT_FALSE(SupervisedUserSyncService::GetAvatarIndex(
      "chrome-avatar-indexxx:2",
      &avatar));

  EXPECT_FALSE(SupervisedUserSyncService::GetAvatarIndex(
      "chrome-avatar-index:",
      &avatar));

  EXPECT_FALSE(SupervisedUserSyncService::GetAvatarIndex(
      "chrome-avatar-index:x",
      &avatar));

  EXPECT_FALSE(SupervisedUserSyncService::GetAvatarIndex(
      "chromeos-avatar-index:5",
      &avatar));
#endif
}
