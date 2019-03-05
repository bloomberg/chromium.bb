// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service.h"

namespace notifications {

struct NotificationParams;

class NotificationScheduleServiceImpl : public NotificationScheduleService {
 public:
  NotificationScheduleServiceImpl();
  ~NotificationScheduleServiceImpl() override;

 private:
  // NotificationScheduleService implementation.
  void Schedule(
      std::unique_ptr<NotificationParams> notification_params) override;

  DISALLOW_COPY_AND_ASSIGN(NotificationScheduleServiceImpl);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULE_SERVICE_IMPL_H_
