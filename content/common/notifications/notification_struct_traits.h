// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NOTIFICATIONS_NOTIFICATION_STRUCT_TRAITS_H_
#define CONTENT_COMMON_NOTIFICATIONS_NOTIFICATION_STRUCT_TRAITS_H_

#include "base/containers/span.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/public/common/platform_notification_data.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "mojo/common/string16_struct_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/WebKit/public/platform/modules/notifications/notification.mojom.h"
#include "url/gurl.h"
#include "url/mojo/url_gurl_struct_traits.h"

namespace mojo {

template <>
struct CONTENT_EXPORT EnumTraits<blink::mojom::NotificationDirection,
                                 content::PlatformNotificationData::Direction> {
  static blink::mojom::NotificationDirection ToMojom(
      content::PlatformNotificationData::Direction input);

  static bool FromMojom(blink::mojom::NotificationDirection input,
                        content::PlatformNotificationData::Direction* out);
};

template <>
struct CONTENT_EXPORT EnumTraits<blink::mojom::NotificationActionType,
                                 content::PlatformNotificationActionType> {
  static blink::mojom::NotificationActionType ToMojom(
      content::PlatformNotificationActionType input);

  static bool FromMojom(blink::mojom::NotificationActionType input,
                        content::PlatformNotificationActionType* out);
};

template <>
struct CONTENT_EXPORT StructTraits<blink::mojom::NotificationActionDataView,
                                   content::PlatformNotificationAction> {
  static content::PlatformNotificationActionType type(
      const content::PlatformNotificationAction& action) {
    return action.type;
  }

  static const std::string& action(
      const content::PlatformNotificationAction& action) {
    return action.action;
  }

  static const base::string16& title(
      const content::PlatformNotificationAction& action) {
    return action.title;
  }

  static const GURL& icon(const content::PlatformNotificationAction& action) {
    return action.icon;
  }

  static const base::Optional<base::string16>& placeholder(
      const content::PlatformNotificationAction& action) {
    return action.placeholder.as_optional_string16();
  }

  static bool Read(
      blink::mojom::NotificationActionDataView notification_action,
      content::PlatformNotificationAction* platform_notification_action);
};

template <>
struct CONTENT_EXPORT StructTraits<blink::mojom::NotificationDataDataView,
                                   content::PlatformNotificationData> {
  static const base::string16& title(
      const content::PlatformNotificationData& data) {
    return data.title;
  }

  static content::PlatformNotificationData::Direction direction(
      const content::PlatformNotificationData& data) {
    return data.direction;
  }

  static const std::string& lang(
      const content::PlatformNotificationData& data) {
    return data.lang;
  }

  static const base::string16& body(
      const content::PlatformNotificationData& data) {
    return data.body;
  }

  static const std::string& tag(const content::PlatformNotificationData& data) {
    return data.tag;
  }

  static const GURL& image(const content::PlatformNotificationData& data) {
    return data.image;
  }

  static const GURL& icon(const content::PlatformNotificationData& data) {
    return data.icon;
  }

  static const GURL& badge(const content::PlatformNotificationData& data) {
    return data.badge;
  }

  static const base::span<const int32_t> vibration_pattern(
      const content::PlatformNotificationData& data) {
    // TODO(https://crbug.com/798466): Store as int32s to avoid this cast.
    return base::make_span(
        reinterpret_cast<const int32_t*>(data.vibration_pattern.data()),
        data.vibration_pattern.size());
  }

  static double timestamp(const content::PlatformNotificationData& data) {
    return data.timestamp.ToJsTime();
  }

  static bool renotify(const content::PlatformNotificationData& data) {
    return data.renotify;
  }

  static bool silent(const content::PlatformNotificationData& data) {
    return data.silent;
  }

  static bool require_interaction(
      const content::PlatformNotificationData& data) {
    return data.require_interaction;
  }

  static const base::span<const int8_t> data(
      const content::PlatformNotificationData& data) {
    // TODO(https://crbug.com/798466): Align data types to avoid this cast.
    return base::make_span(reinterpret_cast<const int8_t*>(data.data.data()),
                           data.data.size());
  }

  static const std::vector<content::PlatformNotificationAction>& actions(
      const content::PlatformNotificationData& data) {
    return data.actions;
  }

  static bool Read(
      blink::mojom::NotificationDataDataView notification_data,
      content::PlatformNotificationData* platform_notification_data);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_STRUCT_TRAITS_H_
