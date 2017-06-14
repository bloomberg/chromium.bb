// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_COMMON_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_COMMON_H_

#include "base/feature_list.h"

namespace features {

// TODO(miguelg) We can probably get rid of this altogether.
extern const base::Feature kAllowFullscreenWebNotificationsFeature;

}  // namespace features

namespace content {
class BrowserContext;
}  // namespace content

class GURL;
class Profile;

// Shared functionality for both in page and persistent notification
class NotificationCommon {
 public:
  // Things as user can do to a notification.
  // TODO(peter): Prefix these options with OPERATION_.
  enum Operation {
    CLICK = 0,
    CLOSE = 1,
    SETTINGS = 2,
    OPERATION_MAX = SETTINGS
  };

  // Possible kinds of notifications
  // TODO(peter): Prefix these options with TYPE_.
  enum Type {
    PERSISTENT = 0,
    NON_PERSISTENT = 1,
    EXTENSION = 2,
    TYPE_MAX = EXTENSION
  };

  // Open the Notification settings screen when clicking the right button.
  // TODO(miguelg) have it take a Profile instead once NotificationObjectProxy
  // is updated.
  static void OpenNotificationSettings(
      content::BrowserContext* browser_context);

  // Whether a web notification should be displayed when chrome is in full
  // screen mode.
  static bool ShouldDisplayOnFullScreen(Profile* profile, const GURL& origin);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_COMMON_H_
