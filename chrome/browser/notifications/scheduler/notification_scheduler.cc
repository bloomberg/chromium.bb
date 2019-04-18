// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/notification_scheduler.h"

#include "base/logging.h"
#include "chrome/browser/notifications/scheduler/notification_params.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler_context.h"

namespace notifications {
namespace {

// Implementation of NotificationScheduler.
class NotificationSchedulerImpl : public NotificationScheduler {
 public:
  NotificationSchedulerImpl(
      std::unique_ptr<NotificationSchedulerContext> context)
      : context_(std::move(context)) {}

  ~NotificationSchedulerImpl() override = default;

 private:
  // NotificationScheduler implementation.
  void Init(InitCallback init_callback) override { NOTIMPLEMENTED(); }

  void Schedule(
      std::unique_ptr<NotificationParams> notification_params) override {
    NOTIMPLEMENTED();
  }

  std::unique_ptr<NotificationSchedulerContext> context_;

  DISALLOW_COPY_AND_ASSIGN(NotificationSchedulerImpl);
};

}  // namespace

// static
std::unique_ptr<NotificationScheduler> NotificationScheduler::Create(
    std::unique_ptr<NotificationSchedulerContext> context) {
  return std::make_unique<NotificationSchedulerImpl>(std::move(context));
}

NotificationScheduler::NotificationScheduler() = default;

NotificationScheduler::~NotificationScheduler() = default;

}  // namespace notifications
