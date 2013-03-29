// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/notifications/notification.h"
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
const char kIconUrl[] = "http://www.google.com/icon1.jpg";
const char kTitle1[] = "New appointment at 2:15";
const char kTitle2[] = "Email from Mark: Upcoming Ski trip";
const char kTitle3[] = "Weather alert - light rain tonight.";
const char kTitle4[] = "Zombie Alert on I-405";
const char kTitle5[] = "5-dimensional plutonian steam hockey scores";
const char kTitle6[] = "Conterfactuals Inc Stock report";
const char kTitle7[] = "Push Messaging app updated";
const char kText1[] = "Space Needle, 12:00 pm";
const char kText2[] = "Stevens Pass is our first choice.";
const char kText3[] = "More rain expected in the Seattle area tonight.";
const char kText4[] = "Traffic slowdown as motorists are hitting zombies";
const char kText5[] = "Neptune wins, pi to e";
const char kText6[] = "Beef flavored base for soups";
const char kText7[] = "You now have the latest version of Push Messaging App.";
const char kIconUrl1[] = "http://www.google.com/icon1.jpg";
const char kIconUrl2[] = "http://www.google.com/icon2.jpg";
const char kIconUrl3[] = "http://www.google.com/icon3.jpg";
const char kIconUrl4[] = "http://www.google.com/icon4.jpg";
const char kIconUrl5[] = "http://www.google.com/icon5.jpg";
const char kIconUrl6[] = "http://www.google.com/icon6.jpg";
const char kIconUrl7[] = "http://www.google.com/icon7.jpg";
const char kImageUrl1[] = "http://www.google.com/image1.jpg";
const char kImageUrl2[] = "http://www.google.com/image2.jpg";
const char kImageUrl3[] = "http://www.google.com/image3.jpg";
const char kImageUrl4[] = "http://www.google.com/image4.jpg";
const char kImageUrl5[] = "http://www.google.com/image5.jpg";
const char kImageUrl6[] = "http://www.google.com/image6.jpg";
const char kImageUrl7[] = "http://www.google.com/image7.jpg";
const char kExpectedOriginUrl[] =
    "chrome-extension://fboilmbenheemaomgaeehigklolhkhnf/";
const char kDefaultDestinationTitle[] = "Open web page";
const char kDefaultDestinationIconUrl[] = "http://www.google.com/image4.jpg";
const char kDefaultDestinationUrl[] = "http://www.google.com";
const char kButtonOneTitle[] = "Read";
const char kButtonOneIconUrl[] = "http://www.google.com/image8.jpg";
const char kButtonOneUrl[] = "http://www.google.com/do-something1";
const char kButtonTwoTitle[] = "Reply";
const char kButtonTwoIconUrl[] = "http://www.google.com/image9.jpg";
const char kButtonTwoUrl[] = "http://www.google.com/do-something2";
const char kContainedTitle1[] = "Today's Picnic moved";
const char kContainedTitle2[] = "Group Run Today";
const char kContainedTitle3[] = "Starcraft Tonight";
const char kContainedMessage1[] = "Due to rain, we will be inside the cafe.";
const char kContainedMessage2[] = "Meet at noon in the Gym.";
const char kContainedMessage3[] = "Let's play starcraft tonight on the LAN.";
const int64 kFakeCreationTime = 42;
const int kProtobufPriority = static_cast<int>(
    sync_pb::CoalescedSyncedNotification_Priority_LOW);
#if defined (ENABLE_MESSAGE_CENTER)
const int kNotificationPriority = static_cast<int>(
    message_center::LOW_PRIORITY);
#else  // ENABLE_MESSAGE_CENTER
const int kNotificationPriority = 1;
#endif  // ENABLE_MESSAGE_CENTER
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
  StubNotificationUIManager() : notification_(GURL(), GURL(), string16(),
                                              string16(), NULL) {}
  virtual ~StubNotificationUIManager() {}

  // Adds a notification to be displayed. Virtual for unit test override.
  virtual void Add(const Notification& notification, Profile* profile)
      OVERRIDE {
    // Make a deep copy of the notification that we can inspect.
    notification_ = notification;
  }

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

  // Test hook to get the notification so we can check it
  const Notification& notification() const { return notification_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(StubNotificationUIManager);
  Notification notification_;
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

  // Helper to create syncer::SyncData.
  static SyncData CreateSyncData(
      const std::string& title,
      const std::string& text,
      const std::string& app_icon_url,
      const std::string& image_url,
      const std::string& app_id,
      const std::string& key,
      const sync_pb::CoalescedSyncedNotification_ReadState read_state) {
    // CreateLocalData makes a copy of this, so this can safely live
    // on the stack.
    EntitySpecifics entity_specifics;

    // Get a writeable pointer to the sync notifications specifics inside the
    // entity specifics.
    SyncedNotificationSpecifics* specifics =
        entity_specifics.mutable_synced_notification();

    specifics->mutable_coalesced_notification()->
        set_app_id(app_id);

    specifics->mutable_coalesced_notification()->
        set_key(key);

    specifics->mutable_coalesced_notification()->
        set_priority(static_cast<sync_pb::CoalescedSyncedNotification_Priority>(
            kProtobufPriority));

    // Set the title.
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        mutable_simple_expanded_layout()->
        set_title(title);

    // Set the text.
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        mutable_simple_expanded_layout()->
        set_text(text);

    // Set the heading.
    specifics->
        mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        mutable_simple_collapsed_layout()->
        set_heading(title);

    // Add the collapsed info and set the app_icon_url on it.
    specifics->
        mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        add_collapsed_info();
    specifics->
        mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        mutable_collapsed_info(0)->
        mutable_simple_collapsed_layout()->
        mutable_app_icon()->
        set_url(app_icon_url);

    // Add the media object and set the image url on it.
    specifics->
        mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        mutable_simple_expanded_layout()->
        add_media();
    specifics->
        mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        mutable_simple_expanded_layout()->
        mutable_media(0)->
        mutable_image()->
        set_url(image_url);

    specifics->mutable_coalesced_notification()->
        set_creation_time_msec(kFakeCreationTime);

    specifics->mutable_coalesced_notification()->
        set_read_state(read_state);

    // Contained notification one.
    // We re-use the collapsed info we added for the app_icon_url,
    // so no need to create another one here.
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        mutable_collapsed_info(0)->
        mutable_simple_collapsed_layout()->
        set_heading(kContainedTitle1);
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        mutable_collapsed_info(0)->
        mutable_simple_collapsed_layout()->
        set_description(kContainedMessage1);

    // Contained notification two.
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        add_collapsed_info();
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        mutable_collapsed_info(1)->
        mutable_simple_collapsed_layout()->
        set_heading(kContainedTitle2);
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        mutable_collapsed_info(1)->
        mutable_simple_collapsed_layout()->
        set_description(kContainedMessage2);

    // Contained notification three.
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        add_collapsed_info();
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        mutable_collapsed_info(2)->
        mutable_simple_collapsed_layout()->
        set_heading(kContainedTitle3);
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        mutable_collapsed_info(2)->
        mutable_simple_collapsed_layout()->
        set_description(kContainedMessage3);

    // Default Destination.
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        mutable_default_destination()->
        set_text(kDefaultDestinationTitle);
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        mutable_default_destination()->
        mutable_icon()->
        set_url(kDefaultDestinationIconUrl);
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        mutable_default_destination()->
        mutable_icon()->
        set_alt_text(kDefaultDestinationTitle);
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        mutable_default_destination()->
        set_url(kDefaultDestinationUrl);

    // Buttons are represented as targets.

    // Button One.
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        add_target();
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        mutable_target(0)->
        mutable_action()->
        set_text(kButtonOneTitle);
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        mutable_target(0)->
        mutable_action()->
        mutable_icon()->
        set_url(kButtonOneIconUrl);
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        mutable_target(0)->
        mutable_action()->
        mutable_icon()->
        set_alt_text(kButtonOneTitle);
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        mutable_target(0)->
        mutable_action()->
        set_url(kButtonOneUrl);

    // Button Two.
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        add_target();
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        mutable_target(1)->
        mutable_action()->
        set_text(kButtonTwoTitle);
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        mutable_target(1)->
        mutable_action()->
        mutable_icon()->
        set_url(kButtonTwoIconUrl);
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        mutable_target(1)->
        mutable_action()->
        mutable_icon()->
        set_alt_text(kButtonTwoTitle);
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_collapsed_info()->
        mutable_target(1)->
        mutable_action()->
        set_url(kButtonTwoUrl);

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

// Model has some notifications, some of them are local only. Sync has some
// notifications. No items match up.
TEST_F(ChromeNotifierServiceTest, LocalRemoteBothNonEmptyNoOverlap) {
  StubNotificationUIManager notification_manager;
  ChromeNotifierService notifier(NULL, &notification_manager);

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
  EXPECT_TRUE(notifier.FindNotificationByKey(kKey1));
  EXPECT_TRUE(notifier.FindNotificationByKey(kKey2));
  EXPECT_TRUE(notifier.FindNotificationByKey(kKey3));
  EXPECT_TRUE(notifier.FindNotificationByKey(kKey4));
  EXPECT_TRUE(notifier.FindNotificationByKey(kKey5));
  EXPECT_TRUE(notifier.FindNotificationByKey(kKey6));
  EXPECT_TRUE(notifier.FindNotificationByKey(kKey7));

  // Test the type conversion and construction functions.
  for (SyncDataList::const_iterator iter = initial_data.begin();
      iter != initial_data.end(); ++iter) {
    scoped_ptr<SyncedNotification> notification1(
        ChromeNotifierService::CreateNotificationFromSyncData(*iter));
    // TODO(petewil): Revisit this when we add version info to notifications.
    const std::string& key = notification1->GetKey();
    const SyncedNotification* notification2 =
        notifier.FindNotificationByKey(key);
    EXPECT_TRUE(NULL != notification2);
    EXPECT_TRUE(notification1->EqualsIgnoringReadState(*notification2));
    EXPECT_EQ(notification1->GetReadState(), notification2->GetReadState());
  }
  EXPECT_TRUE(notifier.FindNotificationByKey(kKey1));
  EXPECT_TRUE(notifier.FindNotificationByKey(kKey2));
  EXPECT_TRUE(notifier.FindNotificationByKey(kKey3));
}

// Test the local store having the read bit unset, the remote store having
// it set.
TEST_F(ChromeNotifierServiceTest, ModelAssocBothNonEmptyReadMismatch1) {
  StubNotificationUIManager notification_manager;
  ChromeNotifierService notifier(NULL, &notification_manager);

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
      notifier.FindNotificationByKey(kKey1);
  EXPECT_FALSE(NULL == notification1);
  EXPECT_EQ(SyncedNotification::kDismissed, notification1->GetReadState());
  EXPECT_TRUE(notifier.FindNotificationByKey(kKey2));
  EXPECT_FALSE(notifier.FindNotificationByKey(kKey3));

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
      notifier.FindNotificationByKey(kKey1);
  EXPECT_FALSE(NULL == notification1);
  EXPECT_EQ(SyncedNotification::kDismissed, notification1->GetReadState());
  EXPECT_TRUE(notifier.FindNotificationByKey(kKey2));
  EXPECT_FALSE(notifier.FindNotificationByKey(kKey3));

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
      notifier.FindNotificationByKey(kKey1);

  EXPECT_FALSE(NULL == notification1);
  EXPECT_EQ(SyncedNotification::kUnread, notification1->GetReadState());
  EXPECT_EQ(kTitle2, notification1->GetTitle());

  // Ensure no new data will be sent to the remote store for notification1.
  EXPECT_EQ(0U, processor()->change_list_size());
  EXPECT_FALSE(processor()->ContainsId(kKey1));
}

#if defined (ENABLE_MESSAGE_CENTER)
TEST_F(ChromeNotifierServiceTest, ShowTest) {
  StubNotificationUIManager notification_manager;
  ChromeNotifierService notifier(NULL, &notification_manager);

  // Create some remote fake data
  syncer::SyncDataList initial_data;
  initial_data.push_back(CreateSyncData(kTitle1, kText1, kIconUrl1, kImageUrl1,
                                        kAppId1, kKey1, kUnread));
  // Merge the local and remote data.
  notifier.MergeDataAndStartSyncing(
      syncer::SYNCED_NOTIFICATIONS,
      initial_data,
      PassProcessor(),
      scoped_ptr<syncer::SyncErrorFactory>(new syncer::SyncErrorFactoryMock()));

  // Check the base fields of the notification.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_IMAGE,
            notification_manager.notification().type());
  EXPECT_EQ(kTitle1,
            UTF16ToUTF8(notification_manager.notification().title()));
  EXPECT_EQ(kText1,
            UTF16ToUTF8(notification_manager.notification().body()));
  EXPECT_EQ(kExpectedOriginUrl,
            notification_manager.notification().origin_url().spec());
  EXPECT_EQ(kIconUrl, notification_manager.notification().icon_url().spec());
  EXPECT_EQ(kKey1,
            UTF16ToUTF8(notification_manager.notification().replace_id()));
  const DictionaryValue* actual_fields =
      notification_manager.notification().optional_fields();

  // Check the optional fields of the notification.
  // Make an optional fields struct like we expect, compare it with actual.
  DictionaryValue expected_fields;
  expected_fields.SetDouble(message_center::kTimestampKey, kFakeCreationTime);
  expected_fields.SetInteger(message_center::kPriorityKey,
                             kNotificationPriority);
  expected_fields.SetString(message_center::kButtonOneTitleKey,
                            kButtonOneTitle);
  expected_fields.SetString(message_center::kButtonOneIconUrlKey,
                            kButtonOneIconUrl);
  expected_fields.SetString(message_center::kButtonTwoTitleKey,
                            kButtonTwoTitle);
  expected_fields.SetString(message_center::kButtonTwoIconUrlKey,
                            kButtonTwoIconUrl);

  // Fill the individual notification fields for a mutiple notification.
  base::ListValue* items = new base::ListValue();
  DictionaryValue* item1 = new DictionaryValue();
  DictionaryValue* item2 = new DictionaryValue();
  DictionaryValue* item3 = new DictionaryValue();
  item1->SetString(message_center::kItemTitleKey,
                   UTF8ToUTF16(kContainedTitle1));
  item1->SetString(message_center::kItemMessageKey,
                   UTF8ToUTF16(kContainedMessage1));
  item2->SetString(message_center::kItemTitleKey,
                   UTF8ToUTF16(kContainedTitle2));
  item2->SetString(message_center::kItemMessageKey,
                   UTF8ToUTF16(kContainedMessage2));
  item3->SetString(message_center::kItemTitleKey,
                   UTF8ToUTF16(kContainedTitle3));
  item3->SetString(message_center::kItemMessageKey,
                   UTF8ToUTF16(kContainedMessage3));
  items->Append(item1);
  items->Append(item2);
  items->Append(item3);
  expected_fields.Set(message_center::kItemsKey, items);

  EXPECT_TRUE(expected_fields.Equals(actual_fields))
      << "Expected: " << expected_fields
      << ", but actual: " << *actual_fields;
}
#endif  // ENABLE_MESSAGE_CENTER

// TODO(petewil): There are more tests to add, such as when we add an API
// to allow data entry from the client, we might have a more up to date
// item on the client than the server, or we might have a merge conflict.
