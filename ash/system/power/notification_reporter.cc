// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/notification_reporter.h"

#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

NotificationReporter::NotificationReporter(
    message_center::MessageCenter* message_center,
    chromeos::PowerManagerClient* power_manager_client)
    : message_center_(message_center),
      power_manager_client_(power_manager_client) {
  DCHECK(message_center_);
  DCHECK(power_manager_client_);
  message_center_->AddObserver(this);
}

NotificationReporter::~NotificationReporter() {
  message_center_->RemoveObserver(this);
}

void NotificationReporter::OnNotificationAdded(
    const std::string& notification_id) {
  MaybeNotifyPowerManager(notification_id);
}

void NotificationReporter::OnNotificationUpdated(
    const std::string& notification_id) {
  MaybeNotifyPowerManager(notification_id);
}

void NotificationReporter::MaybeNotifyPowerManager(
    const std::string& notification_id) {
  // Guard against notification removals before observers are notified.
  message_center::Notification* notification =
      message_center_->FindVisibleNotificationById(notification_id);
  if (!notification)
    return;

  // If this is a high priority notification wake the display up if the
  // system is in dark resume i.e. transition to full resume.
  if (notification->priority() >
      message_center::NotificationPriority::DEFAULT_PRIORITY)
    power_manager_client_->NotifyWakeNotification();
}

}  // namespace ash
