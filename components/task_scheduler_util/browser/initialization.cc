// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/browser/initialization.h"

#include <map>
#include <string>

#include "build/build_config.h"
#include "components/task_scheduler_util/common/variations_util.h"
#include "components/variations/variations_associated_data.h"

namespace task_scheduler_util {

namespace {

constexpr char kFieldTrialName[] = "BrowserScheduler";

}  // namespace

std::unique_ptr<base::TaskScheduler::InitParams>
GetBrowserTaskSchedulerInitParamsFromVariations() {
  std::map<std::string, std::string> variation_params;
  if (!::variations::GetVariationParams(kFieldTrialName, &variation_params))
    return nullptr;

  std::unique_ptr<base::TaskScheduler::InitParams> init_params =
      GetTaskSchedulerInitParams(
          "", variation_params, base::SchedulerBackwardCompatibility::DISABLED);
#if defined(OS_WIN)
  if (init_params) {
    init_params->shared_worker_pool_environment =
        base::TaskScheduler::InitParams::SharedWorkerPoolEnvironment::COM_MTA;
  }
#endif  // defined(OS_WIN)
  return init_params;
}

}  // namespace task_scheduler_util
