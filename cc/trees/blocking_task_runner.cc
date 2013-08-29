// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/blocking_task_runner.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop_proxy.h"

namespace cc {

typedef std::pair<base::SingleThreadTaskRunner*,
                  scoped_refptr<BlockingTaskRunner> > TaskRunnerPair;

struct TaskRunnerPairs {
  static TaskRunnerPairs* GetInstance() {
    return Singleton<TaskRunnerPairs>::get();
  }

  base::Lock lock;
  std::vector<TaskRunnerPair> pairs;

 private:
  friend struct DefaultSingletonTraits<TaskRunnerPairs>;
};

// static
scoped_refptr<BlockingTaskRunner> BlockingTaskRunner::current() {
  TaskRunnerPairs* task_runners = TaskRunnerPairs::GetInstance();

  base::AutoLock lock(task_runners->lock);

  for (size_t i = 0; i < task_runners->pairs.size(); ++i) {
    if (task_runners->pairs[i].first->HasOneRef()) {
      // The SingleThreadTaskRunner is kept alive by its MessageLoop, and we
      // hold a second reference in the TaskRunnerPairs array. If the
      // SingleThreadTaskRunner has one ref, then it is being held alive only
      // by the BlockingTaskRunner and the MessageLoop is gone, so drop the
      // BlockingTaskRunner from the TaskRunnerPairs array along with the
      // SingleThreadTaskRunner.
      task_runners->pairs.erase(task_runners->pairs.begin() + i);
      --i;
    }
  }

  scoped_refptr<base::SingleThreadTaskRunner> current =
      base::MessageLoopProxy::current();
  for (size_t i = 0; i < task_runners->pairs.size(); ++i) {
    if (task_runners->pairs[i].first == current.get())
      return task_runners->pairs[i].second.get();
  }

  scoped_refptr<BlockingTaskRunner> runner = new BlockingTaskRunner(current);
  task_runners->pairs.push_back(TaskRunnerPair(current, runner));
  return runner;
}

BlockingTaskRunner::BlockingTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner), capture_(0) {}

BlockingTaskRunner::~BlockingTaskRunner() {}

bool BlockingTaskRunner::PostTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task) {
  base::AutoLock lock(lock_);
  if (!capture_)
    return task_runner_->PostTask(from_here, task);
  captured_tasks_.push_back(task);
  return true;
}

void BlockingTaskRunner::SetCapture(bool capture) {
  DCHECK(BelongsToCurrentThread());

  std::vector<base::Closure> tasks;

  {
    base::AutoLock lock(lock_);
    capture_ += capture ? 1 : -1;
    DCHECK_GE(capture_, 0);

    if (capture_)
      return;

    // We're done capturing, so grab all the captured tasks and run them.
    tasks.swap(captured_tasks_);
  }
  for (size_t i = 0; i < tasks.size(); ++i)
    tasks[i].Run();
}

BlockingTaskRunner::CapturePostTasks::CapturePostTasks()
    : blocking_runner_(BlockingTaskRunner::current()) {
  blocking_runner_->SetCapture(true);
}

BlockingTaskRunner::CapturePostTasks::~CapturePostTasks() {
  blocking_runner_->SetCapture(false);
}

}  // namespace cc
