// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/scheduler_model_impl.h"

#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/notification_entry.h"

namespace notifications {

SchedulerModelImpl::SchedulerModelImpl() = default;

SchedulerModelImpl::~SchedulerModelImpl() = default;

void SchedulerModelImpl::Initialize(InitCallback init_callback) {
  NOTIMPLEMENTED();
}

TypeState* SchedulerModelImpl::GetTypeState(SchedulerClientType type) {
  NOTIMPLEMENTED();
  return nullptr;
}

void SchedulerModelImpl::UpdateTypeState(SchedulerClientType type) {
  NOTIMPLEMENTED();
}

void SchedulerModelImpl::DeleteTypeState(SchedulerClientType type) {
  NOTIMPLEMENTED();
}

void SchedulerModelImpl::AddNotification(NotificationEntry entry) {
  NOTIMPLEMENTED();
}

NotificationEntry* SchedulerModelImpl::GetNotification(
    const std::string& guid) {
  NOTIMPLEMENTED();
  return nullptr;
}

void SchedulerModelImpl::UpdateNotification(const std::string& guid) {
  NOTIMPLEMENTED();
}

void SchedulerModelImpl::DeleteNotification(const std::string& guid) {
  NOTIMPLEMENTED();
}

}  // namespace notifications
