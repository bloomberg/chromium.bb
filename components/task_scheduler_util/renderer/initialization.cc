// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/renderer/initialization.h"

#include "base/command_line.h"
#include "components/task_scheduler_util/common/variations_util.h"

namespace task_scheduler_util {

std::unique_ptr<base::TaskScheduler::InitParams>
GetRendererTaskSchedulerInitParamsFromCommandLine() {
  return GetTaskSchedulerInitParams(
      "Renderer", GetVariationParamsFromCommandLine(
                      *base::CommandLine::ForCurrentProcess()));
}

}  // namespace task_scheduler_util
