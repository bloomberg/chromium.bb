// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification.h"
#include "sync/api/sync_data.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/synced_notification_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/notification_types.h"

using syncer::SyncData;
using notifier::SyncedNotification;
using sync_pb::EntitySpecifics;
using sync_pb::SyncedNotificationSpecifics;

namespace {

const uint64 kFakeCreationTime = 42;
const int kProtobufPriority = static_cast<int>(
    sync_pb::CoalescedSyncedNotification_Priority_LOW);
#if defined (ENABLE_MESSAGE_CENTER)
const int kNotificationPriority = static_cast<int>(
    message_center::LOW_PRIORITY);
#else  // ENABLE_MESSAGE_CENTER
const int kNotificationPriority = 1;
#endif  // ENABLE_MESSAGE_CENTER


const char kTitle1[] = "New appointment at 2:15";
const char kTitle2[] = "Email from Mark: Upcoming Ski trip";
const char kTitle3[] = "Weather alert - light rain tonight.";
const char kAppId1[] = "fboilmbenheemaomgaeehigklolhkhnf";
const char kAppId2[] = "fbcmoldooppoahjhfflnmljoanccekpf";
const char kKey1[] = "foo";
const char kKey2[] = "bar";
const char kText1[] = "Space Needle, 12:00 pm";
const char kText2[] = "Stevens Pass is our first choice.";
const char kText3[] = "More rain expected in the Seattle area tonight.";
const char kIconUrl1[] = "http://www.google.com/icon1.jpg";
const char kIconUrl2[] = "http://www.google.com/icon2.jpg";
const char kIconUrl3[] = "http://www.google.com/icon3.jpg";
const char kImageUrl1[] = "http://www.google.com/image1.jpg";
const char kImageUrl2[] = "http://www.google.com/image2.jpg";
const char kImageUrl3[] = "http://www.google.com/image3.jpg";
const char kDefaultDestinationTitle[] = "Open web page";
const char kDefaultDestinationIconUrl[] = "http://www.google.com/image4.jpg";
const char kDefaultDestinationUrl[] = "http://www.google.com";
const char kButtonOneTitle[] = "Read";
const char kButtonOneIconUrl[] = "http://www.google.com/image5.jpg";
const char kButtonOneUrl[] = "http://www.google.com/do-something1";
const char kButtonTwoTitle[] = "Reply";
const char kButtonTwoIconUrl[] = "http://www.google.com/image6.jpg";
const char kButtonTwoUrl[] = "http://www.google.com/do-something2";
const char kContainedTitle1[] = "Today's Picnic moved";
const char kContainedTitle2[] = "Group Run Today";
const char kContainedTitle3[] = "Starcraft Tonight";
const char kContainedMessage1[] = "Due to rain, we will be inside the cafe.";
const char kContainedMessage2[] = "Meet at noon in the Gym.";
const char kContainedMessage3[] = "Let's play starcraft tonight on the LAN.";

const sync_pb::CoalescedSyncedNotification_ReadState kRead =
    sync_pb::CoalescedSyncedNotification_ReadState_READ;
const sync_pb::CoalescedSyncedNotification_ReadState kUnread =
    sync_pb::CoalescedSyncedNotification_ReadState_UNREAD;
const sync_pb::CoalescedSyncedNotification_ReadState kDismissed =
    sync_pb::CoalescedSyncedNotification_ReadState_DISMISSED;
}  // namespace

class SyncedNotificationTest : public testing::Test {
 public:
  SyncedNotificationTest() {}
  ~SyncedNotificationTest() {}

  // Methods from testing::Test.

  virtual void SetUp() OVERRIDE {
    sync_data1_ = CreateSyncData(kTitle1, kText1, kIconUrl1, kImageUrl1,
                                 kAppId1, kKey1, kUnread);
    sync_data2_ = CreateSyncData(kTitle2, kText2, kIconUrl2, kImageUrl2,
                                 kAppId2, kKey2, kUnread);
    // Notification 3 will have the same ID as notification1, but different
    // data inside.
    sync_data3_ = CreateSyncData(kTitle3, kText3, kIconUrl3, kImageUrl3,
                                 kAppId1, kKey1, kUnread);
    // Notification 4 will be the same as 1, but the read state will be 'read'.
    sync_data4_ = CreateSyncData(kTitle1, kText1, kIconUrl1, kImageUrl1,
                                 kAppId1, kKey1, kDismissed);

    notification1_.reset(new SyncedNotification(sync_data1_));
    notification2_.reset(new SyncedNotification(sync_data2_));
    notification3_.reset(new SyncedNotification(sync_data3_));
    notification4_.reset(new SyncedNotification(sync_data4_));
  }

  virtual void TearDown() OVERRIDE {
  }

  scoped_ptr<SyncedNotification> notification1_;
  scoped_ptr<SyncedNotification> notification2_;
  scoped_ptr<SyncedNotification> notification3_;
  scoped_ptr<SyncedNotification> notification4_;
  syncer::SyncData sync_data1_;
  syncer::SyncData sync_data2_;
  syncer::SyncData sync_data3_;
  syncer::SyncData sync_data4_;

 private:
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

    // Add the collapsed info and set the app_icon_url on it.
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        add_collapsed_info();
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        mutable_collapsed_info(0)->
        mutable_simple_collapsed_layout()->
        mutable_app_icon()->
        set_url(app_icon_url);

    // Add the media object and set the image url on it.
    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_expanded_info()->
        mutable_simple_expanded_layout()->
        add_media();
    specifics->mutable_coalesced_notification()->
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
        "SyncedNotificationTest",
        entity_specifics);

    return sync_data;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedNotificationTest);
};

// test simple accessors

TEST_F(SyncedNotificationTest, GetAppIdTest) {
  std::string found_app_id = notification1_->GetAppId();
  std::string expected_app_id(kAppId1);

  EXPECT_EQ(found_app_id, expected_app_id);
}

TEST_F(SyncedNotificationTest, GetKeyTest) {
  std::string found_key = notification1_->GetKey();
  std::string expected_key(kKey1);

  EXPECT_EQ(expected_key, found_key);
}

TEST_F(SyncedNotificationTest, GetTitleTest) {
  std::string found_title = notification1_->GetTitle();
  std::string expected_title(kTitle1);

  EXPECT_EQ(expected_title, found_title);
}

TEST_F(SyncedNotificationTest, GetIconURLTest) {
  std::string found_icon_url = notification1_->GetAppIconUrl().spec();
  std::string expected_icon_url(kIconUrl1);

  EXPECT_EQ(expected_icon_url, found_icon_url);
}

TEST_F(SyncedNotificationTest, GetReadStateTest) {
  SyncedNotification::ReadState found_state1 =
      notification1_->GetReadState();
  SyncedNotification::ReadState expected_state1(SyncedNotification::kUnread);

  EXPECT_EQ(expected_state1, found_state1);

  SyncedNotification::ReadState found_state2 =
      notification4_->GetReadState();
  SyncedNotification::ReadState expected_state2(SyncedNotification::kDismissed);

  EXPECT_EQ(expected_state2, found_state2);
}

// TODO(petewil): Improve ctor to pass in an image and type so this test can
// pass on actual data.
TEST_F(SyncedNotificationTest, GetImageURLTest) {
  std::string found_image_url = notification1_->GetImageUrl().spec();
  std::string expected_image_url = kImageUrl1;

  EXPECT_EQ(expected_image_url, found_image_url);
}

// TODO(petewil): test with a multi-line body
TEST_F(SyncedNotificationTest, GetTextTest) {
  std::string found_text = notification1_->GetText();
  std::string expected_text(kText1);

  EXPECT_EQ(expected_text, found_text);
}

TEST_F(SyncedNotificationTest, GetCreationTimeTest) {
  uint64 found_time = notification1_->GetCreationTime();
  EXPECT_EQ(kFakeCreationTime, found_time);
}

TEST_F(SyncedNotificationTest, GetPriorityTest) {
  double found_priority = notification1_->GetPriority();
  EXPECT_EQ(static_cast<double>(kNotificationPriority), found_priority);
}

TEST_F(SyncedNotificationTest, GetButtonCountTest) {
  int found_button_count = notification1_->GetButtonCount();
  EXPECT_EQ(2, found_button_count);
}

TEST_F(SyncedNotificationTest, GetNotificationCountTest) {
  int found_notification_count = notification1_->GetNotificationCount();
  EXPECT_EQ(3, found_notification_count);
}

TEST_F(SyncedNotificationTest, GetDefaultDestinationDataTest) {
    std::string default_destination_title =
        notification1_->GetDefaultDestinationTitle();
    std::string default_destination_icon_url =
        notification1_->GetDefaultDestinationIconUrl();
    std::string default_destination_url =
        notification1_->GetDefaultDestinationUrl();
    EXPECT_EQ(std::string(kDefaultDestinationTitle), default_destination_title);
    EXPECT_EQ(std::string(kDefaultDestinationIconUrl),
              default_destination_icon_url);
    EXPECT_EQ(std::string(kDefaultDestinationUrl), default_destination_url);
}

TEST_F(SyncedNotificationTest, GetButtonDataTest) {
    std::string button_one_title = notification1_->GetButtonOneTitle();
    std::string button_one_icon_url = notification1_->GetButtonOneIconUrl();
    std::string button_one_url = notification1_->GetButtonOneUrl();
    std::string button_two_title = notification1_->GetButtonTwoTitle();
    std::string button_two_icon_url = notification1_->GetButtonTwoIconUrl();
    std::string button_two_url = notification1_->GetButtonTwoUrl();
    EXPECT_EQ(std::string(kButtonOneTitle), button_one_title);
    EXPECT_EQ(std::string(kButtonOneIconUrl), button_one_icon_url);
    EXPECT_EQ(std::string(kButtonOneUrl), button_one_url);
    EXPECT_EQ(std::string(kButtonTwoTitle), button_two_title);
    EXPECT_EQ(std::string(kButtonTwoIconUrl), button_two_icon_url);
    EXPECT_EQ(std::string(kButtonTwoUrl), button_two_url);
}

TEST_F(SyncedNotificationTest, ContainedNotificationTest) {
  std::string notification_title1 =
      notification1_->GetContainedNotificationTitle(0);
  std::string notification_title2 =
      notification1_->GetContainedNotificationTitle(1);
  std::string notification_title3 =
      notification1_->GetContainedNotificationTitle(2);
  std::string notification_message1 =
      notification1_->GetContainedNotificationMessage(0);
  std::string notification_message2 =
      notification1_->GetContainedNotificationMessage(1);
  std::string notification_message3 =
      notification1_->GetContainedNotificationMessage(2);

  EXPECT_EQ(std::string(kContainedTitle1), notification_title1);
  EXPECT_EQ(std::string(kContainedTitle2), notification_title2);
  EXPECT_EQ(std::string(kContainedTitle3), notification_title3);
  EXPECT_EQ(std::string(kContainedMessage1), notification_message1);
  EXPECT_EQ(std::string(kContainedMessage2), notification_message2);
  EXPECT_EQ(std::string(kContainedMessage3), notification_message3);
}

// test that EqualsIgnoringReadState works as we expect
TEST_F(SyncedNotificationTest, EqualsIgnoringReadStateTest) {
  EXPECT_TRUE(notification1_->EqualsIgnoringReadState(*notification1_));
  EXPECT_TRUE(notification2_->EqualsIgnoringReadState(*notification2_));
  EXPECT_FALSE(notification1_->EqualsIgnoringReadState(*notification2_));
  EXPECT_FALSE(notification1_->EqualsIgnoringReadState(*notification3_));
  EXPECT_TRUE(notification1_->EqualsIgnoringReadState(*notification4_));
}

TEST_F(SyncedNotificationTest, UpdateTest) {
  scoped_ptr<SyncedNotification> notification5;
  notification5.reset(new SyncedNotification(sync_data1_));

  // update with the sync data from notification2, and ensure they are equal.
  notification5->Update(sync_data2_);
  EXPECT_TRUE(notification5->EqualsIgnoringReadState(*notification2_));
  EXPECT_EQ(notification5->GetReadState(), notification2_->GetReadState());
  EXPECT_FALSE(notification5->EqualsIgnoringReadState(*notification1_));
}

// Add a test for a notification being read and or deleted.
