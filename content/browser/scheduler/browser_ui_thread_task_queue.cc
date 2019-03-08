// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_ui_thread_task_queue.h"

#include <utility>

#include "base/task/sequence_manager/task_queue_impl.h"

namespace content {

BrowserUIThreadTaskQueue::BrowserUIThreadTaskQueue(
    std::unique_ptr<base::sequence_manager::internal::TaskQueueImpl> impl,
    const TaskQueue::Spec& spec,
    QueueType queue_type)
    : base::sequence_manager::TaskQueue(std::move(impl), spec),
      queue_type_(queue_type) {}

BrowserUIThreadTaskQueue::~BrowserUIThreadTaskQueue() = default;

// static
const char* BrowserUIThreadTaskQueue::NameForQueueType(QueueType queue_type) {
  switch (queue_type) {
    case QueueType::kBestEffort:
      return "best_effort_tq";
    case QueueType::kBootstrap:
      return "bootstrap_tq";
    case QueueType::kNavigation:
      return "navigation_tq";
    case QueueType::kDefault:
      return "default_tq";
    case QueueType::kUserBlocking:
      return "user_blocking_tq";
    case QueueType::kCount:
      break;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace content
