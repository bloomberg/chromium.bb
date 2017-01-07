// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TASK_SCHEDULER_UTIL_RENDERER_INITIALIZATION_H_
#define COMPONENTS_TASK_SCHEDULER_UTIL_RENDERER_INITIALIZATION_H_

#include <stddef.h>

#include <vector>

#include "base/task_scheduler/scheduler_worker_pool_params.h"

namespace base {
class TaskTraits;
}

namespace task_scheduler_util {

// Gets a vector of SchedulerWorkerPoolParams to initialize TaskScheduler in a
// renderer based off variation params specified on the command line. Returns an
// empty vector if variation params specified on the command line are incomplete
// or invalid.
std::vector<base::SchedulerWorkerPoolParams> GetRendererWorkerPoolParams();

// Maps |traits| to the index of a renderer worker pool vector provided by
// GetRendererWorkerPoolParams().
size_t RendererWorkerPoolIndexForTraits(const base::TaskTraits& traits);

}  // namespace task_scheduler_util

#endif  // COMPONENTS_TASK_SCHEDULER_UTIL_RENDERER_INITIALIZATION_H_
