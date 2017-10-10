// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_NOTIFICATION_TYPES_H_
#define COMPONENTS_SAFE_BROWSING_DB_NOTIFICATION_TYPES_H_

// Lists of Safe Browsing related notifications, used with NotificationService.
namespace safe_browsing {

enum NotificationType {
  NOTIFICATION_SAFE_BROWSING_START = 0,

  // General -----------------------------------------------------------------

  // Special signal value to represent an interest in all notifications.
  // Not valid when posting a notification.
  NOTIFICATION_ALL = NOTIFICATION_SAFE_BROWSING_START,

  // A safe browsing database update completed.  Source is the
  // SafeBrowsingDatabaseManager and the details are none.
  NOTIFICATION_SAFE_BROWSING_UPDATE_COMPLETE,

  NOTIFICATION_SAFE_BROWSING_END
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_NOTIFICATION_TYPES_H_
