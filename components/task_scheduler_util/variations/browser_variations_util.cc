// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(fdoray): Remove this file once TaskScheduler initialization in the
// browser process uses the components/task_scheduler_util/browser/ API on all
// platforms.

#include "components/task_scheduler_util/variations/browser_variations_util.h"

#include "components/task_scheduler_util/browser/initialization.h"

namespace task_scheduler_util {
namespace variations {

std::vector<base::SchedulerWorkerPoolParams>
GetBrowserSchedulerWorkerPoolParamsFromVariations() {
  return ::task_scheduler_util::GetBrowserWorkerPoolParamsFromVariations();
}

void MaybePerformBrowserTaskSchedulerRedirection() {
  ::task_scheduler_util::MaybePerformBrowserTaskSchedulerRedirection();
}

}  // variations
}  // namespace task_scheduler_util
