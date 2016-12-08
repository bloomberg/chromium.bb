// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/initialization/browser_util.h"

#include <map>
#include <string>

#include "base/task_scheduler/initialization_util.h"
#include "base/task_scheduler/switches.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/sequenced_worker_pool.h"
#include "build/build_config.h"

namespace task_scheduler_util {
namespace initialization {

namespace {

using StandbyThreadPolicy =
    base::SchedulerWorkerPoolParams::StandbyThreadPolicy;
using ThreadPriority = base::ThreadPriority;

struct SchedulerWorkerPoolCustomizableConfiguration {
  SchedulerWorkerPoolCustomizableConfiguration(
      const char* name_in,
      ThreadPriority priority_hint_in,
      const SingleWorkerPoolConfiguration& single_worker_pool_config_in)
      : name(name_in),
        priority_hint(priority_hint_in),
        single_worker_pool_config(single_worker_pool_config_in) {}

  const char* name;
  ThreadPriority priority_hint;
  const SingleWorkerPoolConfiguration& single_worker_pool_config;
};

}  // namespace

std::vector<base::SchedulerWorkerPoolParams>
BrowserWorkerPoolConfigurationToSchedulerWorkerPoolParams(
    const BrowserWorkerPoolsConfiguration& config) {
  const SchedulerWorkerPoolCustomizableConfiguration worker_pool_config[] = {
      SchedulerWorkerPoolCustomizableConfiguration("Background",
                                                   ThreadPriority::BACKGROUND,
                                                   config.background),
      SchedulerWorkerPoolCustomizableConfiguration("BackgroundFileIO",
                                                   ThreadPriority::BACKGROUND,
                                                   config.background_file_io),
      SchedulerWorkerPoolCustomizableConfiguration("Foreground",
                                                   ThreadPriority::NORMAL,
                                                   config.foreground),
      SchedulerWorkerPoolCustomizableConfiguration("ForegroundFileIO",
                                                   ThreadPriority::NORMAL,
                                                   config.foreground_file_io),

  };
  static_assert(arraysize(worker_pool_config) == WORKER_POOL_COUNT,
                "Mismatched Worker Pool Types and Predefined Parameters");
  constexpr size_t kNumWorkerPoolsDefined = sizeof(config) /
                                            sizeof(config.background);
  static_assert(arraysize(worker_pool_config) == kNumWorkerPoolsDefined,
                "Mismatch in predefined parameters and worker pools.");
  std::vector<base::SchedulerWorkerPoolParams> params_vector;
  for (const auto& config : worker_pool_config) {
    params_vector.emplace_back(
        config.name, config.priority_hint,
        config.single_worker_pool_config.standby_thread_policy,
        config.single_worker_pool_config.threads,
        config.single_worker_pool_config.detach_period);
  }
  DCHECK_EQ(WORKER_POOL_COUNT, params_vector.size());
  return params_vector;
}

// Returns the worker pool index for |traits| defaulting to FOREGROUND or
// FOREGROUND_FILE_IO on any priorities other than background.
size_t BrowserWorkerPoolIndexForTraits(const base::TaskTraits& traits) {
  const bool is_background =
      traits.priority() == base::TaskPriority::BACKGROUND;
  if (traits.with_file_io())
    return is_background ? BACKGROUND_FILE_IO : FOREGROUND_FILE_IO;

  return is_background ? BACKGROUND : FOREGROUND;
}

std::vector<base::SchedulerWorkerPoolParams>
GetDefaultBrowserSchedulerWorkerPoolParams() {
  constexpr size_t kNumWorkerPoolsDefined =
      sizeof(BrowserWorkerPoolsConfiguration) /
      sizeof(SingleWorkerPoolConfiguration);
  static_assert(kNumWorkerPoolsDefined == 4,
                "Expected 4 worker pools in BrowserWorkerPoolsConfiguration");
  BrowserWorkerPoolsConfiguration config;
#if defined(OS_ANDROID) || defined(OS_IOS)
  config.background.standby_thread_policy = StandbyThreadPolicy::ONE;
  config.background.threads =
      base::RecommendedMaxNumberOfThreadsInPool(2, 8, 0.1, 0);
  config.background.detach_period = base::TimeDelta::FromSeconds(30);

  config.background_file_io.standby_thread_policy = StandbyThreadPolicy::ONE;
  config.background_file_io.threads =
      base::RecommendedMaxNumberOfThreadsInPool(2, 8, 0.1, 0);
  config.background_file_io.detach_period = base::TimeDelta::FromSeconds(30);

  config.foreground.standby_thread_policy = StandbyThreadPolicy::ONE;
  config.foreground.threads =
      base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.3, 0);
  config.foreground.detach_period = base::TimeDelta::FromSeconds(30);

  config.foreground_file_io.standby_thread_policy = StandbyThreadPolicy::ONE;
  config.foreground_file_io.threads =
      base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.3, 0);
  config.foreground_file_io.detach_period = base::TimeDelta::FromSeconds(30);
#else
  config.background.standby_thread_policy = StandbyThreadPolicy::ONE;
  config.background.threads =
      base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.1, 0);
  config.background.detach_period = base::TimeDelta::FromSeconds(30);

  config.background_file_io.standby_thread_policy = StandbyThreadPolicy::ONE;
  config.background_file_io.threads =
      base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.1, 0);
  config.background_file_io.detach_period = base::TimeDelta::FromSeconds(30);

  config.foreground.standby_thread_policy = StandbyThreadPolicy::ONE;
  config.foreground.threads =
      base::RecommendedMaxNumberOfThreadsInPool(8, 32, 0.3, 0);
  config.foreground.detach_period = base::TimeDelta::FromSeconds(30);

  config.foreground_file_io.standby_thread_policy = StandbyThreadPolicy::ONE;
  config.foreground_file_io.threads =
      base::RecommendedMaxNumberOfThreadsInPool(8, 32, 0.3, 0);
  config.foreground_file_io.detach_period = base::TimeDelta::FromSeconds(30);
#endif
  return BrowserWorkerPoolConfigurationToSchedulerWorkerPoolParams(config);
}

}  // namespace initialization
}  // namespace task_scheduler_util
