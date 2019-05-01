// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_group_params.h"

namespace base {

ThreadGroupParams::ThreadGroupParams(
    int max_tasks,
    TimeDelta suggested_reclaim_time,
    WorkerThreadBackwardCompatibility backward_compatibility)
    : max_tasks_(max_tasks),
      suggested_reclaim_time_(suggested_reclaim_time),
      backward_compatibility_(backward_compatibility) {}

ThreadGroupParams::ThreadGroupParams(const ThreadGroupParams& other) = default;

ThreadGroupParams& ThreadGroupParams::operator=(
    const ThreadGroupParams& other) = default;

}  // namespace base
