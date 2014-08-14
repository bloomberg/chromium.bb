// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_CONVERSION_HELPER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_CONVERSION_HELPER_H_

#include "chrome/browser/notifications/notification.h"
#include "ui/message_center/notification_types.h"

namespace extensions {
namespace api {
namespace notifications {
struct NotificationOptions;
struct NotificationBitmap;
}  // namespace notifications
}  // namespace api
}  // namespace extensions

namespace gfx {
class Image;
}

// This class provides some static helper functions that could be used to
// convert between different types of notification data.
class NotificationConversionHelper {
 public:
  // Converts Notification::Notification data to
  // extensions::api::notifications::NotificationOptions.
  static void NotificationToNotificationOptions(
      const Notification& notification,
      extensions::api::notifications::NotificationOptions* options);

  // Converts gfx::Image (in ARGB format) type to
  // extensions::api::notifications::NotificationBitmap type (RGBA).
  static void GfxImageToNotificationBitmap(
      const gfx::Image* gfx_image,
      extensions::api::notifications::NotificationBitmap* return_image_args);

  // Converts an extensions::api::notifications::NotificationBitmap type object
  // with width, height, and data in RGBA format into an gfx::Image (ARGB).
  static bool NotificationBitmapToGfxImage(
      float max_scale,
      const gfx::Size& target_size_dips,
      extensions::api::notifications::NotificationBitmap* notification_bitmap,
      gfx::Image* return_image);

 private:
  // Conversts message_center::NotificationType to string type used to convert
  // to extensions::api::notifications::TemplateType
  static std::string MapTypeToString(message_center::NotificationType type);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_CONVERSION_HELPER_H_
