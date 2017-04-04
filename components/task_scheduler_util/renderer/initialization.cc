// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/renderer/initialization.h"

#include "base/command_line.h"
#include "base/task_scheduler/task_traits.h"
#include "components/task_scheduler_util/common/variations_util.h"

namespace task_scheduler_util {

namespace {

enum WorkerPoolType : size_t {
  BACKGROUND = 0,
  BACKGROUND_BLOCKING,
  FOREGROUND,
  FOREGROUND_BLOCKING,
  WORKER_POOL_COUNT  // Always last.
};

}  // namespace

std::unique_ptr<base::TaskScheduler::InitParams>
GetRendererTaskSchedulerInitParamsFromCommandLine() {
  return GetTaskSchedulerInitParams(
      "Renderer", GetVariationParamsFromCommandLine(
                      *base::CommandLine::ForCurrentProcess()));
}

std::vector<base::SchedulerWorkerPoolParams> GetRendererWorkerPoolParams() {
  const auto init_params = GetRendererTaskSchedulerInitParamsFromCommandLine();
  if (!init_params)
    return std::vector<base::SchedulerWorkerPoolParams>();

  return std::vector<base::SchedulerWorkerPoolParams>{
      init_params->background_worker_pool_params,
      init_params->background_blocking_worker_pool_params,
      init_params->foreground_worker_pool_params,
      init_params->foreground_blocking_worker_pool_params};
}

size_t RendererWorkerPoolIndexForTraits(const base::TaskTraits& traits) {
  const bool is_background =
      traits.priority() == base::TaskPriority::BACKGROUND;
  if (traits.may_block() || traits.with_base_sync_primitives())
    return is_background ? BACKGROUND_BLOCKING : FOREGROUND_BLOCKING;
  return is_background ? BACKGROUND : FOREGROUND;
}

}  // namespace task_scheduler_util
