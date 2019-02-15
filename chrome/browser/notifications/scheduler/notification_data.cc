// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/notification_data.h"

#include "chrome/browser/notifications/scheduler/schedule_params.h"

NotificationData::NotificationData(
    Type type,
    std::unique_ptr<message_center::Notification> notification,
    std::unique_ptr<ScheduleParams> schedule_params)
    : type_(type),
      notification_(std::move(notification)),
      schedule_params_(std::move(schedule_params)) {}

NotificationData::~NotificationData() = default;
