// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TASK_SCHEDULER_UTIL_BROWSER_INITIALIZATION_H_
#define COMPONENTS_TASK_SCHEDULER_UTIL_BROWSER_INITIALIZATION_H_

#include <stddef.h>

#include <vector>

#include "base/task_scheduler/scheduler_worker_pool_params.h"

namespace base {
class TaskTraits;
}

namespace task_scheduler_util {

// Gets a vector of SchedulerWorkerPoolParams to initialize TaskScheduler in the
// browser based off variations. Returns an empty vector on failure.
std::vector<base::SchedulerWorkerPoolParams>
GetBrowserWorkerPoolParamsFromVariations();

// Maps |traits| to the index of a browser worker pool vector provided by
// GetBrowserWorkerPoolParamsFromVariations().
size_t BrowserWorkerPoolIndexForTraits(const base::TaskTraits& traits);

// Redirects zero-to-many PostTask APIs to the browser task scheduler based off
// variations.
void MaybePerformBrowserTaskSchedulerRedirection();

}  // namespace task_scheduler_util

#endif  // COMPONENTS_TASK_SCHEDULER_UTIL_BROWSER_INITIALIZATION_H_
