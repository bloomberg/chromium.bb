// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_SEQUENCED_WORKER_POOL_TASK_RUNNER_H_
#define BASE_THREADING_SEQUENCED_WORKER_POOL_TASK_RUNNER_H_
#pragma once

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace base {

// A SequencedTaskRunner which posts tasks to a SequencedWorkerPool with a
// fixed sequence token.
//
// Note that this class is RefCountedThreadSafe (inherited from TaskRunner).
class BASE_EXPORT SequencedWorkerPoolTaskRunner : public SequencedTaskRunner {
 public:
  SequencedWorkerPoolTaskRunner(const scoped_refptr<SequencedWorkerPool>& pool,
                                SequencedWorkerPool::SequenceToken token);

  // TaskRunner implementation
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const Closure& task,
                               TimeDelta delay) OVERRIDE;
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE;

  // SequencedTaskRunner implementation
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const Closure& task,
      TimeDelta delay) OVERRIDE;

 private:
  virtual ~SequencedWorkerPoolTaskRunner();

  // Helper function for posting a delayed task. Asserts that the delay is
  // zero because non-zero delays are not yet supported.
  bool PostDelayedTaskAssertZeroDelay(
      const tracked_objects::Location& from_here,
      const Closure& task,
      TimeDelta delay);

  const scoped_refptr<SequencedWorkerPool> pool_;

  const SequencedWorkerPool::SequenceToken token_;

  DISALLOW_COPY_AND_ASSIGN(SequencedWorkerPoolTaskRunner);
};

}  // namespace base

#endif  // BASE_THREADING_SEQUENCED_WORKER_POOL_TASK_RUNNER_H_
