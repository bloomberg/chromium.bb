// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_data_conversions.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/common/platform_notification_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationData.h"

namespace content {
namespace {

const char kNotificationTitle[] = "My Notification";
const char kNotificationLang[] = "nl";
const char kNotificationBody[] = "Hello, world!";
const char kNotificationTag[] = "my_tag";
const char kNotificationIconUrl[] = "https://example.com/icon.png";

}

TEST(NotificationDataConversionsTest, ToPlatformNotificationData) {
  blink::WebNotificationData web_data(
      blink::WebString::fromUTF8(kNotificationTitle),
      blink::WebNotificationData::DirectionLeftToRight,
      blink::WebString::fromUTF8(kNotificationLang),
      blink::WebString::fromUTF8(kNotificationBody),
      blink::WebString::fromUTF8(kNotificationTag),
      blink::WebURL(GURL(kNotificationIconUrl)),
      true /* silent */);

  PlatformNotificationData platform_data = ToPlatformNotificationData(web_data);
  EXPECT_EQ(base::ASCIIToUTF16(kNotificationTitle), platform_data.title);
  EXPECT_EQ(PlatformNotificationData::NotificationDirectionLeftToRight,
            platform_data.direction);
  EXPECT_EQ(kNotificationLang, platform_data.lang);
  EXPECT_EQ(base::ASCIIToUTF16(kNotificationBody), platform_data.body);
  EXPECT_EQ(kNotificationTag, platform_data.tag);
  EXPECT_EQ(kNotificationIconUrl, platform_data.icon.spec());
  EXPECT_TRUE(platform_data.silent);
}

TEST(NotificationDataConversionsTest,
     ToPlatformNotificationDataDirectionality) {
  blink::WebNotificationData web_data;
  web_data.direction = blink::WebNotificationData::DirectionLeftToRight;

  PlatformNotificationData platform_data = ToPlatformNotificationData(web_data);
  EXPECT_EQ(PlatformNotificationData::NotificationDirectionLeftToRight,
            platform_data.direction);

  web_data.direction = blink::WebNotificationData::DirectionRightToLeft;

  platform_data = ToPlatformNotificationData(web_data);
  EXPECT_EQ(PlatformNotificationData::NotificationDirectionRightToLeft,
            platform_data.direction);
}

TEST(NotificationDataConversionsTest, ToWebNotificationData) {
  PlatformNotificationData platform_data;
  platform_data.title = base::ASCIIToUTF16(kNotificationTitle);
  platform_data.direction =
      PlatformNotificationData::NotificationDirectionLeftToRight;
  platform_data.lang = kNotificationLang;
  platform_data.body = base::ASCIIToUTF16(kNotificationBody);
  platform_data.tag = kNotificationTag;
  platform_data.icon = GURL(kNotificationIconUrl);
  platform_data.silent = true;

  blink::WebNotificationData web_data = ToWebNotificationData(platform_data);
  EXPECT_EQ(kNotificationTitle, web_data.title);
  EXPECT_EQ(blink::WebNotificationData::DirectionLeftToRight,
            web_data.direction);
  EXPECT_EQ(kNotificationLang, web_data.lang);
  EXPECT_EQ(kNotificationBody, web_data.body);
  EXPECT_EQ(kNotificationTag, web_data.tag);
  EXPECT_EQ(kNotificationIconUrl, web_data.icon.string());
  EXPECT_TRUE(web_data.silent);
}

TEST(NotificationDataConversionsTest, ToWebNotificationDataDirectionality) {
  PlatformNotificationData platform_data;
  platform_data.direction =
      PlatformNotificationData::NotificationDirectionLeftToRight;

  blink::WebNotificationData web_data = ToWebNotificationData(platform_data);
  EXPECT_EQ(blink::WebNotificationData::DirectionLeftToRight,
            web_data.direction);

  platform_data.direction =
      PlatformNotificationData::NotificationDirectionRightToLeft;

  web_data = ToWebNotificationData(platform_data);
  EXPECT_EQ(blink::WebNotificationData::DirectionRightToLeft,
            web_data.direction);
}

}  // namespace content
