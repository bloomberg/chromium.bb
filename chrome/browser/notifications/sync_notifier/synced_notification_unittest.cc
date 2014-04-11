// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/sync_notifier/sync_notifier_test_utils.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/notification_types.h"

using syncer::SyncData;
using notifier::SyncedNotification;
using sync_pb::EntitySpecifics;
using sync_pb::SyncedNotificationSpecifics;

namespace {
const int kNotificationPriority = static_cast<int>(
    message_center::LOW_PRIORITY);

}  // namespace

namespace notifier {

class SyncedNotificationTest : public testing::Test {
 public:
  SyncedNotificationTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {}
  virtual ~SyncedNotificationTest() {}

  // Methods from testing::Test.

  virtual void SetUp() OVERRIDE {
    notification_manager_.reset(new StubNotificationUIManager(GURL(
        kSyncedNotificationsWelcomeOrigin)));

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

    notification1_.reset(new SyncedNotification(
        sync_data1_, NULL, notification_manager_.get()));
    notification2_.reset(new SyncedNotification(
        sync_data2_, NULL, notification_manager_.get()));
    notification3_.reset(new SyncedNotification(
        sync_data3_, NULL, notification_manager_.get()));
    notification4_.reset(new SyncedNotification(
        sync_data4_, NULL, notification_manager_.get()));

  }

  virtual void TearDown() OVERRIDE {
    notification_manager_.reset();
  }

  virtual void AddButtonBitmaps(SyncedNotification* notification,
                                unsigned int how_many) {
    for (unsigned int i = 0; i < how_many; ++i) {
      notification->button_bitmaps_.push_back(gfx::Image());
      notification->button_bitmaps_fetch_pending_.push_back(true);
    }
  }

  StubNotificationUIManager* notification_manager() {
    return notification_manager_.get();
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
  base::MessageLoopForIO message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<StubNotificationUIManager> notification_manager_;

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
  GURL found_image_url = notification1_->GetImageUrl();
  GURL expected_image_url = GURL(kImageUrl1);

  EXPECT_EQ(expected_image_url, found_image_url);
}

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
    GURL default_destination_icon_url =
        notification1_->GetDefaultDestinationIconUrl();
    GURL default_destination_url =
        notification1_->GetDefaultDestinationUrl();
    EXPECT_EQ(std::string(kDefaultDestinationTitle), default_destination_title);
    EXPECT_EQ(GURL(kDefaultDestinationIconUrl),
              default_destination_icon_url);
    EXPECT_EQ(GURL(kDefaultDestinationUrl), default_destination_url);
}

TEST_F(SyncedNotificationTest, GetButtonDataTest) {
    std::string button_one_title = notification1_->GetButtonTitle(0);
    GURL button_one_icon_url = notification1_->GetButtonIconUrl(0);
    GURL button_one_url = notification1_->GetButtonUrl(0);
    std::string button_two_title = notification1_->GetButtonTitle(1);
    GURL button_two_icon_url = notification1_->GetButtonIconUrl(1);
    GURL button_two_url = notification1_->GetButtonUrl(1);
    EXPECT_EQ(std::string(kButtonOneTitle), button_one_title);
    EXPECT_EQ(GURL(kButtonOneIconUrl), button_one_icon_url);
    EXPECT_EQ(GURL(kButtonOneUrl), button_one_url);
    EXPECT_EQ(std::string(kButtonTwoTitle), button_two_title);
    EXPECT_EQ(GURL(kButtonTwoIconUrl), button_two_icon_url);
    EXPECT_EQ(GURL(kButtonTwoUrl), button_two_url);
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
  notification5.reset(new SyncedNotification(
      sync_data1_, NULL, notification_manager()));

  // update with the sync data from notification2, and ensure they are equal.
  notification5->Update(sync_data2_);
  EXPECT_TRUE(notification5->EqualsIgnoringReadState(*notification2_));
  EXPECT_EQ(notification5->GetReadState(), notification2_->GetReadState());
  EXPECT_FALSE(notification5->EqualsIgnoringReadState(*notification1_));
}

TEST_F(SyncedNotificationTest, ShowTest) {
  // Call the method under test using the pre-populated data.
  notification1_->Show(NULL);

  const Notification notification = notification_manager()->notification();

  // Check the base fields of the notification.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_IMAGE, notification.type());
  EXPECT_EQ(std::string(kTitle1), base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ(std::string(kText1), base::UTF16ToUTF8(notification.message()));
  EXPECT_EQ(std::string(kExpectedOriginUrl), notification.origin_url().spec());
  EXPECT_EQ(std::string(kKey1), base::UTF16ToUTF8(notification.replace_id()));

  EXPECT_EQ(kFakeCreationTime, notification.timestamp().ToDoubleT());
  EXPECT_EQ(kNotificationPriority, notification.priority());
}

TEST_F(SyncedNotificationTest, DismissTest) {

  // Call the method under test using a dismissed notification.
  notification4_->Show(NULL);

  EXPECT_EQ(std::string(kKey1), notification_manager()->dismissed_id());
}

TEST_F(SyncedNotificationTest, CreateBitmapFetcherTest) {
  scoped_ptr<SyncedNotification> notification6;
  notification6.reset(new SyncedNotification(
      sync_data1_, NULL, notification_manager()));

  // Add two bitmaps to the queue.
  notification6->CreateBitmapFetcher(GURL(kIconUrl1));
  notification6->CreateBitmapFetcher(GURL(kIconUrl2));

  EXPECT_EQ(GURL(kIconUrl1), notification6->fetchers_[0]->url());
  EXPECT_EQ(GURL(kIconUrl2), notification6->fetchers_[1]->url());

  notification6->CreateBitmapFetcher(GURL(kIconUrl2));
}

TEST_F(SyncedNotificationTest, OnFetchCompleteTest) {
  // Set up the internal state that FetchBitmaps() would have set.
  notification1_->notification_manager_ = notification_manager();

  // Add the bitmaps to the queue for us to match up.
  notification1_->CreateBitmapFetcher(GURL(kIconUrl1));
  notification1_->CreateBitmapFetcher(GURL(kImageUrl1));
  notification1_->CreateBitmapFetcher(GURL(kButtonOneIconUrl));
  notification1_->CreateBitmapFetcher(GURL(kButtonTwoIconUrl));

  // Put some realistic looking bitmap data into the url_fetcher.
  SkBitmap bitmap;

  // Put a real bitmap into "bitmap".  2x2 bitmap of green 32 bit pixels.
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 2, 2);
  bitmap.allocPixels();
  bitmap.eraseColor(SK_ColorGREEN);

  // Allocate the button_bitmaps_ array as the calling function normally would.
  AddButtonBitmaps(notification1_.get(), 2);

  notification1_->OnFetchComplete(GURL(kIconUrl1), &bitmap);

  // When we call OnFetchComplete on the last bitmap, show should be called.
  notification1_->OnFetchComplete(GURL(kImageUrl1), &bitmap);

  notification1_->OnFetchComplete(GURL(kButtonOneIconUrl), &bitmap);

  notification1_->OnFetchComplete(GURL(kButtonTwoIconUrl), &bitmap);

  // Expect that the app icon has some data in it.
  EXPECT_FALSE(notification1_->GetAppIcon().IsEmpty());
  EXPECT_FALSE(notification_manager()->notification().small_image().IsEmpty());

  // Since we check Show() thoroughly in its own test, we only check cursorily.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_IMAGE,
            notification_manager()->notification().type());
  EXPECT_EQ(std::string(kTitle1),
            base::UTF16ToUTF8(notification_manager()->notification().title()));
  EXPECT_EQ(
      std::string(kText1),
      base::UTF16ToUTF8(notification_manager()->notification().message()));

  // TODO(petewil): Check that the bitmap in the notification is what we expect.
  // This fails today, the type info is different.
  // EXPECT_TRUE(gfx::BitmapsAreEqual(
  //     image, notification1_->GetAppIconBitmap()));
}

// TODO(petewil): Empty bitmap should count as a successful fetch.
TEST_F(SyncedNotificationTest, EmptyBitmapTest) {
  // Set up the internal state that FetchBitmaps() would have set.
  notification1_->notification_manager_ = notification_manager();

  // Add the bitmaps to the queue for us to match up.
  notification1_->CreateBitmapFetcher(GURL(kIconUrl1));
  notification1_->CreateBitmapFetcher(GURL(kImageUrl1));
  notification1_->CreateBitmapFetcher(GURL(kButtonOneIconUrl));
  notification1_->CreateBitmapFetcher(GURL(kButtonTwoIconUrl));

  // Put some realistic looking bitmap data into the url_fetcher.
  SkBitmap bitmap;
  SkBitmap empty_bitmap;

  // Put a real bitmap into "bitmap".  2x2 bitmap of green 32 bit pixels.
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 2, 2);
  bitmap.allocPixels();
  bitmap.eraseColor(SK_ColorGREEN);

  // Put a null bitmap into "bitmap".  2x2 bitmap of green 32 bit pixels.
  empty_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 0, 0);
  empty_bitmap.allocPixels();
  empty_bitmap.eraseColor(SK_ColorGREEN);

  // Allocate the button_bitmaps_ array as the calling function normally would.
  AddButtonBitmaps(notification1_.get(), 2);

  notification1_->OnFetchComplete(GURL(kIconUrl1), &bitmap);

  // When we call OnFetchComplete on the last bitmap, show should be called.
  notification1_->OnFetchComplete(GURL(kImageUrl1), &bitmap);

  notification1_->OnFetchComplete(GURL(kButtonOneIconUrl), &empty_bitmap);

  notification1_->OnFetchComplete(GURL(kButtonTwoIconUrl), NULL);

  // Since we check Show() thoroughly in its own test, we only check cursorily.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_IMAGE,
            notification_manager()->notification().type());
  EXPECT_EQ(std::string(kTitle1),
            base::UTF16ToUTF8(notification_manager()->notification().title()));
  EXPECT_EQ(
      std::string(kText1),
      base::UTF16ToUTF8(notification_manager()->notification().message()));
}

TEST_F(SyncedNotificationTest, ShowIfNewlyEnabledTest) {
  // Call the method using the wrong app id, nothing should get shown.
  notification1_->ShowAllForAppId(NULL, kAppId2);

  // Ensure no notification was generated and shown.
  const Notification notification1 = notification_manager()->notification();
  EXPECT_EQ(std::string(), base::UTF16ToUTF8(notification1.replace_id()));

  // Call the method under test using the pre-populated data.
  notification1_->ShowAllForAppId(NULL, kAppId1);

  const Notification notification2 = notification_manager()->notification();

  // Check the base fields of the notification.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_IMAGE, notification2.type());
  EXPECT_EQ(std::string(kTitle1), base::UTF16ToUTF8(notification2.title()));
  EXPECT_EQ(std::string(kText1), base::UTF16ToUTF8(notification2.message()));
  EXPECT_EQ(std::string(kExpectedOriginUrl), notification2.origin_url().spec());
  EXPECT_EQ(std::string(kKey1), base::UTF16ToUTF8(notification2.replace_id()));

  EXPECT_EQ(kFakeCreationTime, notification2.timestamp().ToDoubleT());
  EXPECT_EQ(kNotificationPriority, notification2.priority());
}

TEST_F(SyncedNotificationTest, HideIfNewlyRemovedTest) {
  // Add the notification to the notification manger, so it exists before we
  // we remove it.
  notification1_->Show(NULL);
  const Notification* found1 = notification_manager()->FindById(kKey1);
  EXPECT_NE(reinterpret_cast<Notification*>(NULL), found1);

  // Call the method under test using the pre-populated data.
  notification1_->HideAllForAppId(kAppId1);

  // Ensure the notification was removed from the notification manager
  EXPECT_EQ(std::string(kKey1), notification_manager()->dismissed_id());
}

// TODO(petewil): Add a test for a notification being read and or deleted.

}  // namespace notifier
