// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/notification_schedule_service_impl.h"

#include <memory>

#include "base/logging.h"
#include "chrome/browser/notifications/scheduler/notification_params.h"

namespace notifications {

NotificationScheduleServiceImpl::NotificationScheduleServiceImpl() = default;

NotificationScheduleServiceImpl::~NotificationScheduleServiceImpl() = default;

void NotificationScheduleServiceImpl::Schedule(
    std::unique_ptr<NotificationParams> notification_params) {
  NOTIMPLEMENTED();
}

}  // namespace notifications
