// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TASK_SCHEDULER_UTIL_RENDERER_INITIALIZATION_H_
#define COMPONENTS_TASK_SCHEDULER_UTIL_RENDERER_INITIALIZATION_H_

#include <memory>

#include "base/task_scheduler/task_scheduler.h"

namespace task_scheduler_util {

// Gets a TaskScheduler::InitParams object to initialize TaskScheduler in a
// renderer based off variations specified on the command line. Returns nullptr
// on failure.
std::unique_ptr<base::TaskScheduler::InitParams>
GetRendererTaskSchedulerInitParamsFromCommandLine();

}  // namespace task_scheduler_util

#endif  // COMPONENTS_TASK_SCHEDULER_UTIL_RENDERER_INITIALIZATION_H_
