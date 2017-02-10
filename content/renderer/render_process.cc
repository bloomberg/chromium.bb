// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "content/renderer/render_process.h"

namespace content {

RenderProcess::RenderProcess(
    const std::vector<base::SchedulerWorkerPoolParams>& worker_pool_params,
    base::TaskScheduler::WorkerPoolIndexForTraitsCallback
        worker_pool_index_for_traits_callback)
    : ChildProcess(base::ThreadPriority::NORMAL,
                   worker_pool_params,
                   std::move(worker_pool_index_for_traits_callback)) {}

}  // namespace content
