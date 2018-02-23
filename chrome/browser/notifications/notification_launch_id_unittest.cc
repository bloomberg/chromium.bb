// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_launch_id.h"

#include <windows.h>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NotificationLaunchIdTest, SerializationTests) {
  NotificationLaunchId id(NotificationHandler::Type::WEB_PERSISTENT,
                          "notification_id", "Default", true,
                          GURL("https://example.com"));
  EXPECT_TRUE(id.is_valid());
  EXPECT_EQ("0|0|Default|1|https://example.com/|notification_id",
            id.Serialize());
}

TEST(NotificationLaunchIdTest, ParsingTests) {
  // Input string as Windows passes it to us when you click on the notification.
  {
    std::string encoded = "0|0|Default|1|https://example.com/|notification_id";
    NotificationLaunchId id(encoded);

    EXPECT_TRUE(id.is_valid());
    EXPECT_EQ(-1, id.button_index());
    EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT,
              id.notification_type());
    EXPECT_TRUE(id.incognito());
    EXPECT_EQ("Default", id.profile_id());
    EXPECT_EQ("notification_id", id.notification_id());
  }

  // Extra pipe signs should be treated as part of the notification id.
  {
    std::string encoded =
        "0|0|Default|1|https://example.com/|notification_id|Extra|Data";
    NotificationLaunchId id(encoded);

    EXPECT_TRUE(id.is_valid());
    EXPECT_EQ(-1, id.button_index());
    EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT,
              id.notification_type());
    EXPECT_TRUE(id.incognito());
    EXPECT_EQ("Default", id.profile_id());
    EXPECT_EQ("notification_id|Extra|Data", id.notification_id());
  }

  // Input string for when a button is pressed.
  {
    std::string encoded =
        "1|0|0|Default|1|https://example.com/|notification_id";
    NotificationLaunchId id(encoded);

    EXPECT_TRUE(id.is_valid());
    EXPECT_EQ(0, id.button_index());
    EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT,
              id.notification_type());
    EXPECT_TRUE(id.incognito());
    EXPECT_EQ("Default", id.profile_id());
    EXPECT_EQ("notification_id", id.notification_id());
  }

  // Input string for when a button is pressed, with extra pipes in notification
  // id.
  {
    std::string encoded =
        "1|0|0|Default|1|https://example.com/"
        "|notification_id|Extra|Data|";
    NotificationLaunchId id(encoded);

    EXPECT_TRUE(id.is_valid());
    EXPECT_EQ(0, id.button_index());
    EXPECT_EQ(NotificationHandler::Type::WEB_PERSISTENT,
              id.notification_type());
    EXPECT_TRUE(id.incognito());
    EXPECT_EQ("Default", id.profile_id());
    EXPECT_EQ("notification_id|Extra|Data|", id.notification_id());
  }
}

TEST(NotificationLaunchIdTest, ParsingErrorCases) {
  struct TestCases {
    const char* const encoded_string;
  } cases[] = {
      {""},
      // Missing button index/notification type.
      {"1|0|Default|1|https://example.com/|notification_id"},
      // Valid, except button index is not an int.
      {"1|a|0|Default|1|https://example.com/|notification_id"},
      // Missing notification id from end.
      {"0|0|Default|1|https://example.com/"},
      // Missing notification id, and origin.
      {"0|0|Default|1"},
      // Missing notification id, origin, and incognito.
      {"0|0|Default"},
      // Missing notification id, origin, incognito, and profile id.
      {"0|0"},
      // Missing all but the component type (type NORMAL).
      {"0"},
      // Missing all but the component type (type BUTTON_INDEX).
      {"1"},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.encoded_string);
    NotificationLaunchId id(test_case.encoded_string);
    EXPECT_FALSE(id.is_valid());
  }
}
