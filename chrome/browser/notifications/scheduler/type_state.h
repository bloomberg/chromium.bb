// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TYPE_STATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TYPE_STATE_H_

#include <vector>

#include "chrome/browser/notifications/scheduler/impression_entry.h"
#include "chrome/browser/notifications/scheduler/notification_entry.h"

namespace notifications {

// Stores all data about a particular type of notification.
struct TypeState {
  explicit TypeState(SchedulerClientType type);
  ~TypeState();

  SchedulerClientType type;

  // A list of user impression history.
  std::vector<ImpressionEntry> impressions;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TYPE_STATE_H_
