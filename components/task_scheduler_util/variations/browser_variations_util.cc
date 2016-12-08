// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/variations/browser_variations_util.h"

#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/task_scheduler/initialization_util.h"
#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task_scheduler/switches.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/task_scheduler_util/initialization/browser_util.h"
#include "components/variations/variations_associated_data.h"

namespace task_scheduler_util {
namespace variations {

namespace {

constexpr char kFieldTrialName[] = "BrowserScheduler";

// Converts |pool_descriptor| to a SingleWorkerPoolConfiguration. Returns a
// default SingleWorkerPoolConfiguration on failure.
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
initialization::SingleWorkerPoolConfiguration
StringToSingleWorkerPoolConfiguration(const base::StringPiece pool_descriptor) {
  using StandbyThreadPolicy =
      base::SchedulerWorkerPoolParams::StandbyThreadPolicy;
  const std::vector<base::StringPiece> tokens = SplitStringPiece(
      pool_descriptor, ";", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // Normally, we wouldn't initialize the values below because we don't read
  // from them before we write to them. However, some compilers (like MSVC)
  // complain about uninitialized varaibles due to the as_string() call below.
  int min = 0;
  int max = 0;
  double cores_multiplier = 0.0;
  int offset = 0;
  int detach_milliseconds = 0;
  // Checking for a size greater than the expected amount allows us to be
  // forward compatible if we add more variation values.
  if (tokens.size() >= 5 && base::StringToInt(tokens[0], &min) &&
      base::StringToInt(tokens[1], &max) &&
      base::StringToDouble(tokens[2].as_string(), &cores_multiplier) &&
      base::StringToInt(tokens[3], &offset) &&
      base::StringToInt(tokens[4], &detach_milliseconds)) {
    initialization::SingleWorkerPoolConfiguration config;
    config.threads = base::RecommendedMaxNumberOfThreadsInPool(
        min, max, cores_multiplier, offset);
    config.detach_period =
        base::TimeDelta::FromMilliseconds(detach_milliseconds);
    config.standby_thread_policy = (tokens.size() >= 6 && tokens[5] == "lazy")
                                       ? StandbyThreadPolicy::LAZY
                                       : StandbyThreadPolicy::ONE;
    return config;
  }
  DLOG(ERROR) << "Invalid Worker Pool Descriptor: " << pool_descriptor;
  return initialization::SingleWorkerPoolConfiguration();
}

// Converts a browser-based |variation_params| to
// std::vector<base::SchedulerWorkerPoolParams>. Returns an empty vector on
// failure.
std::vector<base::SchedulerWorkerPoolParams>
VariationsParamsToSchedulerWorkerPoolParamsVector(
    const std::map<std::string, std::string>& variation_params) {
  static const char* const kWorkerPoolNames[] = {
      "Background", "BackgroundFileIO", "Foreground", "ForegroundFileIO"};
  static_assert(
      arraysize(kWorkerPoolNames) == initialization::WORKER_POOL_COUNT,
      "Mismatched Worker Pool Types and Worker Pool Names");
  initialization::BrowserWorkerPoolsConfiguration config;
  initialization::SingleWorkerPoolConfiguration* const all_pools[]{
      &config.background, &config.background_file_io, &config.foreground,
      &config.foreground_file_io,
  };
  static_assert(arraysize(kWorkerPoolNames) == arraysize(all_pools),
                "Mismatched Worker Pool Names and All Pools Array");
  for (size_t i = 0; i < arraysize(kWorkerPoolNames); ++i) {
    const auto* const worker_pool_name = kWorkerPoolNames[i];
    const auto pair = variation_params.find(worker_pool_name);
    if (pair == variation_params.end()) {
      DLOG(ERROR) << "Missing Worker Pool Configuration: " << worker_pool_name;
      return std::vector<base::SchedulerWorkerPoolParams>();
    }

    auto* const pool_config = all_pools[i];
    *pool_config = StringToSingleWorkerPoolConfiguration(pair->second);
    if (pool_config->threads <= 0 ||
        pool_config->detach_period <= base::TimeDelta()) {
      DLOG(ERROR) << "Invalid Worker Pool Configuration: " << worker_pool_name
                  << " [" << pair->second << "]";
      return std::vector<base::SchedulerWorkerPoolParams>();
    }
  }
  return BrowserWorkerPoolConfigurationToSchedulerWorkerPoolParams(config);
}

}  // namespace

std::vector<base::SchedulerWorkerPoolParams>
GetBrowserSchedulerWorkerPoolParamsFromVariations() {
  std::map<std::string, std::string> variation_params;
  if (!::variations::GetVariationParams(kFieldTrialName, &variation_params))
    return std::vector<base::SchedulerWorkerPoolParams>();

  return VariationsParamsToSchedulerWorkerPoolParamsVector(variation_params);
}

void MaybePerformBrowserTaskSchedulerRedirection() {
  std::map<std::string, std::string> variation_params;
  ::variations::GetVariationParams(kFieldTrialName, &variation_params);

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

}  // variations
}  // namespace task_scheduler_util
