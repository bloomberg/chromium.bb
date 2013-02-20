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
using sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType_TITLE_AND_SUBTEXT;
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
const char kCoalescingKey1[] = "foo";
const char kCoalescingKey2[] = "bar";
const char kCoalescingKey3[] = "bat";
const char kCoalescingKey4[] = "baz";
const char kCoalescingKey5[] = "foobar";
const char kCoalescingKey6[] = "fu";
const char kCoalescingKey7[] = "meta";
const char kNotificationId1[] = "fboilmbenheemaomgaeehigklolhkhnf/foo";
const char kNotificationId2[] = "fbcmoldooppoahjhfflnmljoanccekpf/bar";
const char kNotificationId3[] = "fbcmoldooppoahjhfflnmljoanccek33/bat";
const char kNotificationId4[] = "fbcmoldooppoahjhfflnmljoanccek44/baz";
const char kNotificationId5[] = "fbcmoldooppoahjhfflnmljoanccek55/foobar";
const char kNotificationId6[] = "fbcmoldooppoahjhfflnmljoanccek66/fu";
const char kNotificationId7[] = "fbcmoldooppoahjhfflnmljoanccek77/meta";

const int64 kFakeCreationTime = 42;

// Extract notification id from syncer::SyncData.
std::string GetNotificationId(const SyncData& sync_data) {
  SyncedNotificationSpecifics specifics = sync_data.GetSpecifics().
      synced_notification();
  std::string notification_id = specifics.
      coalesced_notification().id().app_id();
  notification_id += "/";
  notification_id += specifics.coalesced_notification().id().coalescing_key();
  return notification_id;
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

  SyncedNotification* CreateNotification(const std::string& message,
                                         const std::string& app_id,
                                         const std::string& coalescing_key,
                                         const std::string& external_id) {
    SyncData sync_data = CreateSyncData(message, app_id, coalescing_key,
                                        external_id);
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
  static SyncData CreateSyncData(const std::string& message,
                                 const std::string& app_id,
                                 const std::string& coalescing_key,
                                 const std::string& external_id) {
    // CreateLocalData makes a copy of this, so this can safely live
    // on the stack.
    EntitySpecifics entity_specifics;

    SyncedNotificationSpecifics* specifics =
        entity_specifics.mutable_synced_notification();

    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_layout()->
        set_layout_type(
            SyncedNotificationRenderInfo_Layout_LayoutType_TITLE_AND_SUBTEXT);

    specifics->mutable_coalesced_notification()->
        mutable_id()->
        set_app_id(app_id);

    specifics->mutable_coalesced_notification()->
        mutable_id()->
        set_coalescing_key(coalescing_key);

    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_layout()->
        mutable_title_and_subtext_data()->
        set_title(message);

    specifics->mutable_coalesced_notification()->
        set_creation_time_msec(kFakeCreationTime);

    specifics->mutable_coalesced_notification()->
        add_notification();
    specifics->mutable_coalesced_notification()->
        mutable_notification(0)->set_external_id(external_id);

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

// Create a Notification, convert it to SyncData and convert it back.
TEST_F(ChromeNotifierServiceTest, NotificationToSyncDataToNotification) {
  // TODO(petewil): Add more properties to this test.
  scoped_ptr<SyncedNotification> notification1(
      CreateNotification("1", kAppId1, kCoalescingKey1, "11"));
  SyncData sync_data =
      ChromeNotifierService::CreateSyncDataFromNotification(*notification1);
  scoped_ptr<SyncedNotification> notification2(
      ChromeNotifierService::CreateNotificationFromSyncData(sync_data));
  EXPECT_TRUE(notification2.get());
  EXPECT_TRUE(notification1->Equals(*notification2));
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
          "1", kAppId1, kCoalescingKey1, "11")));
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(
          "2", kAppId2, kCoalescingKey2, "22")));
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(
          "3", kAppId3, kCoalescingKey3, "33")));

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
      "1", kAppId1, kCoalescingKey1, "11"));
  notifier.AddForTest(n1.Pass());
  scoped_ptr<SyncedNotification> n2(CreateNotification(
      "2", kAppId2, kCoalescingKey2, "22"));
  notifier.AddForTest(n2.Pass());
  scoped_ptr<SyncedNotification> n3(CreateNotification(
      "3", kAppId3, kCoalescingKey3, "33"));
  notifier.AddForTest(n3.Pass());

  // Create some remote fake data.
  SyncDataList initial_data;
  initial_data.push_back(CreateSyncData("4", kAppId4, kCoalescingKey4, "44"));
  initial_data.push_back(CreateSyncData("5", kAppId5, kCoalescingKey5, "55"));
  initial_data.push_back(CreateSyncData("6", kAppId6, kCoalescingKey6, "66"));
  initial_data.push_back(CreateSyncData("7", kAppId7, kCoalescingKey7, "77"));

  // Merge the local and remote data.
  notifier.MergeDataAndStartSyncing(
      SYNCED_NOTIFICATIONS,
      initial_data,
      PassProcessor(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock()));

  // Ensure the local store now has all local and remote notifications.
  EXPECT_EQ(7U, notifier.GetAllSyncData(SYNCED_NOTIFICATIONS).size());
  for (SyncDataList::const_iterator iter = initial_data.begin();
      iter != initial_data.end(); ++iter) {
    scoped_ptr<SyncedNotification> notification1(
        ChromeNotifierService::CreateNotificationFromSyncData(*iter));
    // TODO(petewil): Revisit this when we add version info to notifications.
    const std::string& id = notification1->notification_id();
    const SyncedNotification* notification2 = notifier.FindNotificationById(id);
    EXPECT_TRUE(NULL != notification2);
    EXPECT_TRUE(notification1->Equals(*notification2));
  }
  EXPECT_TRUE(notifier.FindNotificationById(kNotificationId1));
  EXPECT_TRUE(notifier.FindNotificationById(kNotificationId2));
  EXPECT_TRUE(notifier.FindNotificationById(kNotificationId3));

  // Verify the changes made it up to the remote service.
  EXPECT_EQ(3U, processor()->change_list_size());
  EXPECT_TRUE(processor()->ContainsId(kNotificationId1));
  EXPECT_EQ(SyncChange::ACTION_ADD, processor()->GetChangeById(
      kNotificationId1).change_type());
  EXPECT_FALSE(processor()->ContainsId(kNotificationId4));
  EXPECT_TRUE(processor()->ContainsId(kNotificationId3));
  EXPECT_EQ(SyncChange::ACTION_ADD, processor()->GetChangeById(
      kNotificationId3).change_type());
}

// TODO(petewil): There are more tests to add, such as when an item in
// the local store matches up with one from the server, with and without
// merge conflicts.
