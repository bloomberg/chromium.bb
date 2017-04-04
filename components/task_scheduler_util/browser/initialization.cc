// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/browser/initialization.h"

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/task_scheduler/scheduler_worker_params.h"
#include "base/task_scheduler/switches.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/task_scheduler_util/common/variations_util.h"
#include "components/variations/variations_associated_data.h"

namespace task_scheduler_util {

namespace {

constexpr char kFieldTrialName[] = "BrowserScheduler";

enum WorkerPoolType : size_t {
  BACKGROUND = 0,
  BACKGROUND_BLOCKING,
  FOREGROUND,
  FOREGROUND_BLOCKING,
  WORKER_POOL_COUNT  // Always last.
};

}  // namespace

std::unique_ptr<base::TaskScheduler::InitParams>
GetBrowserTaskSchedulerInitParamsFromVariations() {
  std::map<std::string, std::string> variation_params;
  if (!::variations::GetVariationParams(kFieldTrialName, &variation_params))
    return nullptr;

  return GetTaskSchedulerInitParams(
      "", variation_params, base::SchedulerBackwardCompatibility::INIT_COM_STA);
}

std::vector<base::SchedulerWorkerPoolParams>
GetBrowserWorkerPoolParamsFromVariations() {
  const auto init_params = GetBrowserTaskSchedulerInitParamsFromVariations();
  if (!init_params)
    return std::vector<base::SchedulerWorkerPoolParams>();

  return std::vector<base::SchedulerWorkerPoolParams>{
      init_params->background_worker_pool_params,
      init_params->background_blocking_worker_pool_params,
      init_params->foreground_worker_pool_params,
      init_params->foreground_blocking_worker_pool_params};
}

size_t BrowserWorkerPoolIndexForTraits(const base::TaskTraits& traits) {
  const bool is_background =
      traits.priority() == base::TaskPriority::BACKGROUND;
  if (traits.may_block() || traits.with_base_sync_primitives())
    return is_background ? BACKGROUND_BLOCKING : FOREGROUND_BLOCKING;
  return is_background ? BACKGROUND : FOREGROUND;
}

void MaybePerformBrowserTaskSchedulerRedirection() {
  // TODO(gab): Remove this when http://crbug.com/622400 concludes.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableBrowserTaskScheduler) &&
      variations::GetVariationParamValue(
          kFieldTrialName, "RedirectSequencedWorkerPools") == "true") {
    const base::TaskPriority max_task_priority =
        variations::GetVariationParamValue(
            kFieldTrialName, "CapSequencedWorkerPoolsAtUserVisible") == "true"
            ? base::TaskPriority::USER_VISIBLE
            : base::TaskPriority::HIGHEST;
    base::SequencedWorkerPool::EnableWithRedirectionToTaskSchedulerForProcess(
        max_task_priority);
  }
}

}  // namespace task_scheduler_util
