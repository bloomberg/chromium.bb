// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_DATA_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_DATA_H_

#include <string>

namespace notifications {

// Contains data used to display a scheduled notification. All fields will be
// persisted to disk as protobuffer NotificationData. The clients of
// notification scheduler can optionally use the texts or icon in this struct,
// or use hard coded assets id.
struct NotificationData {
  NotificationData();
  NotificationData(const NotificationData& other);
  ~NotificationData();

  // The unique identifier of the notification. This is not used as the key for
  // the database entry, but the id of other notification struct plumbed to
  // the scheduler system.
  std::string id;

  // The title of the notification.
  std::string title;

  // The body text of the notification.
  std::string message;

  // The unique identifier of the icon, which must be loaded asynchronously into
  // memory.
  std::string icon_uuid;

  // URL of the web site responsible for showing the notification.
  std::string url;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_DATA_H_
