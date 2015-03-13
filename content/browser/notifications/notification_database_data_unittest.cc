// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_database_data.h"

#include "base/strings/utf_string_conversions.h"
#include "content/browser/notifications/notification_database_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

const int64_t kNotificationId = 42;
const int64_t kServiceWorkerRegistrationId = 9001;

const char kOrigin[] = "https://example.com/";
const char kNotificationTitle[] = "My Notification";
const char kNotificationLang[] = "nl";
const char kNotificationBody[] = "Hello, world!";
const char kNotificationTag[] = "my_tag";
const char kNotificationIconUrl[] = "https://example.com/icon.png";

TEST(NotificationDatabaseDataTest, SerializeAndDeserializeData) {
  PlatformNotificationData notification_data;
  notification_data.title = base::ASCIIToUTF16(kNotificationTitle);
  notification_data.direction =
      PlatformNotificationData::NotificationDirectionRightToLeft;
  notification_data.lang = kNotificationLang;
  notification_data.body = base::ASCIIToUTF16(kNotificationBody);
  notification_data.tag = kNotificationTag;
  notification_data.icon = GURL(kNotificationIconUrl);
  notification_data.silent = true;

  NotificationDatabaseData database_data;
  database_data.notification_id = kNotificationId;
  database_data.origin = GURL(kOrigin);
  database_data.service_worker_registration_id = kServiceWorkerRegistrationId;
  database_data.notification_data = notification_data;

  std::string serialized_data;

  // Serialize the data in |notification_data| to the string |serialized_data|.
  ASSERT_TRUE(database_data.SerializeToString(&serialized_data));

  NotificationDatabaseData copied_data;

  // Deserialize the data in |serialized_data| to |copied_data|.
  ASSERT_TRUE(copied_data.ParseFromString(serialized_data));

  EXPECT_EQ(database_data.notification_id, copied_data.notification_id);
  EXPECT_EQ(database_data.origin, copied_data.origin);
  EXPECT_EQ(database_data.service_worker_registration_id,
            copied_data.service_worker_registration_id);

  const PlatformNotificationData& copied_notification_data =
      copied_data.notification_data;

  EXPECT_EQ(notification_data.title, copied_notification_data.title);
  EXPECT_EQ(notification_data.direction, copied_notification_data.direction);
  EXPECT_EQ(notification_data.lang, copied_notification_data.lang);
  EXPECT_EQ(notification_data.body, copied_notification_data.body);
  EXPECT_EQ(notification_data.tag, copied_notification_data.tag);
  EXPECT_EQ(notification_data.icon, copied_notification_data.icon);
  EXPECT_EQ(notification_data.silent, copied_notification_data.silent);
}

}  // namespace content
