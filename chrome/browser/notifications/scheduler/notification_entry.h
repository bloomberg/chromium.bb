// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_ENTRY_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_ENTRY_H_

#include <string>

#include "chrome/browser/notifications/scheduler/notification_data.h"
#include "chrome/browser/notifications/scheduler/schedule_params.h"

namespace notifications {

// Represents the in-memory counterpart of scheduled notification database
// record.
struct NotificationEntry {
  explicit NotificationEntry(const std::string& guid);
  ~NotificationEntry();

  // The unique id of the notification database entry.
  std::string guid;

  NotificationData notification_data;

  ScheduleParams schedule_params;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_ENTRY_H_
