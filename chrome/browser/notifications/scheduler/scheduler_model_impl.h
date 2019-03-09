// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULER_MODEL_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULER_MODEL_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/scheduler_model.h"

namespace notifications {

struct NotificationEntry;

class SchedulerModelImpl : public SchedulerModel {
 public:
  SchedulerModelImpl();
  ~SchedulerModelImpl() override;

 private:
  // SchedulerModel implementation.
  void Initialize(InitCallback init_callback) override;
  TypeState* GetTypeState(SchedulerClientType type) override;
  void UpdateTypeState(SchedulerClientType type) override;
  void DeleteTypeState(SchedulerClientType type) override;
  void AddNotification(NotificationEntry entry) override;
  NotificationEntry* GetNotification(const std::string& guid) override;
  void UpdateNotification(const std::string& guid) override;
  void DeleteNotification(const std::string& guid) override;

  DISALLOW_COPY_AND_ASSIGN(SchedulerModelImpl);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULER_MODEL_IMPL_H_
