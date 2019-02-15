// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_DATA_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_DATA_H_

#include <memory>

#include "base/macros.h"
#include "ui/message_center/public/cpp/notification.h"

struct ScheduleParams;

// Struct used to schedule a notification.
class NotificationData {
 public:
  enum class Type {
    PLACE_HOLDER,
  };

  NotificationData(Type type,
                   std::unique_ptr<message_center::Notification> notification,
                   std::unique_ptr<ScheduleParams> schedule_params);
  ~NotificationData();

  Type type() const { return type_; }

 private:
  Type type_;

  // Notification data that the client can optionally choose to use when showing
  // a notification. Client should only use this when the data is dynamically
  // generated, such as URL to navigate when the user clicks the notification.
  std::unique_ptr<message_center::Notification> notification_;

  std::unique_ptr<ScheduleParams> schedule_params_;

  DISALLOW_COPY_AND_ASSIGN(NotificationData);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_DATA_H_
