// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/notifications/notification_struct_traits.h"

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/public/common/platform_notification_data.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/notifications/notification.mojom.h"
#include "url/gurl.h"

namespace content {

TEST(NotificationStructTraitsTest, NotificationDataRoundtrip) {
  PlatformNotificationData notification_data;
  notification_data.title = base::ASCIIToUTF16("Title of my notification");
  notification_data.direction =
      PlatformNotificationData::Direction::DIRECTION_AUTO;
  notification_data.lang = "test-lang";
  notification_data.body = base::ASCIIToUTF16("Notification body.");
  notification_data.tag = "notification-tag";
  notification_data.image = GURL("https://example.com/image.png");
  notification_data.icon = GURL("https://example.com/icon.png");
  notification_data.badge = GURL("https://example.com/badge.png");

  const int vibration_pattern[] = {500, 100, 30};
  notification_data.vibration_pattern.assign(
      vibration_pattern, vibration_pattern + arraysize(vibration_pattern));

  notification_data.timestamp = base::Time::FromJsTime(1513966159000.);
  notification_data.renotify = true;
  notification_data.silent = true;
  notification_data.require_interaction = true;

  const char data[] = "mock binary notification data";
  notification_data.data.assign(data, data + arraysize(data));

  notification_data.actions.resize(2);
  notification_data.actions[0].type = PLATFORM_NOTIFICATION_ACTION_TYPE_BUTTON;
  notification_data.actions[0].action = "buttonAction";
  notification_data.actions[0].title = base::ASCIIToUTF16("Button Title!");
  notification_data.actions[0].icon = GURL("https://example.com/aButton.png");
  notification_data.actions[0].placeholder = base::NullableString16();

  notification_data.actions[1].type = PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT;
  notification_data.actions[1].action = "textAction";
  notification_data.actions[1].title = base::ASCIIToUTF16("Reply Button Title");
  notification_data.actions[1].icon = GURL("https://example.com/reply.png");
  notification_data.actions[1].placeholder =
      base::NullableString16(base::ASCIIToUTF16("Placeholder Text"), false);

  PlatformNotificationData roundtrip_notification_data;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<blink::mojom::NotificationData>(
          &notification_data, &roundtrip_notification_data));

  EXPECT_EQ(roundtrip_notification_data.title, notification_data.title);
  EXPECT_EQ(roundtrip_notification_data.direction, notification_data.direction);
  EXPECT_EQ(roundtrip_notification_data.lang, notification_data.lang);
  EXPECT_EQ(roundtrip_notification_data.body, notification_data.body);
  EXPECT_EQ(roundtrip_notification_data.tag, notification_data.tag);
  EXPECT_EQ(roundtrip_notification_data.image, notification_data.image);
  EXPECT_EQ(roundtrip_notification_data.icon, notification_data.icon);
  EXPECT_EQ(roundtrip_notification_data.badge, notification_data.badge);
  EXPECT_EQ(roundtrip_notification_data.vibration_pattern,
            notification_data.vibration_pattern);
  EXPECT_EQ(roundtrip_notification_data.timestamp, notification_data.timestamp);
  EXPECT_EQ(roundtrip_notification_data.renotify, notification_data.renotify);
  EXPECT_EQ(roundtrip_notification_data.silent, notification_data.silent);
  EXPECT_EQ(roundtrip_notification_data.require_interaction,
            notification_data.require_interaction);
  EXPECT_EQ(roundtrip_notification_data.data, notification_data.data);
  ASSERT_EQ(notification_data.actions.size(),
            roundtrip_notification_data.actions.size());
  for (size_t i = 0; i < notification_data.actions.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Action index: %zd", i));
    EXPECT_EQ(notification_data.actions[i].type,
              roundtrip_notification_data.actions[i].type);
    EXPECT_EQ(notification_data.actions[i].action,
              roundtrip_notification_data.actions[i].action);
    EXPECT_EQ(notification_data.actions[i].title,
              roundtrip_notification_data.actions[i].title);
    EXPECT_EQ(notification_data.actions[i].icon,
              roundtrip_notification_data.actions[i].icon);
    EXPECT_EQ(notification_data.actions[i].placeholder,
              roundtrip_notification_data.actions[i].placeholder);
  }
}

// Check upper bound on vibration entries (99).
TEST(NotificationStructTraitsTest, ValidVibrationPattern) {
  constexpr int kEntries = 99;      // valid
  constexpr int kDurationMs = 999;  // valid

  PlatformNotificationData notification_data;
  notification_data.title =
      base::ASCIIToUTF16("Notification with 99 x 999ms entries (valid)");

  for (size_t i = 0; i < kEntries; ++i)
    notification_data.vibration_pattern.push_back(kDurationMs);

  PlatformNotificationData platform_notification_data;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<blink::mojom::NotificationData>(
          &notification_data, &platform_notification_data));
}

// Check round-trip fails when there are too many entries in the vibration
// pattern.
TEST(NotificationStructTraitsTest, TooManyVibrations) {
  constexpr int kEntries = 100;   // invalid
  constexpr int kDurationMs = 1;  // valid

  PlatformNotificationData notification_data;
  notification_data.title =
      base::ASCIIToUTF16("Notification with 100 x 1ms entries (invalid)");

  for (size_t i = 0; i < kEntries; ++i)
    notification_data.vibration_pattern.push_back(kDurationMs);

  PlatformNotificationData platform_notification_data;

  ASSERT_FALSE(
      mojo::test::SerializeAndDeserialize<blink::mojom::NotificationData>(
          &notification_data, &platform_notification_data));
}

// Check round-trip fails when there is a too-long vibration duration.
TEST(NotificationStructTraitsTest, TooLongVibrationDuration) {
  constexpr int kEntries = 1;         // valid
  constexpr int kDurationMs = 10001;  // invalid (>10 seconds)

  PlatformNotificationData notification_data;
  notification_data.title =
      base::ASCIIToUTF16("Notification with 1 x 10001ms entries (invalid)");

  for (size_t i = 0; i < kEntries; ++i)
    notification_data.vibration_pattern.push_back(kDurationMs);

  PlatformNotificationData platform_notification_data;

  ASSERT_FALSE(
      mojo::test::SerializeAndDeserialize<blink::mojom::NotificationData>(
          &notification_data, &platform_notification_data));
}

// Check round-trip fails when there are too many actions provided.
TEST(NotificationStructTraitsTest, TooManyActions) {
  constexpr int kActions = 3;  // invalid (max is 2)

  PlatformNotificationData notification_data;
  notification_data.title =
      base::ASCIIToUTF16("Notification with 3 actions provided (invalid)");

  notification_data.actions.resize(kActions);
  for (size_t i = 0; i < kActions; ++i) {
    notification_data.actions[i].title = base::ASCIIToUTF16("action title");
  }

  PlatformNotificationData platform_notification_data;

  ASSERT_FALSE(
      mojo::test::SerializeAndDeserialize<blink::mojom::NotificationData>(
          &notification_data, &platform_notification_data));
}

// Check round-trip fails when the data size is too big.
TEST(NotificationStructTraitsTest, DataExceedsMaximumSize) {
  constexpr size_t kDataSize = 1024 * 1024 + 1;  // 1 more than max data size.

  PlatformNotificationData notification_data;
  notification_data.title =
      base::ASCIIToUTF16("Notification with too much data");

  notification_data.data.resize(kDataSize);

  PlatformNotificationData platform_notification_data;

  ASSERT_FALSE(
      mojo::test::SerializeAndDeserialize<blink::mojom::NotificationData>(
          &notification_data, &platform_notification_data));
}
}  // namespace content
