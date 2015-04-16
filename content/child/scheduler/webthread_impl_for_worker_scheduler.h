// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SCHEDULER_WEBTHREAD_IMPL_FOR_WORKER_SCHEDULER_H_
#define CONTENT_CHILD_SCHEDULER_WEBTHREAD_IMPL_FOR_WORKER_SCHEDULER_H_

#include "content/child/scheduler/task_queue_manager.h"
#include "content/child/webthread_base.h"

namespace base {
class WaitableEvent;
};

namespace blink {
class WebScheduler;
};

namespace content {

class SingleThreadIdleTaskRunner;
class WebSchedulerImpl;
class WorkerScheduler;

class CONTENT_EXPORT WebThreadImplForWorkerScheduler : public WebThreadBase {
 public:
  explicit WebThreadImplForWorkerScheduler(const char* name);
  virtual ~WebThreadImplForWorkerScheduler();

  // blink::WebThread implementation.
  virtual blink::WebScheduler* scheduler() const;
  blink::PlatformThreadId threadId() const override;

  // WebThreadBase implementation.
  base::SingleThreadTaskRunner* TaskRunner() const override;
  SingleThreadIdleTaskRunner* IdleTaskRunner() const override;

 private:
  base::MessageLoop* MessageLoop() const override;
  void AddTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;
  void RemoveTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;

  void InitOnThread(base::WaitableEvent* completion);
  void ShutDownOnThread(base::WaitableEvent* completion);

  scoped_ptr<base::Thread> thread_;
  scoped_ptr<WorkerScheduler> worker_scheduler_;
  scoped_ptr<WebSchedulerImpl> web_scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
};

}  // namespace content

#endif  // CONTENT_CHILD_SCHEDULER_WEBTHREAD_IMPL_FOR_WORKER_SCHEDULER_H_
