// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/notifications/sync_notifier/sync_notifier_test_utils.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_error_factory_mock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center_util.h"

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

const int kNotificationPriority = static_cast<int>(
    message_center::LOW_PRIORITY);
// The test notification provider name shold match the name of the first
// synced notification service.
const char kTestNotificationProvider[] = "Google+";

// Extract notification id from syncer::SyncData.
std::string GetNotificationId(const SyncData& sync_data) {
  SyncedNotificationSpecifics specifics = sync_data.GetSpecifics().
      synced_notification();

  return specifics.coalesced_notification().key();
}

}  // namespace

namespace notifier {

// Stub out the NotificationUIManager for unit testing.
class StubNotificationUIManager : public NotificationUIManager {
 public:
  StubNotificationUIManager()
      : notification_(GURL(),
                      GURL(),
                      string16(),
                      string16(),
                      new MockNotificationDelegate("stub")) {}
  virtual ~StubNotificationUIManager() {}

  // Adds a notification to be displayed. Virtual for unit test override.
  virtual void Add(const Notification& notification, Profile* profile)
      OVERRIDE {
    // Make a deep copy of the notification that we can inspect.
    notification_ = notification;
    profile_ = profile;
  }

  virtual bool Update(const Notification& notification, Profile* profile)
      OVERRIDE {
    // Make a deep copy of the notification that we can inspect.
    notification_ = notification;
    profile_ = profile;
    return true;
  }

  // Returns true if any notifications match the supplied ID, either currently
  // displayed or in the queue.
  virtual const Notification* FindById(const std::string& id) const OVERRIDE {
    return (notification_.id() == id) ? &notification_ : NULL;
  }

  // Removes any notifications matching the supplied ID, either currently
  // displayed or in the queue.  Returns true if anything was removed.
  virtual bool CancelById(const std::string& notification_id) OVERRIDE {
    dismissed_id_ = notification_id;
    return true;
  }

  // Adds the notification_id for each outstanding notification to the set
  // |notification_ids| (must not be NULL).
  virtual std::set<std::string> GetAllIdsByProfileAndSourceOrigin(
      Profile* profile,
      const GURL& source) OVERRIDE {
    std::set<std::string> notification_ids;
    if (source == notification_.origin_url() &&
        profile->IsSameProfile(profile_))
      notification_ids.insert(notification_.notification_id());
    return notification_ids;
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

  // Test hook to get the notification so we can check it
  const Notification& notification() const { return notification_; }

  // Test hook to check the ID of the last notification cancelled.
  std::string& dismissed_id() { return dismissed_id_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(StubNotificationUIManager);
  Notification notification_;
  Profile* profile_;
  std::string dismissed_id_;
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

  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const
      OVERRIDE {
    return syncer::SyncDataList();
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

  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const
      OVERRIDE {
    return recipient_->GetAllSyncData(type);
  }

 private:
  // The recipient of all sync changes.
  SyncChangeProcessor* recipient_;

  DISALLOW_COPY_AND_ASSIGN(SyncChangeProcessorDelegate);
};

class ChromeNotifierServiceTest : public testing::Test {
 public:
  ChromeNotifierServiceTest()
      : sync_processor_(new TestChangeProcessor),
        sync_processor_delegate_(new SyncChangeProcessorDelegate(
            sync_processor_.get())) {}
  virtual ~ChromeNotifierServiceTest() {}

  // Methods from testing::Test.
  virtual void SetUp() {
    ChromeNotifierService::set_avoid_bitmap_fetching_for_test(true);
  }
  virtual void TearDown() {}

  TestChangeProcessor* processor() {
    return static_cast<TestChangeProcessor*>(sync_processor_.get());
  }

  scoped_ptr<syncer::SyncChangeProcessor> PassProcessor() {
    return sync_processor_delegate_.Pass();
  }

  SyncedNotification* CreateNotification(
      const std::string& title,
      const std::string& text,
      const std::string& app_icon_url,
      const std::string& image_url,
      const std::string& app_id,
      const std::string& key,
      sync_pb::CoalescedSyncedNotification_ReadState read_state) {
    SyncData sync_data = CreateSyncData(title, text, app_icon_url, image_url,
                                        app_id, key, read_state);
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

 private:
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_delegate_;
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNotifierServiceTest);
};

// TODO(petewil): Add more tests as I add functionalty.  Tests are based on
// chrome/browser/extensions/app_notification_manager_sync_unittest.cc

// Create a Notification, convert it to SyncData and convert it back.
TEST_F(ChromeNotifierServiceTest, NotificationToSyncDataToNotification) {
  scoped_ptr<SyncedNotification> notification1(
      CreateNotification(kTitle1, kText1, kIconUrl1, kImageUrl1, kAppId1,
                         kKey1, kUnread));
  SyncData sync_data =
      ChromeNotifierService::CreateSyncDataFromNotification(*notification1);
  scoped_ptr<SyncedNotification> notification2(
      ChromeNotifierService::CreateNotificationFromSyncData(sync_data));
  EXPECT_TRUE(notification2.get());
  EXPECT_TRUE(notification1->EqualsIgnoringReadState(*notification2));
  EXPECT_EQ(notification1->GetReadState(), notification2->GetReadState());
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
  notifier.set_avoid_bitmap_fetching_for_test(true);

  notifier.MergeDataAndStartSyncing(
      SYNCED_NOTIFICATIONS,
      SyncDataList(),
      PassProcessor(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock()));

  // Set up a bunch of ADDs.
  SyncChangeList changes;
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(
          kTitle1, kText1, kIconUrl1, kImageUrl1, kAppId1, kKey1, kUnread)));
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(
          kTitle2, kText2, kIconUrl2, kImageUrl2, kAppId2, kKey2, kUnread)));
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(
          kTitle3, kText3, kIconUrl3, kImageUrl3, kAppId3, kKey3, kUnread)));

  notifier.ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_EQ(3U, notifier.GetAllSyncData(SYNCED_NOTIFICATIONS).size());
  // TODO(petewil): verify that the list entries have expected values to make
  // this test more robust.
}

// Process sync changes when there is no local data.
TEST_F(ChromeNotifierServiceTest, ProcessSyncChangesNonEmptyModel) {
  StubNotificationUIManager notification_manager;
  ChromeNotifierService notifier(NULL, &notification_manager);
  notifier.set_avoid_bitmap_fetching_for_test(true);

  // Create some local fake data.
  scoped_ptr<SyncedNotification> n1(CreateNotification(
      kTitle1, kText1, kIconUrl1, kImageUrl1, kAppId1, kKey1, kUnread));
  notifier.AddForTest(n1.Pass());
  scoped_ptr<SyncedNotification> n2(CreateNotification(
      kTitle2, kText2, kIconUrl2, kImageUrl2, kAppId2, kKey2, kUnread));
  notifier.AddForTest(n2.Pass());
  scoped_ptr<SyncedNotification> n3(CreateNotification(
      kTitle3, kText3, kIconUrl3, kImageUrl3, kAppId3, kKey3, kUnread));
  notifier.AddForTest(n3.Pass());

  notifier.MergeDataAndStartSyncing(
      SYNCED_NOTIFICATIONS,
      SyncDataList(),
      PassProcessor(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock()));

  // Set up some ADDs, some UPDATES, and some DELETEs
  SyncChangeList changes;
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(
          kTitle4, kText4, kIconUrl4, kImageUrl4, kAppId4, kKey4, kUnread)));
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_UPDATE, CreateNotification(
          kTitle2, kText2, kIconUrl2, kImageUrl2, kAppId2, kKey2, kRead)));
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_DELETE, CreateNotification(
          kTitle3, kText3, kIconUrl3, kImageUrl3, kAppId3, kKey3, kDismissed)));

  // Simulate incoming new notifications at runtime.
  notifier.ProcessSyncChanges(FROM_HERE, changes);

  // We should find notifications 1, 2, and 4, but not 3.
  EXPECT_EQ(3U, notifier.GetAllSyncData(SYNCED_NOTIFICATIONS).size());
  EXPECT_TRUE(notifier.FindNotificationById(kKey1));
  EXPECT_TRUE(notifier.FindNotificationById(kKey2));
  EXPECT_FALSE(notifier.FindNotificationById(kKey3));
  EXPECT_TRUE(notifier.FindNotificationById(kKey4));
}

// Process sync changes that arrive before the change they are supposed to
// modify.
TEST_F(ChromeNotifierServiceTest, ProcessSyncChangesOutOfOrder) {
  StubNotificationUIManager notification_manager;
  ChromeNotifierService notifier(NULL, &notification_manager);
  notifier.set_avoid_bitmap_fetching_for_test(true);

  // Create some local fake data.
  scoped_ptr<SyncedNotification> n1(CreateNotification(
      kTitle1, kText1, kIconUrl1, kImageUrl1, kAppId1, kKey1, kUnread));
  notifier.AddForTest(n1.Pass());
  scoped_ptr<SyncedNotification> n2(CreateNotification(
      kTitle2, kText2, kIconUrl2, kImageUrl2, kAppId2, kKey2, kUnread));
  notifier.AddForTest(n2.Pass());
  scoped_ptr<SyncedNotification> n3(CreateNotification(
      kTitle3, kText3, kIconUrl3, kImageUrl3, kAppId3, kKey3, kUnread));
  notifier.AddForTest(n3.Pass());

  notifier.MergeDataAndStartSyncing(
      SYNCED_NOTIFICATIONS,
      SyncDataList(),
      PassProcessor(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock()));

  SyncChangeList changes;
  // UPDATE a notification we have not seen an add for.
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_UPDATE, CreateNotification(
          kTitle4, kText4, kIconUrl4, kImageUrl4, kAppId4, kKey4, kUnread)));
  // ADD a notification that we already have.
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_ADD, CreateNotification(
          kTitle2, kText2, kIconUrl2, kImageUrl2, kAppId2, kKey2, kRead)));
  // DELETE a notification we have not seen yet.
  changes.push_back(CreateSyncChange(
      SyncChange::ACTION_DELETE, CreateNotification(
          kTitle5, kText5, kIconUrl5, kImageUrl5, kAppId5, kKey5, kDismissed)));

  // Simulate incoming new notifications at runtime.
  notifier.ProcessSyncChanges(FROM_HERE, changes);

  // We should find notifications 1, 2, 3, and 4, but not 5.
  EXPECT_EQ(4U, notifier.GetAllSyncData(SYNCED_NOTIFICATIONS).size());
  EXPECT_TRUE(notifier.FindNotificationById(kKey1));
  EXPECT_TRUE(notifier.FindNotificationById(kKey2));
  EXPECT_TRUE(notifier.FindNotificationById(kKey3));
  EXPECT_TRUE(notifier.FindNotificationById(kKey4));
  EXPECT_FALSE(notifier.FindNotificationById(kKey5));
}


// Model has some notifications, some of them are local only. Sync has some
// notifications. No items match up.
TEST_F(ChromeNotifierServiceTest, LocalRemoteBothNonEmptyNoOverlap) {
  StubNotificationUIManager notification_manager;
  ChromeNotifierService notifier(NULL, &notification_manager);
  notifier.set_avoid_bitmap_fetching_for_test(true);

  // Create some local fake data.
  scoped_ptr<SyncedNotification> n1(CreateNotification(
      kTitle1, kText1, kIconUrl1, kImageUrl1, kAppId1, kKey1, kUnread));
  notifier.AddForTest(n1.Pass());
  scoped_ptr<SyncedNotification> n2(CreateNotification(
      kTitle2, kText2, kIconUrl2, kImageUrl2, kAppId2, kKey2, kUnread));
  notifier.AddForTest(n2.Pass());
  scoped_ptr<SyncedNotification> n3(CreateNotification(
      kTitle3, kText3, kIconUrl3, kImageUrl3, kAppId3, kKey3, kUnread));
  notifier.AddForTest(n3.Pass());

  // Create some remote fake data.
  SyncDataList initial_data;
  initial_data.push_back(CreateSyncData(kTitle4, kText4, kIconUrl4, kImageUrl4,
                                        kAppId4, kKey4, kUnread));
  initial_data.push_back(CreateSyncData(kTitle5, kText5, kIconUrl5, kImageUrl5,
                                        kAppId5, kKey5, kUnread));
  initial_data.push_back(CreateSyncData(kTitle6, kText6, kIconUrl6, kImageUrl6,
                                        kAppId6, kKey6, kUnread));
  initial_data.push_back(CreateSyncData(kTitle7, kText7, kIconUrl7, kImageUrl7,
                                        kAppId7, kKey7, kUnread));

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
    const std::string& key = notification1->GetKey();
    const SyncedNotification* notification2 =
        notifier.FindNotificationById(key);
    EXPECT_TRUE(NULL != notification2);
    EXPECT_TRUE(notification1->EqualsIgnoringReadState(*notification2));
    EXPECT_EQ(notification1->GetReadState(), notification2->GetReadState());
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
  notifier.set_avoid_bitmap_fetching_for_test(true);

  // Create some local fake data.
  scoped_ptr<SyncedNotification> n1(CreateNotification(
      kTitle1, kText1, kIconUrl1, kImageUrl1, kAppId1, kKey1, kUnread));
  notifier.AddForTest(n1.Pass());
  scoped_ptr<SyncedNotification> n2(CreateNotification(
      kTitle2, kText2, kIconUrl2, kImageUrl2, kAppId2, kKey2, kUnread));
  notifier.AddForTest(n2.Pass());

  // Create some remote fake data, item 1 matches except for the read state.
  syncer::SyncDataList initial_data;
  initial_data.push_back(CreateSyncData(kTitle1, kText1, kIconUrl1, kImageUrl1,
                                        kAppId1, kKey1, kDismissed));
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
  EXPECT_EQ(SyncedNotification::kDismissed, notification1->GetReadState());
  EXPECT_TRUE(notifier.FindNotificationById(kKey2));
  EXPECT_FALSE(notifier.FindNotificationById(kKey3));

  // Make sure that the notification manager was told to dismiss the
  // notification.
  EXPECT_EQ(std::string(kKey1), notification_manager.dismissed_id());

  // Ensure no new data will be sent to the remote store for notification1.
  EXPECT_EQ(0U, processor()->change_list_size());
  EXPECT_FALSE(processor()->ContainsId(kKey1));
}

// Test when the local store has the read bit set, and the remote store has
// it unset.
TEST_F(ChromeNotifierServiceTest, ModelAssocBothNonEmptyReadMismatch2) {
  StubNotificationUIManager notification_manager;
  ChromeNotifierService notifier(NULL, &notification_manager);
  notifier.set_avoid_bitmap_fetching_for_test(true);

  // Create some local fake data.
  scoped_ptr<SyncedNotification> n1(CreateNotification(
      kTitle1, kText1, kIconUrl1, kImageUrl1, kAppId1, kKey1, kDismissed));
  notifier.AddForTest(n1.Pass());
  scoped_ptr<SyncedNotification> n2(CreateNotification(
      kTitle2, kText2, kIconUrl2, kImageUrl2, kAppId2, kKey2, kUnread));
  notifier.AddForTest(n2.Pass());

  // Create some remote fake data, item 1 matches except for the read state.
  syncer::SyncDataList initial_data;
  initial_data.push_back(CreateSyncData(kTitle1, kText1, kIconUrl1, kImageUrl1,
                                        kAppId1, kKey1, kUnread));
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
  EXPECT_EQ(SyncedNotification::kDismissed, notification1->GetReadState());
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
      kTitle1, kText1, kIconUrl1, kImageUrl1, kAppId1, kKey1, kDismissed));
  notifier.AddForTest(n1.Pass());

  // Create some remote fake data, item 1 matches the ID, but has different data
  syncer::SyncDataList initial_data;
  initial_data.push_back(CreateSyncData(kTitle2, kText2, kIconUrl2, kImageUrl2,
                                        kAppId1, kKey1, kUnread));
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
  EXPECT_EQ(SyncedNotification::kUnread, notification1->GetReadState());
  EXPECT_EQ(std::string(kTitle2), notification1->GetTitle());

  // Ensure no new data will be sent to the remote store for notification1.
  EXPECT_EQ(0U, processor()->change_list_size());
  EXPECT_FALSE(processor()->ContainsId(kKey1));
}

TEST_F(ChromeNotifierServiceTest, ServiceEnabledTest) {
  StubNotificationUIManager notification_manager;
  ChromeNotifierService notifier(NULL, &notification_manager);
  std::vector<std::string>::iterator iter;

  // Create some local fake data.
  scoped_ptr<SyncedNotification> n1(CreateNotification(
      kTitle1, kText1, kIconUrl1, kImageUrl1, kAppId1, kKey1, kUnread));
  notifier.AddForTest(n1.Pass());

  // Enable the service and ensure the service is in the list.
  // Initially the service starts in the disabled state.
  notifier.OnSyncedNotificationServiceEnabled(kTestNotificationProvider, true);
  iter = find(notifier.enabled_sending_services_.begin(),
              notifier.enabled_sending_services_.end(),
              kTestNotificationProvider);
  EXPECT_NE(notifier.enabled_sending_services_.end(), iter);
  // TODO(petewil): Verify Display gets called too.

  // Disable the service and ensure it is gone from the list and the
  // notification_manager.
  notifier.OnSyncedNotificationServiceEnabled(kTestNotificationProvider, false);
  iter = find(notifier.enabled_sending_services_.begin(),
              notifier.enabled_sending_services_.end(),
              kTestNotificationProvider);
  EXPECT_EQ(notifier.enabled_sending_services_.end(), iter);
  EXPECT_EQ(notification_manager.dismissed_id(), std::string(kKey1));

}

}  // namespace notifier
