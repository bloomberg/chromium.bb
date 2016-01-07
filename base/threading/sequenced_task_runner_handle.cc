// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/sequenced_task_runner_handle.h"

#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"

namespace base {

// static
scoped_refptr<SequencedTaskRunner> SequencedTaskRunnerHandle::Get() {
  // Return the SequencedTaskRunner if found or the SingleThreadedTaskRunner for
  // the current thread otherwise.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      SequencedWorkerPool::GetSequencedTaskRunnerForCurrentThread();
  if (task_runner)
    return task_runner;

  return base::ThreadTaskRunnerHandle::Get();
}

// static
bool SequencedTaskRunnerHandle::IsSet() {
  return SequencedWorkerPool::GetWorkerPoolForCurrentThread() ||
         base::ThreadTaskRunnerHandle::IsSet();
}

}  // namespace base
