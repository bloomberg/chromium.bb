// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/variations_util.h"

#include <map>
#include <string>

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/task_scheduler/initialization_util.h"
#include "base/time/time.h"

namespace task_scheduler_util {

namespace {

// Builds a SchedulerWorkerPoolParams from the pool descriptor in
// |variation_params[pool_name]|. Returns an invalid
// SchedulerWorkerPoolParams on failure.
//
// The pool descriptor is a semi-colon separated value string with the following
// items:
// 0. Minimum Thread Count (int)
// 1. Maximum Thread Count (int)
// 2. Thread Count Multiplier (double)
// 3. Thread Count Offset (int)
// 4. Detach Time in Milliseconds (int)
// Additional values may appear as necessary and will be ignored.
std::unique_ptr<base::SchedulerWorkerPoolParams> GetWorkerPoolParams(
    base::StringPiece pool_name,
    const std::map<std::string, std::string>& variation_params) {
  auto pool_descriptor_it = variation_params.find(pool_name.as_string());
  if (pool_descriptor_it == variation_params.end())
    return nullptr;
  const auto& pool_descriptor = pool_descriptor_it->second;

  const std::vector<base::StringPiece> tokens = SplitStringPiece(
      pool_descriptor, ";", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // Normally, we wouldn't initialize the values below because we don't read
  // from them before we write to them. However, some compilers (like MSVC)
  // complain about uninitialized variables due to the as_string() call below.
  int min = 0;
  int max = 0;
  double cores_multiplier = 0.0;
  int offset = 0;
  int detach_milliseconds = 0;
  // Checking for a size greater than the expected amount allows us to be
  // forward compatible if we add more variation values.
  if (tokens.size() < 5 || !base::StringToInt(tokens[0], &min) ||
      !base::StringToInt(tokens[1], &max) ||
      !base::StringToDouble(tokens[2].as_string(), &cores_multiplier) ||
      !base::StringToInt(tokens[3], &offset) ||
      !base::StringToInt(tokens[4], &detach_milliseconds)) {
    DLOG(ERROR) << "Invalid Worker Pool Descriptor Format: " << pool_descriptor;
    return nullptr;
  }

  auto params = std::make_unique<base::SchedulerWorkerPoolParams>(
      base::RecommendedMaxNumberOfThreadsInPool(min, max, cores_multiplier,
                                                offset),
      base::TimeDelta::FromMilliseconds(detach_milliseconds));

  if (params->max_tasks() <= 0) {
    DLOG(ERROR) << "Invalid max tasks in the Worker Pool Descriptor: "
                << params->max_tasks();
    return nullptr;
  }

  if (params->suggested_reclaim_time() < base::TimeDelta()) {
    DLOG(ERROR)
        << "Invalid suggested reclaim time in the Worker Pool Descriptor:"
        << params->suggested_reclaim_time();
    return nullptr;
  }

  return params;
}

}  // namespace

const base::Feature kBrowserSchedulerInitParams = {
    "BrowserSchedulerInitParams", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRendererSchedulerInitParams = {
    "RendererSchedulerInitParams", base::FEATURE_DISABLED_BY_DEFAULT};

std::unique_ptr<base::TaskScheduler::InitParams> GetTaskSchedulerInitParams(
    const base::Feature& feature) {
  std::map<std::string, std::string> variation_params;
  if (!base::GetFieldTrialParamsByFeature(feature, &variation_params))
    return nullptr;

  const auto background_worker_pool_params =
      GetWorkerPoolParams("Background", variation_params);
  const auto background_blocking_worker_pool_params =
      GetWorkerPoolParams("BackgroundBlocking", variation_params);
  const auto foreground_worker_pool_params =
      GetWorkerPoolParams("Foreground", variation_params);
  const auto foreground_blocking_worker_pool_params =
      GetWorkerPoolParams("ForegroundBlocking", variation_params);

  if (!background_worker_pool_params ||
      !background_blocking_worker_pool_params ||
      !foreground_worker_pool_params ||
      !foreground_blocking_worker_pool_params) {
    return nullptr;
  }

  return std::make_unique<base::TaskScheduler::InitParams>(
      *background_worker_pool_params, *background_blocking_worker_pool_params,
      *foreground_worker_pool_params, *foreground_blocking_worker_pool_params);
}

std::unique_ptr<base::TaskScheduler::InitParams>
GetTaskSchedulerInitParamsForBrowser() {
  // Variations params for the browser processes are associated with the feature
  // |kBrowserSchedulerInitParams|.
  return GetTaskSchedulerInitParams(kBrowserSchedulerInitParams);
}

std::unique_ptr<base::TaskScheduler::InitParams>
GetTaskSchedulerInitParamsForRenderer() {
  // Variations params for the renderer processes are associated with the
  // feature |kRendererSchedulerInitParams|.
  return GetTaskSchedulerInitParams(kRendererSchedulerInitParams);
}

}  // namespace task_scheduler_util
