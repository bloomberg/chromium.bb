// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/task_scheduler_util/renderer/initialization.h"

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/task_scheduler/task_traits.h"
#include "base/threading/platform_thread.h"
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

std::vector<base::SchedulerWorkerPoolParams> GetRendererWorkerPoolParams() {
  using ThreadPriority = base::ThreadPriority;
  std::vector<SchedulerImmutableWorkerPoolParams> immutable_worker_pool_params;
  DCHECK_EQ(BACKGROUND, immutable_worker_pool_params.size());
  immutable_worker_pool_params.emplace_back("RendererBackground",
                                            ThreadPriority::BACKGROUND);
  DCHECK_EQ(BACKGROUND_BLOCKING, immutable_worker_pool_params.size());
  immutable_worker_pool_params.emplace_back("RendererBackgroundBlocking",
                                            ThreadPriority::BACKGROUND);
  DCHECK_EQ(FOREGROUND, immutable_worker_pool_params.size());
  immutable_worker_pool_params.emplace_back("RendererForeground",
                                            ThreadPriority::NORMAL);
  DCHECK_EQ(FOREGROUND_BLOCKING, immutable_worker_pool_params.size());
  immutable_worker_pool_params.emplace_back("RendererForegroundBlocking",
                                            ThreadPriority::NORMAL);
  return GetWorkerPoolParams(immutable_worker_pool_params,
                             GetVariationParamsFromCommandLine(
                                 *base::CommandLine::ForCurrentProcess()));
}

size_t RendererWorkerPoolIndexForTraits(const base::TaskTraits& traits) {
  const bool is_background =
      traits.priority() == base::TaskPriority::BACKGROUND;
  if (traits.may_block() || traits.with_base_sync_primitives())
    return is_background ? BACKGROUND_BLOCKING : FOREGROUND_BLOCKING;
  return is_background ? BACKGROUND : FOREGROUND;
}

}  // namespace task_scheduler_util
