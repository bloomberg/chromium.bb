// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TASK_SCHEDULER_UTIL_INITIALIZATION_BROWSER_UTIL_H_
#define COMPONENTS_TASK_SCHEDULER_UTIL_INITIALIZATION_BROWSER_UTIL_H_

#include <vector>

#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/time/time.h"

namespace base {
class TaskTraits;
}

namespace task_scheduler_util {
namespace initialization {

enum WorkerPoolType : size_t {
  BACKGROUND = 0,
  BACKGROUND_FILE_IO,
  FOREGROUND,
  FOREGROUND_FILE_IO,
  WORKER_POOL_COUNT  // Always last.
};

struct SingleWorkerPoolConfiguration {
  base::SchedulerWorkerPoolParams::StandbyThreadPolicy standby_thread_policy;
  int threads = 0;
  base::TimeDelta detach_period;
};

struct BrowserWorkerPoolsConfiguration {
  SingleWorkerPoolConfiguration background;
  SingleWorkerPoolConfiguration background_file_io;
  SingleWorkerPoolConfiguration foreground;
  SingleWorkerPoolConfiguration foreground_file_io;
};

// Converts a BrowserWorkerPoolsConfiguration to a vector of
// base::SchedulerWorkerPoolParams for consumption by task scheduler
// initialization.
std::vector<base::SchedulerWorkerPoolParams>
BrowserWorkerPoolConfigurationToSchedulerWorkerPoolParams(
    const BrowserWorkerPoolsConfiguration& config);

// Maps |traits| to the index of a browser worker pool vector provided by
// BrowserWorkerPoolConfigurationToSchedulerWorkerPoolParams() or
// GetDefaultBrowserSchedulerWorkerPoolParams().
size_t BrowserWorkerPoolIndexForTraits(const base::TaskTraits& traits);

// Returns the default browser scheduler worker pool params.
std::vector<base::SchedulerWorkerPoolParams>
GetDefaultBrowserSchedulerWorkerPoolParams();

}  // namespace initialization
}  // namespace task_scheduler_util

#endif  // COMPONENTS_TASK_SCHEDULER_UTIL_INITIALIZATION_BROWSER_UTIL_H_
