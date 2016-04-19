// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_conversion_helper.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/common/extensions/api/notifications.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "url/gurl.h"

class NotificationConversionHelperTest : public testing::Test {
 public:
  NotificationConversionHelperTest() {}

  void SetUp() override {}

  void TearDown() override {}

 protected:
  std::unique_ptr<Notification> CreateNotification(
      message_center::NotificationType type) {
    message_center::RichNotificationData optional_fields;
    optional_fields.priority = 1;
    optional_fields.context_message =
        base::UTF8ToUTF16("I am a context message.");
    optional_fields.timestamp = base::Time::FromDoubleT(12345678.9);
    optional_fields.buttons.push_back(
        message_center::ButtonInfo(base::UTF8ToUTF16("Button 1")));
    optional_fields.buttons.push_back(
        message_center::ButtonInfo(base::UTF8ToUTF16("Button 2")));
    optional_fields.clickable = false;

    if (type == message_center::NOTIFICATION_TYPE_IMAGE)
      optional_fields.image = gfx::Image();
    if (type == message_center::NOTIFICATION_TYPE_MULTIPLE) {
      optional_fields.items.push_back(message_center::NotificationItem(
          base::UTF8ToUTF16("Item 1 Title"),
          base::UTF8ToUTF16("Item 1 Message")));
      optional_fields.items.push_back(message_center::NotificationItem(
          base::UTF8ToUTF16("Item 2 Title"),
          base::UTF8ToUTF16("Item 2 Message")));
    }
    if (type == message_center::NOTIFICATION_TYPE_PROGRESS)
      optional_fields.progress = 50;

    NotificationDelegate* delegate(new MockNotificationDelegate("id1"));

    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    bitmap.eraseColor(SkColorSetRGB(1, 2, 3));
    gfx::Image icon = gfx::Image::CreateFrom1xBitmap(bitmap);

    std::unique_ptr<Notification> notification(new Notification(
        type, base::UTF8ToUTF16("Title"),
        base::UTF8ToUTF16("This is a message."), icon,
        message_center::NotifierId(message_center::NotifierId::APPLICATION,
                                   "Notifier 1"),
        base::UTF8ToUTF16("Notifier's Name"), GURL(), "id1", optional_fields,
        delegate));

    return notification;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationConversionHelperTest);
};

TEST_F(NotificationConversionHelperTest, NotificationToNotificationOptions) {
  // Create a notification of image type
  std::unique_ptr<Notification> notification1 =
      CreateNotification(message_center::NOTIFICATION_TYPE_IMAGE);
  std::unique_ptr<extensions::api::notifications::NotificationOptions> options1(
      new extensions::api::notifications::NotificationOptions());
  NotificationConversionHelper::NotificationToNotificationOptions(
      *(notification1), options1.get());

  EXPECT_EQ(options1->type,
            extensions::api::notifications::TEMPLATE_TYPE_IMAGE);
  EXPECT_EQ(*(options1->title), "Title");
  EXPECT_EQ(*(options1->message), "This is a message.");
  EXPECT_EQ(*(options1->priority), 1);
  EXPECT_EQ(*(options1->context_message), "I am a context message.");
  EXPECT_FALSE(*(options1->is_clickable));
  EXPECT_EQ(*(options1->event_time), 12345678.9);
  EXPECT_EQ(options1->buttons->at(0).title, "Button 1");
  EXPECT_EQ(options1->buttons->at(1).title, "Button 2");

  EXPECT_EQ(options1->icon_bitmap->width, 1);
  EXPECT_EQ(options1->icon_bitmap->height, 1);

  // Create a notification of progress type
  std::unique_ptr<Notification> notification2 =
      CreateNotification(message_center::NOTIFICATION_TYPE_PROGRESS);
  std::unique_ptr<extensions::api::notifications::NotificationOptions> options2(
      new extensions::api::notifications::NotificationOptions());
  NotificationConversionHelper::NotificationToNotificationOptions(
      *(notification2), options2.get());
  EXPECT_EQ(options2->type,
            extensions::api::notifications::TEMPLATE_TYPE_PROGRESS);
  EXPECT_EQ(*(options2->progress), 50);

  // Create a notification of multiple type
  std::unique_ptr<Notification> notification3 =
      CreateNotification(message_center::NOTIFICATION_TYPE_MULTIPLE);
  std::unique_ptr<extensions::api::notifications::NotificationOptions> options3(
      new extensions::api::notifications::NotificationOptions());
  NotificationConversionHelper::NotificationToNotificationOptions(
      *(notification3), options3.get());
  EXPECT_EQ(options3->type, extensions::api::notifications::TEMPLATE_TYPE_LIST);
  EXPECT_EQ(options3->items->at(0).title, "Item 1 Title");
  EXPECT_EQ(options3->items->at(0).message, "Item 1 Message");
  EXPECT_EQ(options3->items->at(1).title, "Item 2 Title");
  EXPECT_EQ(options3->items->at(1).message, "Item 2 Message");
}
