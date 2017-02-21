// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/browser/initialization.h"

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/task_scheduler/scheduler_worker_params.h"
#include "base/task_scheduler/switches.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/platform_thread.h"
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

std::vector<base::SchedulerWorkerPoolParams>
GetBrowserWorkerPoolParamsFromVariations() {
  using ThreadPriority = base::ThreadPriority;

  std::map<std::string, std::string> variation_params;
  if (!::variations::GetVariationParams(kFieldTrialName, &variation_params))
    return std::vector<base::SchedulerWorkerPoolParams>();

  std::vector<SchedulerImmutableWorkerPoolParams> immutable_worker_pool_params;
  DCHECK_EQ(BACKGROUND, immutable_worker_pool_params.size());
  immutable_worker_pool_params.emplace_back("Background",
                                            ThreadPriority::BACKGROUND);
  DCHECK_EQ(BACKGROUND_BLOCKING, immutable_worker_pool_params.size());
  immutable_worker_pool_params.emplace_back("BackgroundBlocking",
                                            ThreadPriority::BACKGROUND);
  DCHECK_EQ(FOREGROUND, immutable_worker_pool_params.size());
  immutable_worker_pool_params.emplace_back("Foreground",
                                            ThreadPriority::NORMAL);
  // Tasks posted to SequencedWorkerPool or BrowserThreadImpl may be redirected
  // to this pool. Since COM STA is initialized in these environments, it must
  // also be initialized in this pool.
  DCHECK_EQ(FOREGROUND_BLOCKING, immutable_worker_pool_params.size());
  immutable_worker_pool_params.emplace_back(
      "ForegroundBlocking", ThreadPriority::NORMAL,
      base::SchedulerBackwardCompatibility::INIT_COM_STA);

  return GetWorkerPoolParams(immutable_worker_pool_params, variation_params);
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
