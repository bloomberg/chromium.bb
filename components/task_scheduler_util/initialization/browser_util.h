// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(fdoray): Remove this file once TaskScheduler initialization in the
// browser process uses the components/task_scheduler_util/browser/ API on all
// platforms.

#ifndef COMPONENTS_TASK_SCHEDULER_UTIL_INITIALIZATION_BROWSER_UTIL_H_
#define COMPONENTS_TASK_SCHEDULER_UTIL_INITIALIZATION_BROWSER_UTIL_H_

#include <stddef.h>

namespace base {
class TaskTraits;
}

namespace task_scheduler_util {
namespace initialization {

// Maps |traits| to the index of a browser worker pool vector provided by
// BrowserWorkerPoolConfigurationToSchedulerWorkerPoolParams() or
// GetDefaultBrowserSchedulerWorkerPoolParams().
size_t BrowserWorkerPoolIndexForTraits(const base::TaskTraits& traits);

}  // namespace initialization
}  // namespace task_scheduler_util

#endif  // COMPONENTS_TASK_SCHEDULER_UTIL_INITIALIZATION_BROWSER_UTIL_H_
