// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/notifications/notification_style.h"

#include "ui/message_center/message_center_style.h"

NotificationBitmapSizes GetNotificationBitmapSizes() {
  NotificationBitmapSizes sizes;
  sizes.image_size =
      gfx::Size(message_center::kNotificationPreferredImageWidth,
                message_center::kNotificationPreferredImageHeight);
  sizes.icon_size = gfx::Size(message_center::kNotificationIconSize,
                              message_center::kNotificationIconSize);
  sizes.button_icon_size =
      gfx::Size(message_center::kNotificationButtonIconSize,
                message_center::kNotificationButtonIconSize);
  return sizes;
}
