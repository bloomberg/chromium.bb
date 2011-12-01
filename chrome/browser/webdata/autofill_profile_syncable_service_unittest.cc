// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/sync/internal_api/read_node_mock.h"
#include "chrome/browser/sync/internal_api/syncapi_mock.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_mock.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_profile_syncable_service.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using ::testing::Property;
using content::BrowserThread;
class AutofillProfile;

class MockAutofillProfileSyncableService
    : public AutofillProfileSyncableService {
 public:
  MockAutofillProfileSyncableService() {
  }
  virtual ~MockAutofillProfileSyncableService() {}

  MOCK_METHOD1(LoadAutofillData, bool(std::vector<AutofillProfile*>*));
  MOCK_METHOD1(SaveChangesToWebData,
               bool(const AutofillProfileSyncableService::DataBundle&));
};

ACTION_P(CopyData, data) {
  arg0->resize(data->size());
  std::copy(data->begin(), data->end(), arg0->begin());
}

MATCHER_P(CheckSyncChanges, n_sync_changes_list, "") {
  if (arg.size() != n_sync_changes_list.size())
    return false;
  SyncChangeList::const_iterator passed, expected;
  for (passed = arg.begin(), expected = n_sync_changes_list.begin();
       passed != arg.end() && expected != n_sync_changes_list.end();
       ++passed, ++expected) {
    DCHECK(passed->IsValid());
    if (passed->change_type() != expected->change_type())
      return false;
    if (passed->sync_data().GetSpecifics().GetExtension(
            sync_pb::autofill_profile).guid() !=
        expected->sync_data().GetSpecifics().GetExtension(
            sync_pb::autofill_profile).guid()) {
      return false;
    }
  }
  return true;
}

MATCHER_P(DataBundleCheck, n_bundle, "") {
  if ((arg.profiles_to_delete.size() != n_bundle.profiles_to_delete.size()) ||
      (arg.profiles_to_update.size() != n_bundle.profiles_to_update.size()) ||
      (arg.profiles_to_add.size() != n_bundle.profiles_to_add.size())) {
    return false;
  }
  for (size_t i = 0; i < arg.profiles_to_delete.size(); ++i) {
    if (arg.profiles_to_delete[i] != n_bundle.profiles_to_delete[i])
      return false;
  }
  for (size_t i = 0; i < arg.profiles_to_update.size(); ++i) {
    if (arg.profiles_to_update[i]->guid() !=
        n_bundle.profiles_to_update[i]->guid()) {
      return false;
    }
  }
  for (size_t i = 0; i < arg.profiles_to_add.size(); ++i) {
    if (arg.profiles_to_add[i]->Compare(*n_bundle.profiles_to_add[i]))
      return false;
  }
  return true;
}

class MockSyncChangeProcessor : public SyncChangeProcessor {
 public:
  MockSyncChangeProcessor() {}
  virtual ~MockSyncChangeProcessor() {}

  MOCK_METHOD2(ProcessSyncChanges,
               SyncError(const tracked_objects::Location&,
                         const SyncChangeList&));
};

class AutofillProfileSyncableServiceTest : public testing::Test {
 public:
  AutofillProfileSyncableServiceTest()
    : db_thread_(BrowserThread::DB, &message_loop_) {}

  virtual void SetUp() OVERRIDE {
    sync_processor_.reset(new MockSyncChangeProcessor);
  }

  virtual void TearDown() OVERRIDE {
    // Each test passes ownership of the sync processor to the SyncableService.
    // We don't release it immediately so we can verify the mock calls, so
    // release it at teardown. Any test that doesn't call
    // MergeDataAndStartSyncing or set_sync_processor must ensure the
    // sync_processor_ gets properly reset.
    ignore_result(sync_processor_.release());
  }
 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread db_thread_;
  MockAutofillProfileSyncableService autofill_syncable_service_;
  scoped_ptr<MockSyncChangeProcessor> sync_processor_;
};

TEST_F(AutofillProfileSyncableServiceTest, MergeDataAndStartSyncing) {
  std::vector<AutofillProfile *> profiles_from_web_db;
  std::string guid_present1 = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
  std::string guid_present2 = "EDC609ED-7EEE-4F27-B00C-423242A9C44C";
  std::string guid_synced1 = "EDC609ED-7EEE-4F27-B00C-423242A9C44D";
  std::string guid_synced2 = "EDC609ED-7EEE-4F27-B00C-423242A9C44E";

  profiles_from_web_db.push_back(new AutofillProfile(guid_present1));
  profiles_from_web_db.back()->SetInfo(NAME_FIRST, UTF8ToUTF16("John"));
  profiles_from_web_db.push_back(new AutofillProfile(guid_present2));
  profiles_from_web_db.back()->SetInfo(NAME_FIRST, UTF8ToUTF16("Tom"));

  SyncDataList data_list;
  AutofillProfile profile1(guid_synced1);
  profile1.SetInfo(NAME_FIRST, UTF8ToUTF16("Jane"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile1));
  AutofillProfile profile2(guid_synced2);
  profile2.SetInfo(NAME_FIRST, UTF8ToUTF16("Harry"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile2));
  // This one will have the name updated.
  AutofillProfile profile3(guid_present2);
  profile3.SetInfo(NAME_FIRST, UTF8ToUTF16("Tom Doe"));
  data_list.push_back(autofill_syncable_service_.CreateData(profile3));

  SyncChangeList expected_change_list;
  expected_change_list.push_back(
      SyncChange(SyncChange::ACTION_ADD,
                 AutofillProfileSyncableService::CreateData(
                     (*profiles_from_web_db.front()))));

  AutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_add.push_back(&profile1);
  expected_bundle.profiles_to_add.push_back(&profile2);
  expected_bundle.profiles_to_update.push_back(&profile3);

  EXPECT_CALL(autofill_syncable_service_, LoadAutofillData(_))
      .Times(1)
      .WillOnce(DoAll(CopyData(&profiles_from_web_db), Return(true)));
  EXPECT_CALL(autofill_syncable_service_,
              SaveChangesToWebData(DataBundleCheck(expected_bundle)))
      .Times(1)
      .WillOnce(Return(true));
  ON_CALL(*sync_processor_, ProcessSyncChanges(_, _))
      .WillByDefault(Return(SyncError()));
  EXPECT_CALL(*sync_processor_,
              ProcessSyncChanges(_, CheckSyncChanges(expected_change_list)))
      .Times(1)
      .WillOnce(Return(SyncError()));

  // Takes ownership of sync_processor_.
  autofill_syncable_service_.MergeDataAndStartSyncing(
      syncable::AUTOFILL_PROFILE, data_list, sync_processor_.get());
  autofill_syncable_service_.StopSyncing(syncable::AUTOFILL_PROFILE);
}

TEST_F(AutofillProfileSyncableServiceTest, GetAllSyncData) {
  std::vector<AutofillProfile *> profiles_from_web_db;
  std::string guid_present1 = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
  std::string guid_present2 = "EDC609ED-7EEE-4F27-B00C-423242A9C44C";

  profiles_from_web_db.push_back(new AutofillProfile(guid_present1));
  profiles_from_web_db.back()->SetInfo(NAME_FIRST, UTF8ToUTF16("John"));
  profiles_from_web_db.push_back(new AutofillProfile(guid_present2));
  profiles_from_web_db.back()->SetInfo(NAME_FIRST, UTF8ToUTF16("Jane"));

  EXPECT_CALL(autofill_syncable_service_, LoadAutofillData(_))
      .Times(1)
      .WillOnce(DoAll(CopyData(&profiles_from_web_db), Return(true)));
  EXPECT_CALL(autofill_syncable_service_, SaveChangesToWebData(_))
      .Times(1)
      .WillOnce(Return(true));
  ON_CALL(*sync_processor_, ProcessSyncChanges(_, _))
      .WillByDefault(Return(SyncError()));
  EXPECT_CALL(*sync_processor_,
              ProcessSyncChanges(_, Property(&SyncChangeList::size, Eq(2U))))
      .Times(1)
      .WillOnce(Return(SyncError()));

  SyncDataList data_list;
  // Takes ownership of sync_processor_.
  autofill_syncable_service_.MergeDataAndStartSyncing(
      syncable::AUTOFILL_PROFILE, data_list, sync_processor_.get());

  SyncDataList data =
      autofill_syncable_service_.GetAllSyncData(syncable::AUTOFILL_PROFILE);

  EXPECT_EQ(2U, data.size());
  EXPECT_EQ(guid_present1, data.front().GetSpecifics()
                               .GetExtension(sync_pb::autofill_profile).guid());
  EXPECT_EQ(guid_present2, data.back().GetSpecifics()
                               .GetExtension(sync_pb::autofill_profile).guid());
}

TEST_F(AutofillProfileSyncableServiceTest, ProcessSyncChanges) {
  std::vector<AutofillProfile *> profiles_from_web_db;
  std::string guid_present = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
  std::string guid_synced = "EDC609ED-7EEE-4F27-B00C-423242A9C44C";

  SyncChangeList change_list;
  AutofillProfile profile(guid_synced);
  profile.SetInfo(NAME_FIRST, UTF8ToUTF16("Jane"));
  change_list.push_back(
      SyncChange(SyncChange::ACTION_ADD,
      AutofillProfileSyncableService::CreateData(profile)));
  AutofillProfile empty_profile(guid_present);
  change_list.push_back(
      SyncChange(SyncChange::ACTION_DELETE,
                 AutofillProfileSyncableService::CreateData(empty_profile)));

  AutofillProfileSyncableService::DataBundle expected_bundle;
  expected_bundle.profiles_to_delete.push_back(guid_present);
  expected_bundle.profiles_to_add.push_back(&profile);

  EXPECT_CALL(autofill_syncable_service_, SaveChangesToWebData(
              DataBundleCheck(expected_bundle)))
      .Times(1)
      .WillOnce(Return(true));

  autofill_syncable_service_.set_sync_processor(sync_processor_.get());
  SyncError error = autofill_syncable_service_.ProcessSyncChanges(
      FROM_HERE, change_list);

  EXPECT_FALSE(error.IsSet());
}

TEST_F(AutofillProfileSyncableServiceTest, ActOnChange) {
  std::string guid1 = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
  std::string guid2 = "EDC609ED-7EEE-4F27-B00C-423242A9C44C";

  AutofillProfile profile(guid1);
  profile.SetInfo(NAME_FIRST, UTF8ToUTF16("Jane"));
  AutofillProfileChange change1(AutofillProfileChange::ADD, guid1, &profile);
  AutofillProfileChange change2(AutofillProfileChange::REMOVE, guid2, NULL);
  ON_CALL(*sync_processor_, ProcessSyncChanges(_, _))
      .WillByDefault(Return(SyncError(FROM_HERE, std::string("an error"),
                                      syncable::AUTOFILL_PROFILE)));

  autofill_syncable_service_.set_sync_processor(sync_processor_.get());
  autofill_syncable_service_.ActOnChange(change1);
  autofill_syncable_service_.ActOnChange(change2);
}
