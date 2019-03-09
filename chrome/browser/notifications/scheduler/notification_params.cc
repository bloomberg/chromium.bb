// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/notification_params.h"

#include <utility>

#include "chrome/browser/notifications/scheduler/schedule_params.h"

namespace notifications {

NotificationParams::NotificationParams(SchedulerClientType type,
                                       NotificationData notification,
                                       ScheduleParams schedule_params)
    : type(type),
      notification(std::move(notification)),
      schedule_params(std::move(schedule_params)) {}

NotificationParams::~NotificationParams() = default;

}  // namespace notifications
