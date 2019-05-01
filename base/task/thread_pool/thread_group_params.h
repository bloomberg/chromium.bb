// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_THREAD_GROUP_PARAMS_H_
#define BASE_TASK_THREAD_POOL_THREAD_GROUP_PARAMS_H_

#include "base/task/thread_pool/worker_thread_params.h"
#include "base/time/time.h"

namespace base {

class BASE_EXPORT ThreadGroupParams final {
 public:
  // Constructs a set of params used to initialize a pool. The pool will run
  // concurrently at most |max_tasks| that aren't blocked (ScopedBlockingCall).
  // |suggested_reclaim_time| sets a suggestion on when to reclaim idle threads.
  // The pool is free to ignore this value for performance or correctness
  // reasons. |backward_compatibility| indicates whether backward compatibility
  // is enabled.
  ThreadGroupParams(int max_tasks,
                    TimeDelta suggested_reclaim_time,
                    WorkerThreadBackwardCompatibility backward_compatibility =
                        WorkerThreadBackwardCompatibility::DISABLED);

  ThreadGroupParams(const ThreadGroupParams& other);
  ThreadGroupParams& operator=(const ThreadGroupParams& other);

  int max_tasks() const { return max_tasks_; }
  TimeDelta suggested_reclaim_time() const { return suggested_reclaim_time_; }
  WorkerThreadBackwardCompatibility backward_compatibility() const {
    return backward_compatibility_;
  }

 private:
  int max_tasks_;
  TimeDelta suggested_reclaim_time_;
  WorkerThreadBackwardCompatibility backward_compatibility_;
};

}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_THREAD_GROUP_PARAMS_H_
