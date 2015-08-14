// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/task_manager_observer.h"

#include "chrome/browser/task_management/task_manager_interface.h"

namespace task_management {

TaskManagerObserver::TaskManagerObserver(base::TimeDelta refresh_time,
                                         int64 resources_flags)
    : observed_task_manager_(nullptr),
      desired_refresh_time_(
        refresh_time < base::TimeDelta::FromSeconds(1) ?
            base::TimeDelta::FromSeconds(1) : refresh_time),
      desired_resources_flags_(resources_flags) {
}

TaskManagerObserver::~TaskManagerObserver() {
}

void TaskManagerObserver::AddRefreshType(RefreshType type) {
  desired_resources_flags_ |= type;

  if (observed_task_manager_)
    observed_task_manager_->RecalculateRefreshFlags();
}

void TaskManagerObserver::RemoveRefreshType(RefreshType type) {
  desired_resources_flags_ &= ~type;

  if (observed_task_manager_)
    observed_task_manager_->RecalculateRefreshFlags();
}


}  // namespace task_management
