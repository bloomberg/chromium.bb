// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_SCHEDULER_WORKER_THREAD_STACK_H_
#define BASE_TASK_SCHEDULER_SCHEDULER_WORKER_THREAD_STACK_H_

#include <stddef.h>

#include <vector>

#include "base/base_export.h"
#include "base/macros.h"

namespace base {
namespace internal {

class SchedulerWorkerThread;

// A stack of SchedulerWorkerThreads. Supports removal of arbitrary
// SchedulerWorkerThreads. DCHECKs when a SchedulerWorkerThread is inserted
// multiple times. SchedulerWorkerThreads are not owned by the stack. Push() is
// amortized O(1). Pop(), Size() and Empty() are O(1). Remove is O(n). This
// class is NOT thread-safe.
class BASE_EXPORT SchedulerWorkerThreadStack {
 public:
  SchedulerWorkerThreadStack();
  ~SchedulerWorkerThreadStack();

  // Inserts |worker_thread| at the top of the stack. |worker_thread| must not
  // already be on the stack.
  void Push(SchedulerWorkerThread* worker_thread);

  // Removes the top SchedulerWorkerThread from the stack and returns it.
  // Returns nullptr if the stack is empty.
  SchedulerWorkerThread* Pop();

  // Removes |worker_thread| from the stack.
  void Remove(const SchedulerWorkerThread* worker_thread);

  // Returns the number of SchedulerWorkerThreads on the stack.
  size_t Size() const { return stack_.size(); }

  // Returns true if the stack is empty.
  bool IsEmpty() const { return stack_.empty(); }

 private:
  std::vector<SchedulerWorkerThread*> stack_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerThreadStack);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_SCHEDULER_WORKER_THREAD_STACK_H_
