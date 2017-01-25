// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TASK_SCHEDULER_UTIL_COMMON_VARIATIONS_UTIL_H_
#define COMPONENTS_TASK_SCHEDULER_UTIL_COMMON_VARIATIONS_UTIL_H_

#include <map>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "base/strings/string_piece.h"
#include "base/task_scheduler/scheduler_worker_params.h"
#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/threading/platform_thread.h"

namespace base {
class CommandLine;
}

namespace task_scheduler_util {

class SchedulerImmutableWorkerPoolParams {
 public:
  SchedulerImmutableWorkerPoolParams(
      const char* name,
      base::ThreadPriority priority_hint,
      base::SchedulerBackwardCompatibility backward_compatibility =
          base::SchedulerBackwardCompatibility::DISABLED);

  const char* name() const { return name_; }
  base::ThreadPriority priority_hint() const { return priority_hint_; }
  base::SchedulerBackwardCompatibility backward_compatibility() const {
    return backward_compatibility_;
  }

 private:
  const char* name_;
  base::ThreadPriority priority_hint_;
  base::SchedulerBackwardCompatibility backward_compatibility_;
};

// Returns a SchedulerWorkerPoolParams vector to initialize pools specified in
// |constant_worker_pool_params_vector|. SchedulerWorkerPoolParams members
// without a counterpart in SchedulerImmutableWorkerPoolParams are initialized
// based of |variation_params|. Returns an empty vector on failure.
std::vector<base::SchedulerWorkerPoolParams> GetWorkerPoolParams(
    const std::vector<SchedulerImmutableWorkerPoolParams>&
        constant_worker_pool_params_vector,
    const std::map<std::string, std::string>& variation_params);

#if !defined(OS_IOS)
// Serializes variation params from the BrowserScheduler field trial whose key
// start with |prefix| to the --task-scheduler-variation-params switch of
// |command_line|.
void AddVariationParamsToCommandLine(base::StringPiece key_prefix,
                                     base::CommandLine* command_line);

// Returns a map of key-value pairs deserialized from the
// --task-scheduler-variation-params switch of |command_line|.
std::map<std::string, std::string> GetVariationParamsFromCommandLine(
    const base::CommandLine& command_line);
#endif  // !defined(OS_IOS)

}  // namespace task_scheduler_util

#endif  // COMPONENTS_TASK_SCHEDULER_UTIL_COMMON_VARIATIONS_UTIL_H_
