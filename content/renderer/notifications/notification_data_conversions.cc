// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/notifications/notification_data_conversions.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationAction.h"

using blink::WebNotificationData;
using blink::WebString;

namespace content {

PlatformNotificationData ToPlatformNotificationData(
    const WebNotificationData& web_data) {
  PlatformNotificationData platform_data;
  platform_data.title = web_data.title.Utf16();

  switch (web_data.direction) {
    case WebNotificationData::kDirectionLeftToRight:
      platform_data.direction =
          PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT;
      break;
    case WebNotificationData::kDirectionRightToLeft:
      platform_data.direction =
          PlatformNotificationData::DIRECTION_RIGHT_TO_LEFT;
      break;
    case WebNotificationData::kDirectionAuto:
      platform_data.direction = PlatformNotificationData::DIRECTION_AUTO;
      break;
  }

  platform_data.lang = web_data.lang.Utf8(
      WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD);
  platform_data.body = web_data.body.Utf16();
  platform_data.tag = web_data.tag.Utf8(
      WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD);
  platform_data.image = WebStringToGURL(web_data.image.GetString());
  platform_data.icon = WebStringToGURL(web_data.icon.GetString());
  platform_data.badge = WebStringToGURL(web_data.badge.GetString());
  platform_data.vibration_pattern.assign(web_data.vibrate.begin(),
                                         web_data.vibrate.end());
  platform_data.timestamp = base::Time::FromJsTime(web_data.timestamp);
  platform_data.renotify = web_data.renotify;
  platform_data.silent = web_data.silent;
  platform_data.require_interaction = web_data.require_interaction;
  platform_data.data.assign(web_data.data.begin(), web_data.data.end());
  platform_data.actions.resize(web_data.actions.size());
  for (size_t i = 0; i < web_data.actions.size(); ++i) {
    switch (web_data.actions[i].type) {
      case blink::WebNotificationAction::kButton:
        platform_data.actions[i].type =
            PLATFORM_NOTIFICATION_ACTION_TYPE_BUTTON;
        break;
      case blink::WebNotificationAction::kText:
        platform_data.actions[i].type = PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT;
        break;
      default:
        NOTREACHED() << "Unknown notification action type: "
                     << web_data.actions[i].type;
    }
    platform_data.actions[i].action = web_data.actions[i].action.Utf8(
        WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD);
    platform_data.actions[i].title = web_data.actions[i].title.Utf16();
    platform_data.actions[i].icon =
        WebStringToGURL(web_data.actions[i].icon.GetString());
    platform_data.actions[i].placeholder =
        WebString::ToNullableString16(web_data.actions[i].placeholder);
  }

  return platform_data;
}

WebNotificationData ToWebNotificationData(
    const PlatformNotificationData& platform_data) {
  WebNotificationData web_data;
  web_data.title = WebString::FromUTF16(platform_data.title);

  switch (platform_data.direction) {
    case PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT:
      web_data.direction = WebNotificationData::kDirectionLeftToRight;
      break;
    case PlatformNotificationData::DIRECTION_RIGHT_TO_LEFT:
      web_data.direction = WebNotificationData::kDirectionRightToLeft;
      break;
    case PlatformNotificationData::DIRECTION_AUTO:
      web_data.direction = WebNotificationData::kDirectionAuto;
      break;
  }

  web_data.lang = WebString::FromUTF8(platform_data.lang);
  web_data.body = WebString::FromUTF16(platform_data.body);
  web_data.tag = WebString::FromUTF8(platform_data.tag);
  web_data.image = blink::WebURL(platform_data.image);
  web_data.icon = blink::WebURL(platform_data.icon);
  web_data.badge = blink::WebURL(platform_data.badge);
  web_data.vibrate = platform_data.vibration_pattern;
  web_data.timestamp = platform_data.timestamp.ToJsTime();
  web_data.renotify = platform_data.renotify;
  web_data.silent = platform_data.silent;
  web_data.require_interaction = platform_data.require_interaction;
  web_data.data = platform_data.data;
  blink::WebVector<blink::WebNotificationAction> resized(
      platform_data.actions.size());
  web_data.actions.Swap(resized);
  for (size_t i = 0; i < platform_data.actions.size(); ++i) {
    switch (platform_data.actions[i].type) {
      case PLATFORM_NOTIFICATION_ACTION_TYPE_BUTTON:
        web_data.actions[i].type = blink::WebNotificationAction::kButton;
        break;
      case PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT:
        web_data.actions[i].type = blink::WebNotificationAction::kText;
        break;
      default:
        NOTREACHED() << "Unknown platform data type: "
                     << platform_data.actions[i].type;
    }
    web_data.actions[i].action =
        WebString::FromUTF8(platform_data.actions[i].action);
    web_data.actions[i].title =
        WebString::FromUTF16(platform_data.actions[i].title);
    web_data.actions[i].icon = blink::WebURL(platform_data.actions[i].icon);
    web_data.actions[i].placeholder =
        WebString::FromUTF16(platform_data.actions[i].placeholder);
  }

  return web_data;
}

}  // namespace content
