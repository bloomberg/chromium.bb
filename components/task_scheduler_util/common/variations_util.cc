// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/common/variations_util.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/initialization_util.h"
#include "base/time/time.h"
#include "components/variations/variations_associated_data.h"

namespace task_scheduler_util {

namespace {

struct SchedulerCustomizableWorkerPoolParams {
  base::SchedulerWorkerPoolParams::StandbyThreadPolicy standby_thread_policy;
  int max_threads = 0;
  base::TimeDelta detach_period;
};

#if !defined(OS_IOS)
constexpr char kTaskSchedulerVariationParamsSwitch[] =
    "task-scheduler-variation-params";

constexpr char kSeparator[] = "|";

bool ContainsSeparator(const std::string& str) {
  return str.find(kSeparator) != std::string::npos;
}
#endif  // !defined(OS_IOS)

// Converts |pool_descriptor| to a SchedulerWorkerPoolVariableParams. Returns a
// default SchedulerWorkerPoolVariableParams on failure.
//
// |pool_descriptor| is a semi-colon separated value string with the following
// items:
// 0. Minimum Thread Count (int)
// 1. Maximum Thread Count (int)
// 2. Thread Count Multiplier (double)
// 3. Thread Count Offset (int)
// 4. Detach Time in Milliseconds (int)
// 5. Standby Thread Policy (string)
// Additional values may appear as necessary and will be ignored.
SchedulerCustomizableWorkerPoolParams StringToVariableWorkerPoolParams(
    const base::StringPiece pool_descriptor) {
  using StandbyThreadPolicy =
      base::SchedulerWorkerPoolParams::StandbyThreadPolicy;
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
  if (tokens.size() >= 5 && base::StringToInt(tokens[0], &min) &&
      base::StringToInt(tokens[1], &max) &&
      base::StringToDouble(tokens[2].as_string(), &cores_multiplier) &&
      base::StringToInt(tokens[3], &offset) &&
      base::StringToInt(tokens[4], &detach_milliseconds)) {
    SchedulerCustomizableWorkerPoolParams params;
    params.max_threads = base::RecommendedMaxNumberOfThreadsInPool(
        min, max, cores_multiplier, offset);
    params.detach_period =
        base::TimeDelta::FromMilliseconds(detach_milliseconds);
    params.standby_thread_policy = (tokens.size() >= 6 && tokens[5] == "lazy")
                                       ? StandbyThreadPolicy::LAZY
                                       : StandbyThreadPolicy::ONE;
    return params;
  }
  DLOG(ERROR) << "Invalid Worker Pool Descriptor: " << pool_descriptor;
  return SchedulerCustomizableWorkerPoolParams();
}

}  // namespace

SchedulerImmutableWorkerPoolParams::SchedulerImmutableWorkerPoolParams(
    const char* name,
    base::ThreadPriority priority_hint,
    base::SchedulerBackwardCompatibility backward_compatibility)
    : name_(name),
      priority_hint_(priority_hint),
      backward_compatibility_(backward_compatibility) {}

std::vector<base::SchedulerWorkerPoolParams> GetWorkerPoolParams(
    const std::vector<SchedulerImmutableWorkerPoolParams>&
        constant_worker_pool_params_vector,
    const std::map<std::string, std::string>& variation_params) {
  std::vector<base::SchedulerWorkerPoolParams> worker_pool_params_vector;
  for (const auto& constant_worker_pool_params :
       constant_worker_pool_params_vector) {
    const char* const worker_pool_name = constant_worker_pool_params.name();
    auto it = variation_params.find(worker_pool_name);
    if (it == variation_params.end()) {
      // Non-branded builds don't have access to external worker pool
      // configurations.
      return std::vector<base::SchedulerWorkerPoolParams>();
    }
    const auto variable_worker_pool_params =
        StringToVariableWorkerPoolParams(it->second);
    if (variable_worker_pool_params.max_threads <= 0 ||
        variable_worker_pool_params.detach_period <= base::TimeDelta()) {
      DLOG(ERROR) << "Invalid Worker Pool Configuration: " << worker_pool_name
                  << " [" << it->second << "]";
      return std::vector<base::SchedulerWorkerPoolParams>();
    }
    worker_pool_params_vector.emplace_back(
        worker_pool_name, constant_worker_pool_params.priority_hint(),
        variable_worker_pool_params.standby_thread_policy,
        variable_worker_pool_params.max_threads,
        variable_worker_pool_params.detach_period,
        constant_worker_pool_params.backward_compatibility());
  }
  return worker_pool_params_vector;
}

#if !defined(OS_IOS)
void AddVariationParamsToCommandLine(base::StringPiece key_prefix,
                                     base::CommandLine* command_line) {
  DCHECK(command_line);

  std::map<std::string, std::string> variation_params;
  if (!variations::GetVariationParams("BrowserScheduler", &variation_params))
    return;

  std::vector<std::string> parts;
  for (const auto& key_value : variation_params) {
    if (base::StartsWith(key_value.first, key_prefix,
                         base::CompareCase::SENSITIVE)) {
      if (ContainsSeparator(key_value.first) ||
          ContainsSeparator(key_value.second)) {
        DLOG(ERROR)
            << "Unexpected Character in Task Scheduler Variation Params: "
            << key_value.first << " [" << key_value.second << "]";
        return;
      }
      parts.push_back(key_value.first);
      parts.push_back(key_value.second);
    }
  }

  if (!parts.empty()) {
    command_line->AppendSwitchASCII(kTaskSchedulerVariationParamsSwitch,
                                    base::JoinString(parts, kSeparator));
  }
}

std::map<std::string, std::string> GetVariationParamsFromCommandLine(
    const base::CommandLine& command_line) {
  const auto serialized_variation_params =
      command_line.GetSwitchValueASCII(kTaskSchedulerVariationParamsSwitch);
  const auto parts =
      base::SplitStringPiece(serialized_variation_params, kSeparator,
                             base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::map<std::string, std::string> variation_params;
  for (auto it = parts.begin(); it != parts.end(); ++it) {
    base::StringPiece key = *it;
    ++it;
    if (it == parts.end()) {
      NOTREACHED();
      return std::map<std::string, std::string>();
    }
    base::StringPiece value = *it;
    variation_params[key.as_string()] = value.as_string();
  }
  return variation_params;
}
#endif  // !defined(OS_IOS)

}  // namespace task_scheduler_util
