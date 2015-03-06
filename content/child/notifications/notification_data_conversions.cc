// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_data_conversions.h"

#include "base/strings/utf_string_conversions.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebNotificationData;

namespace content {

PlatformNotificationData ToPlatformNotificationData(
    const WebNotificationData& web_data) {
  PlatformNotificationData platform_data;
  platform_data.title = web_data.title;
  platform_data.direction =
      web_data.direction == WebNotificationData::DirectionLeftToRight
          ? PlatformNotificationData::NotificationDirectionLeftToRight
          : PlatformNotificationData::NotificationDirectionRightToLeft;
  platform_data.lang = base::UTF16ToUTF8(web_data.lang);
  platform_data.body = web_data.body;
  platform_data.tag = web_data.tag;
  platform_data.icon = GURL(web_data.icon.string());
  platform_data.silent = web_data.silent;

  return platform_data;
}

WebNotificationData ToWebNotificationData(
    const PlatformNotificationData& platform_data) {
  WebNotificationData web_data;
  web_data.title = platform_data.title;
  web_data.direction =
      platform_data.direction ==
          PlatformNotificationData::NotificationDirectionLeftToRight
          ? WebNotificationData::DirectionLeftToRight
          : WebNotificationData::DirectionRightToLeft;
  web_data.lang = blink::WebString::fromUTF8(platform_data.lang);
  web_data.body = platform_data.body;
  web_data.tag = platform_data.tag;
  web_data.icon = blink::WebURL(platform_data.icon);
  web_data.silent = platform_data.silent;

  return web_data;
}

}  // namespace content
