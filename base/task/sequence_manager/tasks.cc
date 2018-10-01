// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/tasks.h"

namespace base {
namespace sequence_manager {
namespace internal {

PostedTask::PostedTask(OnceClosure callback,
                       Location location,
                       TimeDelta delay,
                       Nestable nestable,
                       int task_type)
    : callback(std::move(callback)),
      location(location),
      delay(delay),
      nestable(nestable),
      task_type(task_type) {}

PostedTask::PostedTask(PostedTask&& move_from) noexcept
    : callback(std::move(move_from.callback)),
      location(move_from.location),
      delay(move_from.delay),
      nestable(move_from.nestable),
      task_type(move_from.task_type) {}

PostedTask::~PostedTask() = default;

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
