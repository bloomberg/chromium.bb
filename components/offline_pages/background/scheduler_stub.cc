// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/scheduler_stub.h"

namespace offline_pages {

SchedulerStub::SchedulerStub()
    : schedule_called_(false),
      backup_schedule_called_(false),
      unschedule_called_(false),
      schedule_delay_(0L),
      conditions_(false, 0, false) {}

SchedulerStub::~SchedulerStub() {}

void SchedulerStub::Schedule(const TriggerConditions& trigger_conditions) {
  schedule_called_ = true;
  conditions_ = trigger_conditions;
}

void SchedulerStub::BackupSchedule(const TriggerConditions& trigger_conditions,
                                   long delay_in_seconds) {
  backup_schedule_called_ = true;
  schedule_delay_ = delay_in_seconds;
  conditions_ = trigger_conditions;
}

void SchedulerStub::Unschedule() {
  unschedule_called_ = true;
}

}  // namespace offline_pages
