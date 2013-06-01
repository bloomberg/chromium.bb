// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/one_shot_event.h"

#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/task_runner.h"

using base::TaskRunner;

namespace extensions {

struct OneShotEvent::TaskInfo {
  TaskInfo() {}
  TaskInfo(const tracked_objects::Location& from_here,
           const scoped_refptr<TaskRunner>& runner,
           const base::Closure& task)
      : from_here(from_here), runner(runner), task(task) {
    CHECK(runner.get());  // Detect mistakes with a decent stack frame.
  }
  tracked_objects::Location from_here;
  scoped_refptr<TaskRunner> runner;
  base::Closure task;
};

OneShotEvent::OneShotEvent() : signaled_(false) {
  // It's acceptable to construct the OneShotEvent on one thread, but
  // immediately move it to another thread.
  thread_checker_.DetachFromThread();
}
OneShotEvent::~OneShotEvent() {}

void OneShotEvent::Post(const tracked_objects::Location& from_here,
                        const base::Closure& task) const {
  Post(from_here, task, base::MessageLoopProxy::current());
}

void OneShotEvent::Post(const tracked_objects::Location& from_here,
                        const base::Closure& task,
                        const scoped_refptr<TaskRunner>& runner) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (is_signaled()) {
    runner->PostTask(from_here, task);
  } else {
    tasks_.push_back(TaskInfo(from_here, runner, task));
  }
}

void OneShotEvent::Signal() {
  DCHECK(thread_checker_.CalledOnValidThread());

  CHECK(!signaled_) << "Only call Signal once.";

  signaled_ = true;
  // After this point, a call to Post() from one of the queued tasks
  // could proceed immediately, but the fact that this object is
  // single-threaded prevents that from being relevant.

  // We could randomize tasks_ in debug mode in order to check that
  // the order doesn't matter...
  for (size_t i = 0; i < tasks_.size(); ++i) {
    tasks_[i].runner->PostTask(tasks_[i].from_here, tasks_[i].task);
  }
}

}  // namespace extensions
