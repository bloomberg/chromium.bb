// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WORKER_TASK_RUNNER_H_
#define CONTENT_CHILD_WORKER_TASK_RUNNER_H_

#include <map>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local.h"
#include "content/common/content_export.h"

namespace base {
class TaskRunner;
}

namespace content {

class CONTENT_EXPORT WorkerTaskRunner {
 public:
  WorkerTaskRunner();

  bool PostTask(int id, const base::Closure& task);
  int PostTaskToAllThreads(const base::Closure& task);
  int CurrentWorkerId();
  static WorkerTaskRunner* Instance();

  class CONTENT_EXPORT Observer {
   public:
    virtual ~Observer() {}
    virtual void OnWorkerRunLoopStopped() = 0;
  };
  // Add/Remove an observer that will get notified when the current worker run
  // loop is stopping. This observer will not get notified when other threads
  // are stopping.  It's only valid to call these on a worker thread.
  void AddStopObserver(Observer* observer);
  void RemoveStopObserver(Observer* observer);

  void OnWorkerRunLoopStarted();
  void OnWorkerRunLoopStopped();

  base::TaskRunner* GetTaskRunnerFor(int worker_id);

 private:
  friend class WorkerTaskRunnerTest;

  using IDToTaskRunnerMap = std::map<base::PlatformThreadId, base::TaskRunner*>;

  ~WorkerTaskRunner();

  // It is possible for an IPC message to arrive for a worker thread that has
  // already gone away. In such cases, it is still necessary to provide a
  // task-runner for that particular thread, because otherwise the message will
  // end up being handled as per usual in the main-thread, causing incorrect
  // results. |task_runner_for_dead_worker_| is used to handle such messages,
  // which silently discards all the tasks it receives.
  scoped_refptr<base::TaskRunner> task_runner_for_dead_worker_;

  struct ThreadLocalState;
  base::ThreadLocalPointer<ThreadLocalState> current_tls_;

  IDToTaskRunnerMap task_runner_map_;
  base::Lock task_runner_map_lock_;
};

}  // namespace content

#endif  // CONTENT_CHILD_WORKER_TASK_RUNNER_H_
