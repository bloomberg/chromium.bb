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

using syncer::SyncData;
using notifier::SyncedNotification;
using sync_pb::EntitySpecifics;
using sync_pb::SyncedNotificationSpecifics;
using sync_pb::SyncedNotificationRenderInfo_Layout_LayoutType_TITLE_AND_SUBTEXT;

namespace {

const int64 kFakeCreationTime = 42;

const char kTitle1[] = "New appointment at 2:15";
const char kTitle2[] = "Email from Mark: Upcoming Ski trip";
const char kTitle3[] = "Weather alert - light rain tonight.";
const char kAppId1[] = "fboilmbenheemaomgaeehigklolhkhnf";
const char kAppId2[] = "fbcmoldooppoahjhfflnmljoanccekpf";
const char kCoalescingKey1[] = "foo";
const char kCoalescingKey2[] = "bar";
const char kBody1[] = "Space Needle, 12:00 pm";
const char kBody2[] = "Stevens Pass is our first choice.";
const char kBody3[] = "More rain expected in the Seattle area tonight.";
const char kIconUrl1[] = "http://www.google.com/icon1.jpg";
const char kIconUrl2[] = "http://www.google.com/icon2.jpg";
const char kIconUrl3[] = "http://www.google.com/icon3.jpg";
const char kImageUrl1[] = "http://www.google.com/image1.jpg";
const char kImageUrl2[] = "http://www.google.com/image2.jpg";
const char kImageUrl3[] = "http://www.google.com/image3.jpg";
const sync_pb::CoalescedSyncedNotification_ReadState kRead =
    sync_pb::CoalescedSyncedNotification_ReadState_READ;
const sync_pb::CoalescedSyncedNotification_ReadState kUnread =
    sync_pb::CoalescedSyncedNotification_ReadState_UNREAD;
}  // namespace

class SyncedNotificationTest : public testing::Test {
 public:
  SyncedNotificationTest() {}
  ~SyncedNotificationTest() {}

  // Methods from testing::Test.

  virtual void SetUp() OVERRIDE {
    sync_data1_ = CreateSyncData(
        kTitle1, kBody1, kIconUrl1, kAppId1, kCoalescingKey1, kUnread);
    sync_data2_ = CreateSyncData(
        kTitle2, kBody2, kIconUrl2, kAppId2, kCoalescingKey2, kUnread);
    // Notification 3 will have the same ID as notification1, but different
    // data inside.
    sync_data3_ = CreateSyncData(
        kTitle3, kBody3, kIconUrl3, kAppId1, kCoalescingKey1, kUnread);
    // Notification 4 will be the same as 1, but the read state will be 'read'.
    sync_data4_ = CreateSyncData(
        kTitle1, kBody1, kIconUrl1, kAppId1, kCoalescingKey1, kRead);

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
      const std::string& body,
      const std::string& icon_url,
      const std::string& app_id,
      const std::string& coalescing_key,
      const sync_pb::CoalescedSyncedNotification_ReadState read_state) {
    // CreateLocalData makes a copy of this, so this can safely live
    // on the stack.
    EntitySpecifics entity_specifics;

    // Get a writeable pointer to the sync notifications specifics inside the
    // entity specifics.
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
        set_title(title);

    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_layout()->
        mutable_title_and_subtext_data()->
        add_subtext(body);

    specifics->mutable_coalesced_notification()->
        mutable_render_info()->
        mutable_layout()->
        mutable_title_and_subtext_data()->
        mutable_icon()->
        set_url(icon_url);

    specifics->mutable_coalesced_notification()->
        set_creation_time_msec(kFakeCreationTime);

    specifics->mutable_coalesced_notification()->
        add_notification();

    specifics->mutable_coalesced_notification()->
        set_read_state(read_state);

    // TODO(petewil): Improve ctor to pass in an image and type so this test can
    // pass on actual data.
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
  std::string found_app_id = notification1_->app_id();
  std::string expected_app_id(kAppId1);

  EXPECT_EQ(found_app_id, expected_app_id);
}

TEST_F(SyncedNotificationTest, GetCoalescingKeyTest) {
  std::string found_key = notification1_->coalescing_key();
  std::string expected_key(kCoalescingKey1);

  EXPECT_EQ(expected_key, found_key);
}

TEST_F(SyncedNotificationTest, GetTitleTest) {
  std::string found_title = notification1_->title();
  std::string expected_title(kTitle1);

  EXPECT_EQ(expected_title, found_title);
}

TEST_F(SyncedNotificationTest, GetIconURLTest) {
  std::string found_icon_url = notification1_->icon_url().spec();
  std::string expected_icon_url(kIconUrl1);

  EXPECT_EQ(expected_icon_url, found_icon_url);
}

TEST_F(SyncedNotificationTest, GetReadStateTest) {
  SyncedNotification::ReadState found_state1 =
      notification1_->read_state();
  SyncedNotification::ReadState expected_state1(SyncedNotification::kUnread);

  EXPECT_EQ(expected_state1, found_state1);

  SyncedNotification::ReadState found_state2 =
      notification4_->read_state();
  SyncedNotification::ReadState expected_state2(SyncedNotification::kRead);

  EXPECT_EQ(expected_state2, found_state2);
}

// TODO(petewil): Improve ctor to pass in an image and type so this test can
// pass on actual data.
TEST_F(SyncedNotificationTest, GetImageURLTest) {
  std::string found_image_url = notification1_->image_url().spec();
  std::string expected_image_url;  // TODO(petewil): (kImageUrl1)

  EXPECT_EQ(expected_image_url, found_image_url);
}

// TODO(petewil): test with a multi-line body
TEST_F(SyncedNotificationTest, GetBodyTest) {
  std::string found_body = notification1_->body();
  std::string expected_body(kBody1);

  EXPECT_EQ(expected_body, found_body);
}

TEST_F(SyncedNotificationTest, GetNotificationIdTest) {
  std::string found_id = notification1_->notification_id();
  std::string expected_id(kAppId1);
  expected_id += "/";
  expected_id += kCoalescingKey1;

  EXPECT_EQ(expected_id, found_id);
}

// test that the ID match function works as we expect
TEST_F(SyncedNotificationTest, IdMatchesTest) {
  EXPECT_TRUE(notification1_->IdMatches(*notification1_));
  EXPECT_TRUE(notification2_->IdMatches(*notification2_));
  EXPECT_FALSE(notification1_->IdMatches(*notification2_));
  EXPECT_TRUE(notification1_->IdMatches(*notification3_));
  EXPECT_TRUE(notification1_->IdMatches(*notification4_));
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
  EXPECT_EQ(notification5->read_state(), notification2_->read_state());
  EXPECT_FALSE(notification5->EqualsIgnoringReadState(*notification1_));
}

// Add a test for set_local_changes and has_local_changes together
// Add a test for a notification being read and or deleted
