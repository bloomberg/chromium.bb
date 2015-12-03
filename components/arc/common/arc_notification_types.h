// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMMON_ARC_NOTIFICATION_TYPES_H_
#define COMPONENTS_ARC_COMMON_ARC_NOTIFICATION_TYPES_H_

#include <string>
#include <vector>

#include "base/time/time.h"

namespace arc {

// These values must be matched with the NOTIFICATION_EVENT_* constants in
// com.android.server.ArcNotificationListenerService.
enum class ArcNotificationEvent {
  BODY_CLICKED = 0,
  CLOSED = 1,
  // Five buttons at maximum (message_center::kNotificationMaximumItems = 5).
  BUTTON1_CLICKED = 2,
  BUTTON2_CLICKED = 3,
  BUTTON3_CLICKED = 4,
  BUTTON4_CLICKED = 5,
  BUTTON5_CLICKED = 6,

  // Last enum entry for IPC_ENUM_TRAITS
  LAST = 6
};

// These values must be matched with the NOTIFICATION_TYPE_* constants in
// com.android.server.ArcNotificationListenerService.
enum class ArcNotificationType {
  BASIC = 0,
  IMAGE = 1,
  PROGRESS = 2,

  // Last enum entry for IPC_ENUM_TRAITS
  LAST = 2
};

struct ArcNotificationData {
  ArcNotificationData();
  ~ArcNotificationData();

  // Identifier of notification
  std::string key;
  // Type of notification
  ArcNotificationType type;
  // Body message of notification
  std::string message;
  // Title of notification
  std::string title;
  // Mimetype of |icon_data|
  std::string icon_mimetype;
  // Binary data of the icon
  std::vector<uint8_t> icon_data;
  // Priority of notification, must be [2,-2]
  int priority;
  // Timestamp related to the notification
  base::Time time;
  // The current value of progress, must be [0, progress_max].
  int progress_current;
  // The maximum value of progress.
  int progress_max;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMMON_ARC_NOTIFICATION_TYPES_H_
