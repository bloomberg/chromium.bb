// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/notification_scheduler_context.h"

#include "chrome/browser/notifications/scheduler/notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/scheduler_config.h"

namespace notifications {

NotificationSchedulerContext::NotificationSchedulerContext(
    std::unique_ptr<NotificationBackgroundTaskScheduler> scheduler,
    std::unique_ptr<SchedulerConfig> config)
    : background_task_scheduler_(std::move(scheduler)),
      config_(std::move(config)) {}

NotificationSchedulerContext::~NotificationSchedulerContext() = default;

}  // namespace notifications
