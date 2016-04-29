// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_SCHEDULER_THREAD_POOL_H_
#define BASE_TASK_SCHEDULER_SCHEDULER_THREAD_POOL_H_

#include <memory>

#include "base/base_export.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "base/task_scheduler/sequence.h"
#include "base/task_scheduler/task.h"
#include "base/task_scheduler/task_traits.h"

namespace base {
namespace internal {

class SchedulerWorkerThread;
class SequenceSortKey;

// Interface for a thread pool.
class BASE_EXPORT SchedulerThreadPool {
 public:
  virtual ~SchedulerThreadPool() = default;

  // Returns a TaskRunner whose PostTask invocations will result in scheduling
  // Tasks with |traits| and |execution_mode| in this SchedulerThreadPool.
  virtual scoped_refptr<TaskRunner> CreateTaskRunnerWithTraits(
      const TaskTraits& traits,
      ExecutionMode execution_mode) = 0;

  // Inserts |sequence| with |sequence_sort_key| into a queue of Sequences that
  // can be processed by any worker thread owned by this SchedulerThreadPool.
  // Must only be used to put |sequence| back into a queue after running a Task
  // from it. The thread that calls this doesn't have to belong to this
  // SchedulerThreadPool.
  virtual void ReEnqueueSequence(scoped_refptr<Sequence> sequence,
                                 const SequenceSortKey& sequence_sort_key) = 0;

  // Posts |task| to be executed by this SchedulerThreadPool as part of
  // |sequence|. If |worker_thread| is non-null, |task| will be scheduled to run
  // on it specifically (note: |worker_thread| must be owned by this
  // SchedulerThreadPool); otherwise, |task| will be added to the pending shared
  // work. |task| won't be executed before its delayed run time, if any. Returns
  // true if |task| is posted.
  virtual bool PostTaskWithSequence(std::unique_ptr<Task> task,
                                    scoped_refptr<Sequence> sequence,
                                    SchedulerWorkerThread* worker_thread) = 0;

  // Posts |task| to be executed by this SchedulerThreadPool as part of
  // |sequence|. If |worker_thread| is non-null, |task| will be scheduled to run
  // on it specifically (note: |worker_thread| must be owned by this
  // SchedulerThreadPool); otherwise, |task| will be added to the pending shared
  // work. This must only be called after |task| has gone through
  // PostTaskWithSequence() and after |task|'s delayed run time.
  virtual void PostTaskWithSequenceNow(
      std::unique_ptr<Task> task,
      scoped_refptr<Sequence> sequence,
      SchedulerWorkerThread* worker_thread) = 0;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_SCHEDULER_THREAD_POOL_H_
