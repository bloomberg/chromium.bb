// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULER_CONTEXT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULER_CONTEXT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler_types.h"

namespace notifications {

class DisplayDecider;
class IconStore;
class ImpressionHistoryTracker;
class NotificationBackgroundTaskScheduler;
class NotificationSchedulerClientRegistrar;
class ScheduledNotificationManager;
struct SchedulerConfig;

// Context that contains necessary components needed by the notification
// scheduler to perform tasks.
class NotificationSchedulerContext {
 public:
  NotificationSchedulerContext(
      std::unique_ptr<NotificationSchedulerClientRegistrar> client_registrar,
      std::unique_ptr<NotificationBackgroundTaskScheduler> scheduler,
      std::unique_ptr<IconStore> icon_store,
      std::unique_ptr<ImpressionHistoryTracker> impression_tracker,
      std::unique_ptr<ScheduledNotificationManager> notification_manager,
      std::unique_ptr<DisplayDecider> display_decider,
      std::unique_ptr<SchedulerConfig> config);
  ~NotificationSchedulerContext();

  NotificationSchedulerClientRegistrar* client_registrar() {
    return client_registrar_.get();
  }

  NotificationBackgroundTaskScheduler* background_task_scheduler() {
    return background_task_scheduler_.get();
  }

  IconStore* icon_store() { return icon_store_.get(); }

  ImpressionHistoryTracker* impression_tracker() {
    return impression_tracker_.get();
  }

  ScheduledNotificationManager* notification_manager() {
    return notification_manager_.get();
  }

  DisplayDecider* display_decider() { return display_decider_.get(); }

  const SchedulerConfig* config() const { return config_.get(); }

 private:
  // Holds a list of clients using the notification scheduler system.
  std::unique_ptr<NotificationSchedulerClientRegistrar> client_registrar_;

  // Used to schedule background task in OS level.
  std::unique_ptr<NotificationBackgroundTaskScheduler>
      background_task_scheduler_;

  // Stores notification icons.
  std::unique_ptr<IconStore> icon_store_;

  // Tracks user impressions towards specific notification type.
  std::unique_ptr<ImpressionHistoryTracker> impression_tracker_;

  // Stores all scheduled notifications.
  std::unique_ptr<ScheduledNotificationManager> notification_manager_;

  // Helper class to decide which notification should be displayed to the user.
  std::unique_ptr<DisplayDecider> display_decider_;

  // System configuration.
  std::unique_ptr<SchedulerConfig> config_;

  DISALLOW_COPY_AND_ASSIGN(NotificationSchedulerContext);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULER_CONTEXT_H_
