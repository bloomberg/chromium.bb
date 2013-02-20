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
const char kAppId1[] = "fboilmbenheemaomgaeehigklolhkhnf";
const char kAppId2[] = "fbcmoldooppoahjhfflnmljoanccekpf";
const char kCoalescingKey1[] = "foo";
const char kCoalescingKey2[] = "bar";
const char kBody1[] = "Space Needle, 12:00 pm";
const char kBody2[] = "Stevens Pass is our first choice.";
const char kIconUrl1[] = "http://www.google.com/icon1.jpg";
const char kIconUrl2[] = "http://www.google.com/icon2.jpg";
const char kImageUrl1[] = "http://www.google.com/image1.jpg";
const char kImageUrl2[] = "http://www.google.com/image2.jpg";

}  // namespace

class SyncedNotificationTest : public testing::Test {
 public:
  SyncedNotificationTest() : notification1(NULL),
                             notification2(NULL) {}
  virtual ~SyncedNotificationTest() {}

  // Methods from testing::Test.

  virtual void SetUp() OVERRIDE {
    syncer::SyncData sync_data1 = CreateSyncData(kTitle1, kBody1, kIconUrl1,
                                                 kAppId1, kCoalescingKey1);
    syncer::SyncData sync_data2 = CreateSyncData(kTitle2, kBody2, kIconUrl2,
                                                 kAppId2, kCoalescingKey2);

    notification1.reset(new SyncedNotification(sync_data1));
    notification2.reset(new SyncedNotification(sync_data2));
  }

  virtual void TearDown() OVERRIDE {
  }

  scoped_ptr<SyncedNotification> notification1;
  scoped_ptr<SyncedNotification> notification2;

 private:
  // Helper to create syncer::SyncData.
  static SyncData CreateSyncData(const std::string& title,
                                 const std::string& body,
                                 const std::string& icon_url,
                                 const std::string& app_id,
                                 const std::string& coalescing_key) {
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
  std::string found_app_id = notification1->app_id();
  std::string expected_app_id(kAppId1);

  EXPECT_EQ(found_app_id, expected_app_id);
}

TEST_F(SyncedNotificationTest, GetCoalescingKeyTest) {
  std::string found_key = notification1->coalescing_key();
  std::string expected_key(kCoalescingKey1);

  EXPECT_EQ(found_key, expected_key);
}

TEST_F(SyncedNotificationTest, GetTitleTest) {
  std::string found_title = notification1->title();
  std::string expected_title(kTitle1);

  EXPECT_EQ(found_title, expected_title);
}

TEST_F(SyncedNotificationTest, GetIconURLTest) {
  std::string found_icon_url = notification1->icon_url().spec();
  std::string expected_icon_url(kIconUrl1);

  EXPECT_EQ(found_icon_url, expected_icon_url);
}

// TODO(petewil): Improve ctor to pass in an image and type so this test can
// pass on actual data.
TEST_F(SyncedNotificationTest, GetImageURLTest) {
  std::string found_image_url = notification1->image_url().spec();
  std::string expected_image_url;  // TODO(petewil): (kImageUrl1)

  EXPECT_EQ(found_image_url, expected_image_url);
}

// TODO(petewil): test with a multi-line body
TEST_F(SyncedNotificationTest, GetBodyTest) {
  std::string found_body = notification1->body();
  std::string expected_body(kBody1);

  EXPECT_EQ(found_body, expected_body);
}

TEST_F(SyncedNotificationTest, GetNotificationIdTest) {
  std::string found_id = notification1->notification_id();
  std::string expected_id(kAppId1);
  expected_id += "/";
  expected_id += kCoalescingKey1;

  EXPECT_EQ(found_id, expected_id);
}

// test that equals works as we expect
TEST_F(SyncedNotificationTest, EqualsTest) {
  EXPECT_TRUE(notification1->Equals(*notification1));
  EXPECT_TRUE(notification2->Equals(*notification2));
  EXPECT_FALSE(notification1->Equals(*notification2));
}

// Add a test for set_local_changes and has_local_changes together
// Add a test for a notification being read and or deleted
