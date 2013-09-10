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

bool UseRichNotifications() {
  return message_center::IsRichNotificationEnabled();
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

class SyncedNotificationTest : public testing::Test {
 public:
  SyncedNotificationTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {}
  virtual ~SyncedNotificationTest() {}

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
  base::MessageLoopForIO message_loop_;
  content::TestBrowserThread ui_thread_;

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

// TODO(petewil): test with a multi-line message
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
  notification5.reset(new SyncedNotification(sync_data1_));

  // update with the sync data from notification2, and ensure they are equal.
  notification5->Update(sync_data2_);
  EXPECT_TRUE(notification5->EqualsIgnoringReadState(*notification2_));
  EXPECT_EQ(notification5->GetReadState(), notification2_->GetReadState());
  EXPECT_FALSE(notification5->EqualsIgnoringReadState(*notification1_));
}

TEST_F(SyncedNotificationTest, ShowTest) {

  if (!UseRichNotifications())
    return;

  StubNotificationUIManager notification_manager;

  // Call the method under test using the pre-populated data.
  notification1_->Show(&notification_manager, NULL, NULL);

  const Notification notification = notification_manager.notification();

  // Check the base fields of the notification.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_IMAGE, notification.type());
  EXPECT_EQ(std::string(kTitle1), UTF16ToUTF8(notification.title()));
  EXPECT_EQ(std::string(kText1), UTF16ToUTF8(notification.message()));
  EXPECT_EQ(std::string(kExpectedOriginUrl), notification.origin_url().spec());
  EXPECT_EQ(std::string(kKey1), UTF16ToUTF8(notification.replace_id()));

  EXPECT_EQ(kFakeCreationTime, notification.timestamp().ToDoubleT());
  EXPECT_EQ(kNotificationPriority, notification.priority());
}

TEST_F(SyncedNotificationTest, DismissTest) {

  if (!UseRichNotifications())
    return;

  StubNotificationUIManager notification_manager;

  // Call the method under test using a dismissed notification.
  notification4_->Show(&notification_manager, NULL, NULL);

  EXPECT_EQ(std::string(kKey1), notification_manager.dismissed_id());
}

TEST_F(SyncedNotificationTest, AddBitmapToFetchQueueTest) {
  scoped_ptr<SyncedNotification> notification6;
  notification6.reset(new SyncedNotification(sync_data1_));

  // Add two bitmaps to the queue.
  notification6->AddBitmapToFetchQueue(GURL(kIconUrl1));
  notification6->AddBitmapToFetchQueue(GURL(kIconUrl2));

  EXPECT_EQ(2, notification6->active_fetcher_count_);
  EXPECT_EQ(GURL(kIconUrl1), notification6->fetchers_[0]->url());
  EXPECT_EQ(GURL(kIconUrl2), notification6->fetchers_[1]->url());

  notification6->AddBitmapToFetchQueue(GURL(kIconUrl2));
  EXPECT_EQ(2, notification6->active_fetcher_count_);
}

TEST_F(SyncedNotificationTest, OnFetchCompleteTest) {
  if (!UseRichNotifications())
    return;

  StubNotificationUIManager notification_manager;

  // Set up the internal state that FetchBitmaps() would have set.
  notification1_->notification_manager_ = &notification_manager;

  // Add two bitmaps to the queue for us to match up.
  notification1_->AddBitmapToFetchQueue(GURL(kIconUrl1));
  notification1_->AddBitmapToFetchQueue(GURL(kIconUrl2));
  EXPECT_EQ(2, notification1_->active_fetcher_count_);

  // Put some realistic looking bitmap data into the url_fetcher.
  SkBitmap bitmap;

  // Put a real bitmap into "bitmap".  2x2 bitmap of green 32 bit pixels.
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, 2, 2);
  bitmap.allocPixels();
  bitmap.eraseColor(SK_ColorGREEN);

  notification1_->OnFetchComplete(GURL(kIconUrl1), &bitmap);
  EXPECT_EQ(1, notification1_->active_fetcher_count_);

  // When we call OnFetchComplete on the second bitmap, show should be called.
  notification1_->OnFetchComplete(GURL(kIconUrl2), &bitmap);
  EXPECT_EQ(0, notification1_->active_fetcher_count_);

  // Since we check Show() thoroughly in its own test, we only check cursorily.
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_IMAGE,
            notification_manager.notification().type());
  EXPECT_EQ(std::string(kTitle1),
            UTF16ToUTF8(notification_manager.notification().title()));
  EXPECT_EQ(std::string(kText1),
            UTF16ToUTF8(notification_manager.notification().message()));

  // TODO(petewil): Check that the bitmap in the notification is what we expect.
  // This fails today, the type info is different.
  // EXPECT_TRUE(gfx::BitmapsAreEqual(
  //     image, notification1_->GetAppIconBitmap()));
}

TEST_F(SyncedNotificationTest, QueueBitmapFetchJobsTest) {
  if (!UseRichNotifications())
    return;

  StubNotificationUIManager notification_manager;

  notification1_->QueueBitmapFetchJobs(&notification_manager, NULL, NULL);

  // There should be 4 urls in the queue, icon, image, and two buttons.
  EXPECT_EQ(4, notification1_->active_fetcher_count_);
}

// TODO(petewil): Add a test for a notification being read and or deleted.

}  // namespace notifier
