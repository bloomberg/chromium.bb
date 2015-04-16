// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SCHEDULER_WORKER_SCHEDULER_H_
#define CONTENT_CHILD_SCHEDULER_WORKER_SCHEDULER_H_

#include "base/message_loop/message_loop.h"
#include "content/child/scheduler/child_scheduler.h"
#include "content/child/scheduler/single_thread_idle_task_runner.h"
#include "content/common/content_export.h"

namespace base {
class MessageLoop;
}

namespace content {

class CONTENT_EXPORT WorkerScheduler : public ChildScheduler {
 public:
  ~WorkerScheduler() override;
  static scoped_ptr<WorkerScheduler> Create(base::MessageLoop* message_loop);

  // Must be called before the scheduler can be used. Does any post construction
  // initialization needed such as initializing idle period detection.
  virtual void Init() = 0;

 protected:
  WorkerScheduler();
  DISALLOW_COPY_AND_ASSIGN(WorkerScheduler);
};

}  // namespace content

#endif  // CONTENT_CHILD_SCHEDULER_WORKER_SCHEDULER_H_
