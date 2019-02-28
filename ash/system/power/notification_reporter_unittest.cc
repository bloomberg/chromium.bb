// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/notification_reporter.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/test/ash_test_base.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

class NotificationReporterTest : public testing::Test {
 public:
  NotificationReporterTest()
      : fake_power_manager_client_(new chromeos::FakePowerManagerClient()),
        notification_reporter_(&fake_message_center_,
                               fake_power_manager_client_) {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetPowerManagerClient(
        std::unique_ptr<chromeos::PowerManagerClient>(
            fake_power_manager_client_));
  }
  ~NotificationReporterTest() override = default;

 protected:
  message_center::FakeMessageCenter fake_message_center_;
  // Owned by DBusThreadManager.
  chromeos::FakePowerManagerClient* fake_power_manager_client_;
  NotificationReporter notification_reporter_;

  DISALLOW_COPY_AND_ASSIGN(NotificationReporterTest);
};

TEST_F(NotificationReporterTest, CheckNotifyWakeNotification) {
  // Create a high priority notification and check that the power manager got
  // called.
  auto notification = std::make_unique<message_center::Notification>();
  const std::string notification_id = notification->id();
  notification->set_priority(
      static_cast<int>(message_center::NotificationPriority::HIGH_PRIORITY));
  fake_message_center_.AddNotification(std::move(notification));
  EXPECT_EQ(1, fake_power_manager_client_->num_wake_notification_calls());

  // Update the old notification. Check if the power manager got called again.
  notification = std::make_unique<message_center::Notification>();
  notification->set_priority(
      static_cast<int>(message_center::NotificationPriority::HIGH_PRIORITY));
  fake_message_center_.UpdateNotification(notification_id,
                                          std::move(notification));
  EXPECT_EQ(2, fake_power_manager_client_->num_wake_notification_calls());

  // A low priority notification should not result in any calls to the power
  // manager.
  notification = std::make_unique<message_center::Notification>();
  notification->set_priority(
      static_cast<int>(message_center::NotificationPriority::LOW_PRIORITY));
  fake_message_center_.AddNotification(std::move(notification));
  EXPECT_EQ(2, fake_power_manager_client_->num_wake_notification_calls());
}

TEST_F(NotificationReporterTest, CheckDismissedNotification) {
  // Create a high priority notification and check that the power manager got
  // called.
  auto notification = std::make_unique<message_center::Notification>();
  const std::string notification_id = notification->id();
  notification->set_priority(
      static_cast<int>(message_center::NotificationPriority::HIGH_PRIORITY));
  fake_message_center_.AddNotification(std::move(notification));
  EXPECT_EQ(1, fake_power_manager_client_->num_wake_notification_calls());

  // Remove the notification from the message center and then directly call the
  // observer API. This shouldn't call the power manager as
  // |notification_reporter_| won't be able to find the notification and
  // consequently determine it's priority.
  fake_message_center_.RemoveNotification(notification_id, false /* by_user */);
  notification_reporter_.OnNotificationUpdated(notification_id);
  EXPECT_EQ(1, fake_power_manager_client_->num_wake_notification_calls());
}

}  // namespace ash
