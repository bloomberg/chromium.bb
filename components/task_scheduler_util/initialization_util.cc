// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/initialization_util.h"

#include <vector>

#include "base/bind.h"
#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task_scheduler/task_scheduler.h"
#include "components/task_scheduler_util/initialization/browser_util.h"
#include "components/task_scheduler_util/variations/browser_variations_util.h"

namespace task_scheduler_util {

void InitializeDefaultBrowserTaskScheduler() {
  std::vector<base::SchedulerWorkerPoolParams> params_vector =
      variations::GetBrowserSchedulerWorkerPoolParamsFromVariations();
  if (params_vector.empty()) {
    params_vector =
        initialization::GetDefaultBrowserSchedulerWorkerPoolParams();
  }

  base::TaskScheduler::CreateAndSetDefaultTaskScheduler(
      params_vector,
      base::Bind(&initialization::BrowserWorkerPoolIndexForTraits));
  task_scheduler_util::variations::
      MaybePerformBrowserTaskSchedulerRedirection();
}

}  // namespace task_scheduler_util
