// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/initialization_util.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/task_scheduler/initialization_util.h"
#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task_scheduler/switches.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/variations/variations_associated_data.h"

namespace task_scheduler_util {

namespace {

using StandbyThreadPolicy =
    base::SchedulerWorkerPoolParams::StandbyThreadPolicy;

enum WorkerPoolType : size_t {
  BACKGROUND_WORKER_POOL = 0,
  BACKGROUND_FILE_IO_WORKER_POOL,
  FOREGROUND_WORKER_POOL,
  FOREGROUND_FILE_IO_WORKER_POOL,
  WORKER_POOL_COUNT  // Always last.
};

struct WorkerPoolVariationValues {
  StandbyThreadPolicy standby_thread_policy;
  int threads = 0;
  base::TimeDelta detach_period;
};

// Converts |pool_descriptor| to a WorkerPoolVariationValues. Returns a default
// WorkerPoolVariationValues on failure.
//
// |pool_descriptor| is a semi-colon separated value string with the following
// items:
// 0. Minimum Thread Count (int)
// 1. Maximum Thread Count (int)
// 2. Thread Count Multiplier (double)
// 3. Thread Count Offset (int)
// 4. Detach Time in Milliseconds (milliseconds)
// 5. Standby Thread Policy (string)
// Additional values may appear as necessary and will be ignored.
WorkerPoolVariationValues StringToWorkerPoolVariationValues(
    const base::StringPiece pool_descriptor) {
  const std::vector<std::string> tokens =
      SplitString(pool_descriptor, ";",
                  base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int min;
  int max;
  double cores_multiplier;
  int offset;
  int detach_milliseconds;
  // Checking for a size greater than the expected amount allows us to be
  // forward compatible if we add more variation values.
  if (tokens.size() >= 5 && base::StringToInt(tokens[0], &min) &&
      base::StringToInt(tokens[1], &max) &&
      base::StringToDouble(tokens[2], &cores_multiplier) &&
      base::StringToInt(tokens[3], &offset) &&
      base::StringToInt(tokens[4], &detach_milliseconds)) {
    WorkerPoolVariationValues values;
    values.threads = base::RecommendedMaxNumberOfThreadsInPool(
        min, max, cores_multiplier, offset);
    values.detach_period =
        base::TimeDelta::FromMilliseconds(detach_milliseconds);
    values.standby_thread_policy =
        (tokens.size() >= 6 && tokens[5] == "lazy")
            ? StandbyThreadPolicy::LAZY
            : StandbyThreadPolicy::ONE;
    return values;
  }
  DLOG(ERROR) << "Invalid Worker Pool Descriptor: " << pool_descriptor;
  return WorkerPoolVariationValues();
}

// Returns the worker pool index for |traits| defaulting to
// FOREGROUND_WORKER_POOL or FOREGROUND_FILE_IO_WORKER_POOL on unknown
// priorities.
size_t WorkerPoolIndexForTraits(const base::TaskTraits& traits) {
  const bool is_background =
      traits.priority() == base::TaskPriority::BACKGROUND;
  if (traits.with_file_io()) {
    return is_background ? BACKGROUND_FILE_IO_WORKER_POOL
                         : FOREGROUND_FILE_IO_WORKER_POOL;
  }
  return is_background ? BACKGROUND_WORKER_POOL : FOREGROUND_WORKER_POOL;
}

std::map<std::string, std::string> GetDefaultBrowserVariationParams() {
  std::map<std::string, std::string> variation_params;
#if defined(OS_ANDROID) || defined(OS_IOS)
  variation_params["Background"] = "2;8;0.1;0;30000";
  variation_params["BackgroundFileIO"] = "2;8;0.1;0;30000";
  variation_params["Foreground"] = "3;8;0.3;0;30000";
  variation_params["ForegroundFileIO"] = "3;8;0.3;0;30000";
#else
  variation_params["Background"] = "3;8;0.1;0;30000";
  variation_params["BackgroundFileIO"] = "3;8;0.1;0;30000";
  variation_params["Foreground"] = "8;32;0.3;0;30000";
  variation_params["ForegroundFileIO"] = "8;32;0.3;0;30000";
#endif  // defined(OS_ANDROID) || defined(OS_IOS)
  return variation_params;
}

// Converts a browser-based |variation_params| to
// std::vector<base::SchedulerWorkerPoolParams>. Returns an empty vector on
// failure.
std::vector<base::SchedulerWorkerPoolParams>
VariationsParamsToBrowserSchedulerWorkerPoolParams(
    const std::map<std::string, std::string>& variation_params) {
  using ThreadPriority = base::ThreadPriority;
  struct SchedulerWorkerPoolPredefinedParams {
    const char* name;
    ThreadPriority priority_hint;
  };
  static const SchedulerWorkerPoolPredefinedParams kAllPredefinedParams[] = {
      {"Background", ThreadPriority::BACKGROUND},
      {"BackgroundFileIO", ThreadPriority::BACKGROUND},
      {"Foreground", ThreadPriority::NORMAL},
      {"ForegroundFileIO", ThreadPriority::NORMAL},
  };
  static_assert(arraysize(kAllPredefinedParams) == WORKER_POOL_COUNT,
                "Mismatched Worker Pool Types and Predefined Parameters");
  std::vector<base::SchedulerWorkerPoolParams> params_vector;
  for (const auto& predefined_params : kAllPredefinedParams) {
    const auto pair = variation_params.find(predefined_params.name);
    if (pair == variation_params.end()) {
      DLOG(ERROR) << "Missing Worker Pool Configuration: "
                  << predefined_params.name;
      return std::vector<base::SchedulerWorkerPoolParams>();
    }

    const WorkerPoolVariationValues variation_values =
        StringToWorkerPoolVariationValues(pair->second);

    if (variation_values.threads <= 0 ||
        variation_values.detach_period.is_zero()) {
      DLOG(ERROR) << "Invalid Worker Pool Configuration: " <<
          predefined_params.name << " [" << pair->second << "]";
      return std::vector<base::SchedulerWorkerPoolParams>();
    }

    params_vector.emplace_back(predefined_params.name,
                               predefined_params.priority_hint,
                               variation_values.standby_thread_policy,
                               variation_values.threads,
                               variation_values.detach_period);
  }
  DCHECK_EQ(WORKER_POOL_COUNT, params_vector.size());
  return params_vector;
}

}  // namespace

void InitializeDefaultBrowserTaskScheduler() {
  static constexpr char kFieldTrialName[] = "BrowserScheduler";
  std::map<std::string, std::string> variation_params;
  if (!variations::GetVariationParams(kFieldTrialName, &variation_params))
    variation_params = GetDefaultBrowserVariationParams();

  auto params_vector =
      VariationsParamsToBrowserSchedulerWorkerPoolParams(variation_params);
  if (params_vector.empty()) {
    variation_params = GetDefaultBrowserVariationParams();
    params_vector =
        VariationsParamsToBrowserSchedulerWorkerPoolParams(variation_params);
    DCHECK(!params_vector.empty());
  }
  base::TaskScheduler::CreateAndSetDefaultTaskScheduler(
      params_vector, base::Bind(WorkerPoolIndexForTraits));

  // TODO(gab): Remove this when http://crbug.com/622400 concludes.
  const auto sequenced_worker_pool_param =
      variation_params.find("RedirectSequencedWorkerPools");
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableBrowserTaskScheduler) &&
      sequenced_worker_pool_param != variation_params.end() &&
      sequenced_worker_pool_param->second == "true") {
    base::SequencedWorkerPool::EnableWithRedirectionToTaskSchedulerForProcess();
  } else {
    base::SequencedWorkerPool::EnableForProcess();
  }
}

}  // namespace task_scheduler_util
