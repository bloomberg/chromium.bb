// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULER_CONTEXT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULER_CONTEXT_H_

#include <memory>

#include "base/macros.h"

namespace notifications {

class NotificationBackgroundTaskScheduler;
struct SchedulerConfig;

// Context that contains necessary components needed by the notification
// scheduler to perform tasks.
// TODO(xingliu): If we have less than 5 dependencies, remove this file to
// directly pass in the dependencies.
class NotificationSchedulerContext {
 public:
  NotificationSchedulerContext(
      std::unique_ptr<NotificationBackgroundTaskScheduler> scheduler,
      std::unique_ptr<SchedulerConfig> config);
  ~NotificationSchedulerContext();

  // Gets the background task scheduler.
  NotificationBackgroundTaskScheduler* background_task_scheduler() {
    return background_task_scheduler_.get();
  }

  // Gets system configuration.
  const SchedulerConfig* config() const { return config_.get(); }

 private:
  std::unique_ptr<NotificationBackgroundTaskScheduler>
      background_task_scheduler_;
  std::unique_ptr<SchedulerConfig> config_;

  DISALLOW_COPY_AND_ASSIGN(NotificationSchedulerContext);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULER_CONTEXT_H_
