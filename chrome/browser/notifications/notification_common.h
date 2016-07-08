// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_COMMON_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_COMMON_H_

namespace content {
class BrowserContext;
}  // namespace content

// Shared functionality for both in page and persistent notification
class NotificationCommon {
 public:
  // Things as user can do to a notification.
  enum Operation {
    CLICK = 0,
    CLOSE = 1,
    SETTINGS = 2,
    OPERATION_MAX = SETTINGS
  };

  // Possible kinds of notifications
  enum Type {
    PERSISTENT = 0,
    NON_PERSISTENT = 1,
    TYPE_MAX = NON_PERSISTENT
  };

  // Open the Notification settings screen when clicking the right button.
  // TODO(miguelg) have it take a Profile instead once NotificationObjectProxy
  // is updated.
  static void OpenNotificationSettings(
      content::BrowserContext* browser_context);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_COMMON_H_
