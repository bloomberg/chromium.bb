// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/notification_reporter.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

using NotificationReporterTest = AshTestBase;

TEST_F(NotificationReporterTest, CheckNotifyWakeNotification) {
  // Create a high priority notification and check that the power manager got
  // called.
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, "notification-id", {}, {}, {},
      {}, {}, {}, {}, nullptr);
  notification.set_priority(
      static_cast<int>(message_center::NotificationPriority::HIGH_PRIORITY));
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(notification));
  EXPECT_EQ(
      1,
      chromeos::FakePowerManagerClient::Get()->num_wake_notification_calls());

  // Update the old notification. Check if the power manager got called again.
  message_center::MessageCenter::Get()->UpdateNotification(
      notification.id(),
      std::make_unique<message_center::Notification>(notification));
  EXPECT_EQ(
      2,
      chromeos::FakePowerManagerClient::Get()->num_wake_notification_calls());

  // A low priority notification should not result in any calls to the power
  // manager.
  notification.set_priority(
      static_cast<int>(message_center::NotificationPriority::LOW_PRIORITY));
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>("different-id",
                                                     notification));
  EXPECT_EQ(
      2,
      chromeos::FakePowerManagerClient::Get()->num_wake_notification_calls());
}

TEST_F(NotificationReporterTest, CheckDismissedNotification) {
  // Create a high priority notification and check that the power manager got
  // called.
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, "notification-id", {}, {}, {},
      {}, {}, {}, {}, nullptr);
  notification.set_priority(
      static_cast<int>(message_center::NotificationPriority::HIGH_PRIORITY));
  message_center::MessageCenter::Get()->AddNotification(
      std::make_unique<message_center::Notification>(notification));
  EXPECT_EQ(
      1,
      chromeos::FakePowerManagerClient::Get()->num_wake_notification_calls());

  // Remove the notification from the message center and then directly call the
  // observer API. This shouldn't call the power manager as NotificationReporter
  // won't be able to find the notification and consequently determine it's
  // priority.
  message_center::MessageCenter::Get()->RemoveNotification(notification.id(),
                                                           false /* by_user */);
  Shell::Get()->notification_reporter()->OnNotificationUpdated(
      notification.id());
  EXPECT_EQ(
      1,
      chromeos::FakePowerManagerClient::Get()->num_wake_notification_calls());
}

}  // namespace ash
