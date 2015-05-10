// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PLATFORM_NOTIFICATION_DATA_H_
#define CONTENT_PUBLIC_COMMON_PLATFORM_NOTIFICATION_DATA_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// Structure representing the information associated with a Web Notification.
// This struct should include the developer-visible information, kept
// synchronized with the WebNotificationData structure defined in the Blink API.
struct CONTENT_EXPORT PlatformNotificationData {
  PlatformNotificationData();
  ~PlatformNotificationData();

  // The maximum size of developer-provided data to be stored in the |data|
  // property of this structure.
  static const size_t kMaximumDeveloperDataSize = 1024 * 1024;

  enum NotificationDirection {
    NotificationDirectionLeftToRight,
    NotificationDirectionRightToLeft,

    NotificationDirectionLast = NotificationDirectionRightToLeft
  };

  // Title to be displayed with the Web Notification.
  base::string16 title;

  // Hint to determine the directionality of the displayed notification.
  NotificationDirection direction = NotificationDirectionLeftToRight;

  // BCP 47 language tag describing the notification's contents. Optional.
  std::string lang;

  // Contents of the notification.
  base::string16 body;

  // Tag of the notification. Notifications sharing both their origin and their
  // tag will replace the first displayed notification.
  std::string tag;

  // URL of the icon which is to be displayed with the notification.
  GURL icon;

  // Vibration pattern for the notification, following the syntax of the
  // Vibration API. https://www.w3.org/TR/vibration/
  std::vector<int> vibration_pattern;

  // Whether default notification indicators (sound, vibration, light) should
  // be suppressed.
  bool silent = false;

  // Developer-provided data associated with the notification, in the form of
  // a serialized string. Must not exceed |kMaximumDeveloperDataSize| bytes.
  std::vector<char> data;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PLATFORM_NOTIFICATION_DATA_H_
