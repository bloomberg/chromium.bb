// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/synced_notification_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_pb::SyncedNotificationSpecifics;
using sync_pb::EntitySpecifics;
using syncer::SyncData;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncDataList;
using syncer::SYNCED_NOTIFICATIONS;
using notifier::SyncedNotification;
using notifier::ChromeNotifierService;

namespace {

const char kAppId1[] = "fboilmbenheemaomgaeehigklolhkhnf";
const char kAppId2[] = "fbcmoldooppoahjhfflnmljoanccekpf";
const char kAppId3[] = "fbcmoldooppoahjhfflnmljoanccek33";
const char kAppId4[] = "fbcmoldooppoahjhfflnmljoanccek44";
const char kAppId5[] = "fbcmoldooppoahjhfflnmljoanccek55";
const char kAppId6[] = "fbcmoldooppoahjhfflnmljoanccek66";
const char kAppId7[] = "fbcmoldooppoahjhfflnmljoanccek77";
const char kKey1[] = "foo";
const char kKey2[] = "bar";
const char kKey3[] = "bat";
const char kKey4[] = "baz";
const char kKey5[] = "foobar";
const char kKey6[] = "fu";
const char kKey7[] = "meta";
const int64 kFakeCreationTime = 42;
const sync_pb::CoalescedSyncedNotification_ReadState kDismissed =
    sync_pb::CoalescedSyncedNotification_ReadState_DISMISSED;
const sync_pb::CoalescedSyncedNotification_ReadState kUnread =
    sync_pb::CoalescedSyncedNotification_ReadState_UNREAD;

// Extract notification id from syncer::SyncData.
std::string GetNotificationId(const SyncData& sync_data) {
  SyncedNotificationSpecifics specifics = sync_data.GetSpecifics().
      synced_notification();
  return specifics.coalesced_notification().key();
}

// Stub out the NotificationUIManager for unit testing.
class StubNotificationUIManager : public NotificationUIManager {
 public:
  StubNotificationUIManager() {}
  virtual ~StubNotificationUIManager() {}

  // Adds a notification to be displayed. Virtual for unit test override.
  virtual void Add(const Notification& notification, Profile* profile)
      OVERRIDE {}

  // Returns true if any notifications match the supplied ID, either currently
  // displayed or in the queue.
  virtual bool DoesIdExist(const std::string& id) OVERRIDE {
    return true;
  }

  // Removes any notifications matching the supplied ID, either currently
  // displayed or in the queue.  Returns true if anything was removed.
  virtual bool CancelById(const std::string& notification_id) OVERRIDE {
    return false;
  }

  // Removes notifications matching the |source_origin| (which could be an
  // extension ID). Returns true if anything was removed.
  virtual bool CancelAllBySourceOrigin(const GURL& source_origin) OVERRIDE {
    return false;
  }

  // Removes notifications matching |profile|. Returns true if any were removed.
  virtual bool CancelAllByProfile(Profile* profile) OVERRIDE {
    return false;
  }

  // Cancels all pending notifications and closes anything currently showing.
  // Used when the app is terminating.
  virtual void CancelAll() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(StubNotificationUIManager);
};

// Dummy SyncChangeProcessor used to help review what SyncChanges are pushed
// back up to Sync.
class TestChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  TestChangeProcessor() { }
  virtual ~TestChangeProcessor() { }

  // Store a copy of all the changes passed in so we can examine them later.
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE {
    change_map_.clear();
    for (SyncChangeList::const_iterator iter = change_list.begin();
        iter != change_list.end(); ++iter) {
      // Put the data into the change tracking map.
      change_map_[GetNotificationId(iter->sync_data())] = *iter;
    }

    return syncer::SyncError();
  }

  size_t change_list_size() { return change_map_.size(); }

  bool ContainsId(const std::string& id) {
    return change_map_.find(id) != change_map_.end();
  }

  SyncChange GetChangeById(const std::string& id) {
    EXPECT_TRUE(ContainsId(id));
    return change_map_[id];
  }

 private:
  // Track the changes received in ProcessSyncChanges.
  std::map<std::string, SyncChange> change_map_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeProcessor);
};

class SyncChangeProcessorDelegate : public syncer::SyncChangeProcessor {
 public:
  explicit SyncChangeProcessorDelegate(SyncChangeProcessor* recipient)
      : recipient_(recipient) {
    EXPECT_TRUE(recipient_);
  }
  virtual ~SyncChangeProcessorDelegate() {}

  // syncer::SyncChangeProcessor implementation.
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE {
    return recipient_->ProcessSyncChanges(from_here, change_list);
  }

 private:
  // The recipient of all sync changes.
  SyncChangeProcessor* recipient_;

  DISALLOW_COPY_AND_ASSIGN(SyncChangeProcessorDelegate);
};

}  // namespace

class ChromeNotifierServiceTest : public testing::Test {
 public:
  ChromeNotifierServiceTest()
      : sync_processor_(new TestChangeProcessor),
        sync_processor_delegate_(new SyncChangeProcessorDelegate(
            sync_processor_.get())) {}
  virtual ~ChromeNotifierServiceTest() {}

  // Methods from testing::Test.
  virtual void SetUp() {}
  virtual void TearDown() {}

  TestChangeProcessor* processor() {
    return static_cast<TestChangeProcessor*>(sync_processor_.get());
}

  scoped_ptr<syncer::SyncChangeProcessor> PassProcessor() {
    return sync_processor_delegate_.Pass();
  }

  SyncedNotification* CreateNotification(
      const std::string& message,
      const std::string& app_id,
      const std::string& key,
      const std::string& external_id,
      sync_pb::CoalescedSyncedNotification_ReadState read_state) {
    SyncData sync_data = CreateSyncData(message, app_id, key,
                                        external_id, read_state);
    // Set enough fields in sync_data, including specifics, for our tests
    // to pass.
    return new SyncedNotification(sync_data);
  }

  // Helper to create syncer::SyncChange.
  static SyncChange CreateSyncChange(
      SyncChange::SyncChangeType type,
      SyncedNotification* notification) {
    // Take control of the notification to clean it up after we create data
    // out of it.
    scoped_ptr<SyncedNotification> scoped_notification(notification);
    return SyncChange(
        FROM_HERE,
        type,
        ChromeNotifierService::CreateSyncDataFromNotification(*notification));
  }

  // Helper to create syncer::SyncData.
  static SyncData CreateSyncData(
      const std::string& message,
      const std::string& app_id,
      const std::string& key,
      const std::string& external_id,
      sync_pb::CoalescedSyncedNotification_ReadState read_state) {
    // CreateLocalData makes a copy of this, so this can safely live
    // on the stack.
    EntitySpecifics entity_specifics;

    SyncedNotificationSpecifics* specifics =
        entity_specifics.mutable_synced_notification();

    specifics->mutable_coalesced_notification()->
        set_app_id(app_id);

    specifics->mutable_coalesced_notification()->
        set_key(key);

    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        mutable_simple_expanded_layout()->
        set_title(message);

    specifics->
        mutable_coalesced_notification()->
        set_creation_time_msec(kFakeCreationTime);

    specifics->
        mutable_coalesced_notification()->
        add_notification();

    specifics->
        mutable_coalesced_notification()->
        mutable_notification(0)->set_external_id(external_id);

    specifics->
        mutable_coalesced_notification()->
        set_read_state(read_state);

    SyncData sync_data = SyncData::CreateLocalData(
        "syncer::SYNCED_NOTIFICATIONS",
        "ChromeNotifierServiceUnitTest",
        entity_specifics);

    return sync_data;
  }

 private:
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNotifierServiceTest);
};

// TODO(petewil): Add more tests as I add functionalty.  Tests are based on
// chrome/browser/extensions/app_notification_manager_sync_unittest.cc

// Create a Notification, convert it to SyncData and convert it back.
TEST_F(ChromeNotifierServiceTest, NotificationToSyncDataToNotification) {
  scoped_ptr<SyncedNotification> notification1(
      CreateNotification("1", kAppId1, kKey1, "11", kUnread));
  SyncData sync_data =
      ChromeNotifierService::CreateSyncDataFromNotification(*notification1);
  scoped_ptr<SyncedNotification> notification2(
      ChromeNotifierService::CreateNotificationFromSyncData(sync_data));
  EXPECT_TRUE(notification2.get());
  EXPECT_TRUE(notification1->EqualsIgnoringReadState(*notification2));
  EXPECT_EQ(notification1->read_state(), notification2->read_state());
}

// Model assocation:  We have no local data, and no remote data.
TEST_F(ChromeNotifierServiceTest, ModelAssocBothEmpty) {
  StubNotificationUIManager notification_manager;
  ChromeNotifierService notifier(NULL, &notification_manager);

  notifier.MergeDataAndStartSyncing(
      SYNCED_NOTIFICATIONS,
      SyncDataList(),  // Empty.
      PassProcessor(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock()));

  EXPECT_EQ(0U, notifier.GetAllSyncData(SYNCED_NOTIFICATIONS).size());
  EXPECT_EQ(0U, processor()->change_list_size());
}

// Process sync changes when there is no local data.
TEST_F(ChromeNotifierServiceTest, ProcessSyncChangesEmptyModel) {
  // We initially have no data.
  StubNotificationUIManager notification_manager;
  ChromeNotifierService notifier(NULL, &notification_manager);

  notifier.MergeDataAndStartSyncing(
      SYNCED_NOTIFICATIONS,
      SyncDataList(),
      PassProcessor(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock()));

  // Set up a bunch of ADDs.
  SyncChangeList changes;
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(
          "1", kAppId1, kKey1, "11", kUnread)));
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(
          "2", kAppId2, kKey2, "22", kUnread)));
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(
          "3", kAppId3, kKey3, "33", kUnread)));

  notifier.ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_EQ(3U, notifier.GetAllSyncData(SYNCED_NOTIFICATIONS).size());
  // TODO(petewil): verify that the list entries have expected values to make
  // this test more robust.
}

// Model has some notifications, some of them are local only. Sync has some
// notifications. No items match up.
TEST_F(ChromeNotifierServiceTest, LocalRemoteBothNonEmptyNoOverlap) {
  StubNotificationUIManager notification_manager;
  ChromeNotifierService notifier(NULL, &notification_manager);

  // Create some local fake data.
  scoped_ptr<SyncedNotification> n1(CreateNotification(
      "1", kAppId1, kKey1, "11", kUnread));
  notifier.AddForTest(n1.Pass());
  scoped_ptr<SyncedNotification> n2(CreateNotification(
      "2", kAppId2, kKey2, "22", kUnread));
  notifier.AddForTest(n2.Pass());
  scoped_ptr<SyncedNotification> n3(CreateNotification(
      "3", kAppId3, kKey3, "33", kUnread));
  notifier.AddForTest(n3.Pass());

  // Create some remote fake data.
  SyncDataList initial_data;
  initial_data.push_back(CreateSyncData("4", kAppId4, kKey4,
                                        "44", kUnread));
  initial_data.push_back(CreateSyncData("5", kAppId5, kKey5,
                                        "55", kUnread));
  initial_data.push_back(CreateSyncData("6", kAppId6, kKey6,
                                        "66", kUnread));
  initial_data.push_back(CreateSyncData("7", kAppId7, kKey7,
                                        "77", kUnread));

  // Merge the local and remote data.
  notifier.MergeDataAndStartSyncing(
      SYNCED_NOTIFICATIONS,
      initial_data,
      PassProcessor(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock()));

  // Ensure the local store now has all local and remote notifications.
  EXPECT_EQ(7U, notifier.GetAllSyncData(SYNCED_NOTIFICATIONS).size());
  EXPECT_TRUE(notifier.FindNotificationById(kKey1));
  EXPECT_TRUE(notifier.FindNotificationById(kKey2));
  EXPECT_TRUE(notifier.FindNotificationById(kKey3));
  EXPECT_TRUE(notifier.FindNotificationById(kKey4));
  EXPECT_TRUE(notifier.FindNotificationById(kKey5));
  EXPECT_TRUE(notifier.FindNotificationById(kKey6));
  EXPECT_TRUE(notifier.FindNotificationById(kKey7));

  // Test the type conversion and construction functions.
  for (SyncDataList::const_iterator iter = initial_data.begin();
      iter != initial_data.end(); ++iter) {
    scoped_ptr<SyncedNotification> notification1(
        ChromeNotifierService::CreateNotificationFromSyncData(*iter));
    // TODO(petewil): Revisit this when we add version info to notifications.
    const std::string& id = notification1->notification_id();
    const SyncedNotification* notification2 = notifier.FindNotificationById(id);
    EXPECT_TRUE(NULL != notification2);
    EXPECT_TRUE(notification1->EqualsIgnoringReadState(*notification2));
    EXPECT_EQ(notification1->read_state(), notification2->read_state());
  }
  EXPECT_TRUE(notifier.FindNotificationById(kKey1));
  EXPECT_TRUE(notifier.FindNotificationById(kKey2));
  EXPECT_TRUE(notifier.FindNotificationById(kKey3));
}

// Test the local store having the read bit unset, the remote store having
// it set.
TEST_F(ChromeNotifierServiceTest, ModelAssocBothNonEmptyReadMismatch1) {
  StubNotificationUIManager notification_manager;
  ChromeNotifierService notifier(NULL, &notification_manager);

  // Create some local fake data.
  scoped_ptr<SyncedNotification> n1(CreateNotification(
      "1", kAppId1, kKey1, "11", kUnread));
  notifier.AddForTest(n1.Pass());
  scoped_ptr<SyncedNotification> n2(CreateNotification(
      "2", kAppId2, kKey2, "22", kUnread));
  notifier.AddForTest(n2.Pass());

  // Create some remote fake data, item 1 matches except for the read state.
  syncer::SyncDataList initial_data;
  initial_data.push_back(CreateSyncData("1", kAppId1, kKey1,
                                        "11", kDismissed));
  // Merge the local and remote data.
  notifier.MergeDataAndStartSyncing(
      syncer::SYNCED_NOTIFICATIONS,
      initial_data,
      PassProcessor(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock()));

  // Ensure the local store still has only two notifications, and the read
  // state of the first is now read.
  EXPECT_EQ(2U, notifier.GetAllSyncData(syncer::SYNCED_NOTIFICATIONS).size());
  SyncedNotification* notification1 =
      notifier.FindNotificationById(kKey1);
  EXPECT_FALSE(NULL == notification1);
  EXPECT_EQ(SyncedNotification::kDismissed, notification1->read_state());
  EXPECT_TRUE(notifier.FindNotificationById(kKey2));
  EXPECT_FALSE(notifier.FindNotificationById(kKey3));

  // Ensure no new data will be sent to the remote store for notification1.
  EXPECT_EQ(0U, processor()->change_list_size());
  EXPECT_FALSE(processor()->ContainsId(kKey1));
}

// Test when the local store has the read bit set, and the remote store has
// it unset.
TEST_F(ChromeNotifierServiceTest, ModelAssocBothNonEmptyReadMismatch2) {
  StubNotificationUIManager notification_manager;
  ChromeNotifierService notifier(NULL, &notification_manager);

  // Create some local fake data.
  scoped_ptr<SyncedNotification> n1(CreateNotification(
      "1", kAppId1, kKey1, "11", kDismissed));
  notifier.AddForTest(n1.Pass());
  scoped_ptr<SyncedNotification> n2(CreateNotification(
      "2", kAppId2, kKey2, "22", kUnread));
  notifier.AddForTest(n2.Pass());

  // Create some remote fake data, item 1 matches except for the read state.
  syncer::SyncDataList initial_data;
  initial_data.push_back(CreateSyncData("1", kAppId1, kKey1,
                                        "11", kUnread));
  // Merge the local and remote data.
  notifier.MergeDataAndStartSyncing(
      syncer::SYNCED_NOTIFICATIONS,
      initial_data,
      PassProcessor(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock()));

  // Ensure the local store still has only two notifications, and the read
  // state of the first is now read.
  EXPECT_EQ(2U, notifier.GetAllSyncData(syncer::SYNCED_NOTIFICATIONS).size());
  SyncedNotification* notification1 =
      notifier.FindNotificationById(kKey1);
  EXPECT_FALSE(NULL == notification1);
  EXPECT_EQ(SyncedNotification::kDismissed, notification1->read_state());
  EXPECT_TRUE(notifier.FindNotificationById(kKey2));
  EXPECT_FALSE(notifier.FindNotificationById(kKey3));

  // Ensure the new data will be sent to the remote store for notification1.
  EXPECT_EQ(1U, processor()->change_list_size());
  EXPECT_TRUE(processor()->ContainsId(kKey1));
  EXPECT_EQ(SyncChange::ACTION_UPDATE, processor()->GetChangeById(
      kKey1).change_type());
}

// We have a notification in the local store, we get an updated version
// of the same notification remotely, it should take precedence.
TEST_F(ChromeNotifierServiceTest, ModelAssocBothNonEmptyWithUpdate) {
  StubNotificationUIManager notification_manager;
  ChromeNotifierService notifier(NULL, &notification_manager);

  // Create some local fake data.
  scoped_ptr<SyncedNotification> n1(CreateNotification(
      "1", kAppId1, kKey1, "11", kDismissed));
  notifier.AddForTest(n1.Pass());

  // Create some remote fake data, item 1 matches the ID, but has different data
  syncer::SyncDataList initial_data;
  initial_data.push_back(CreateSyncData("One", kAppId1, kKey1,
                                        "Eleven", kUnread));
  // Merge the local and remote data.
  notifier.MergeDataAndStartSyncing(
      syncer::SYNCED_NOTIFICATIONS,
      initial_data,
      PassProcessor(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock()));

  // Ensure the local store still has only one notification
  EXPECT_EQ(1U, notifier.GetAllSyncData(syncer::SYNCED_NOTIFICATIONS).size());
  SyncedNotification* notification1 =
      notifier.FindNotificationById(kKey1);
  EXPECT_FALSE(NULL == notification1);
  EXPECT_EQ(SyncedNotification::kUnread, notification1->read_state());
  EXPECT_EQ("One", notification1->title());
  EXPECT_EQ("Eleven", notification1->first_external_id());

  // Ensure no new data will be sent to the remote store for notification1.
  EXPECT_EQ(0U, processor()->change_list_size());
  EXPECT_FALSE(processor()->ContainsId(kKey1));
}

// TODO(petewil): There are more tests to add, such as when we add an API
// to allow data entry from the client, we might have a more up to date
// item on the client than the server, or we might have a merge conflict.
