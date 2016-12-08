// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TASK_SCHEDULER_UTIL_VARIATIONS_BROWSER_VARIATIONS_UTIL_H_
#define COMPONENTS_TASK_SCHEDULER_UTIL_VARIATIONS_BROWSER_VARIATIONS_UTIL_H_

#include <vector>

#include "base/task_scheduler/scheduler_worker_pool_params.h"

namespace task_scheduler_util {
namespace variations {

// Gets the browser SchedulerWorkerPoolParams vector based off of variations.
// Returns an empty vector on failure.
std::vector<base::SchedulerWorkerPoolParams>
GetBrowserSchedulerWorkerPoolParamsFromVariations();

// Redirects zero-to-many PostTask APIs to the browser task scheduler based off
// of input from the variations parameters.
void MaybePerformBrowserTaskSchedulerRedirection();

}  // variations
}  // namespace task_scheduler_util

#endif  // COMPONENTS_TASK_SCHEDULER_UTIL_VARIATIONS_BROWSER_VARIATIONS_UTIL_H_
