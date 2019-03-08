// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_TASK_QUEUE_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_TASK_QUEUE_H_

#include <memory>

#include "base/task/sequence_manager/task_queue.h"
#include "content/common/content_export.h"

namespace base {
namespace sequence_manager {
// TODO(carlscab): Refactor creation of task queues so we don't need to expose
// the internal namespace.
namespace internal {
class TaskQueueImpl;
}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

namespace content {

// There are a number of BrowserUIThreadTaskQueues and all UI thread tasks are
// posted to one of these. The scheduler manipulates these queues in order to
// implement scheduling policy.
class CONTENT_EXPORT BrowserUIThreadTaskQueue
    : public base::sequence_manager::TaskQueue {
 public:
  enum class QueueType {
    // Catch all for tasks that don't fit other categories.
    // TODO(alexclarke): Introduce new semantic types as needed to minimize the
    // number of default tasks.
    kDefault,

    // For non-urgent work, that will only execute if there's nothing else to
    // do. Can theoretically be starved indefinitely although that's unlikely in
    // practice.
    kBestEffort,

    // For tasks on the critical path up to issuing the initial navigation.
    kBootstrap,

    // For navigation related tasks.
    kNavigation,

    // A generic high priority queue.  Long term we should replace this with
    // additional semantic annotations.
    kUserBlocking,

    kCount
  };

  BrowserUIThreadTaskQueue(
      std::unique_ptr<base::sequence_manager::internal::TaskQueueImpl> impl,
      const TaskQueue::Spec& spec,
      QueueType queue_type);

  QueueType queue_type() const { return queue_type_; }

  // Returns name of the given queue type. Returned string has application
  // lifetime.
  static const char* NameForQueueType(QueueType queue_type);

 private:
  ~BrowserUIThreadTaskQueue() override;

  QueueType queue_type_;

  DISALLOW_COPY_AND_ASSIGN(BrowserUIThreadTaskQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_TASK_QUEUE_H_
