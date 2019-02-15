// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULE_PARAMS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULE_PARAMS_H_

// Specifies when to show the scheduled notification, and throttling details.
struct ScheduleParams {
  enum class Priority {
    // No notification throttling logic is applied, every notification scheduled
    // will be delivered.
    NO_THROTTLE,
    // Notification may be delivered if picked by display decision layer. Has
    // higher chance to pick as the next notification to deliver than low
    // priority.
    HIGH,
    // Notification may be delivered if picked by display decision layer. Most
    // notification types should use this priority.
    LOW,
  };

  ScheduleParams();
  ~ScheduleParams();

  Priority priority;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULE_PARAMS_H_
