// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TASK_SCHEDULER_UTIL_COMMON_VARIATIONS_UTIL_H_
#define COMPONENTS_TASK_SCHEDULER_UTIL_COMMON_VARIATIONS_UTIL_H_

#include <map>
#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "base/task_scheduler/scheduler_worker_params.h"
#include "base/task_scheduler/task_scheduler.h"
#include "build/build_config.h"

namespace base {
class CommandLine;
}

namespace task_scheduler_util {

// Builds a TaskScheduler::InitParams from pool descriptors in
// |variations_params| that are prefixed with |variation_param_prefix|.
// |foreground_blocking_backward_compatibility| controls backward compatibility
// in the foreground blocking pool. Returns nullptr on failure.
std::unique_ptr<base::TaskScheduler::InitParams> GetTaskSchedulerInitParams(
    base::StringPiece variation_param_prefix,
    const std::map<std::string, std::string>& variation_params,
    base::SchedulerBackwardCompatibility
        foreground_blocking_backward_compatibility =
            base::SchedulerBackwardCompatibility::DISABLED);

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
