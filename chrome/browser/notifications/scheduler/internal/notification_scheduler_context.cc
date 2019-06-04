// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/notification_scheduler_context.h"

#include <utility>

#include "chrome/browser/notifications/scheduler/internal/display_decider.h"
#include "chrome/browser/notifications/scheduler/internal/icon_store.h"
#include "chrome/browser/notifications/scheduler/internal/impression_history_tracker.h"
#include "chrome/browser/notifications/scheduler/internal/scheduled_notification_manager.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/public/notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client_registrar.h"

namespace notifications {

NotificationSchedulerContext::NotificationSchedulerContext(
    std::unique_ptr<NotificationSchedulerClientRegistrar> client_registrar,
    std::unique_ptr<NotificationBackgroundTaskScheduler> scheduler,
    std::unique_ptr<IconStore> icon_store,
    std::unique_ptr<ImpressionHistoryTracker> impression_tracker,
    std::unique_ptr<ScheduledNotificationManager> notification_manager,
    std::unique_ptr<DisplayDecider> display_decider,
    std::unique_ptr<SchedulerConfig> config)
    : client_registrar_(std::move(client_registrar)),
      background_task_scheduler_(std::move(scheduler)),
      impression_tracker_(std::move(impression_tracker)),
      notification_manager_(std::move(notification_manager)),
      display_decider_(std::move(display_decider)),
      config_(std::move(config)) {}

NotificationSchedulerContext::~NotificationSchedulerContext() = default;

}  // namespace notifications
