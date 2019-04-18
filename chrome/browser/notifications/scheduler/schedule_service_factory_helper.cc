// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/schedule_service_factory_helper.h"

#include "chrome/browser/notifications/scheduler/notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_impl.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler_context.h"
#include "chrome/browser/notifications/scheduler/scheduler_config.h"

namespace notifications {

KeyedService* CreateNotificationScheduleService(
    std::unique_ptr<NotificationBackgroundTaskScheduler>
        background_task_scheduler,
    leveldb_proto::ProtoDatabaseProvider* db_provider) {
  auto config = SchedulerConfig::Create();
  auto context = std::make_unique<NotificationSchedulerContext>(
      std::move(background_task_scheduler), std::move(config));
  return static_cast<KeyedService*>(
      new NotificationScheduleServiceImpl(std::move(context)));
}

}  // namespace notifications
