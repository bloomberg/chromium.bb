// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_PPAPI_WEBTHREAD_IMPL_FOR_PPAPI_H_
#define COMPONENTS_SCHEDULER_PPAPI_WEBTHREAD_IMPL_FOR_PPAPI_H_

#include "components/scheduler/child/webthread_base.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebScheduler;
};

namespace scheduler {
class SchedulerTaskRunnerDelegate;
class SingleThreadIdleTaskRunner;
class WebSchedulerImpl;
class WebTaskRunnerImpl;
class WorkerScheduler;

class SCHEDULER_EXPORT WebThreadImplForPPAPI : public WebThreadBase {
 public:
  explicit WebThreadImplForPPAPI();
  virtual ~WebThreadImplForPPAPI();

  // blink::WebThread implementation.
  virtual blink::WebScheduler* scheduler() const;
  blink::PlatformThreadId threadId() const override;
  virtual blink::WebTaskRunner* taskRunner();

  // WebThreadBase implementation.
  base::SingleThreadTaskRunner* TaskRunner() const override;
  scheduler::SingleThreadIdleTaskRunner* IdleTaskRunner() const override;

 private:
  void AddTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;
  void RemoveTaskObserverInternal(
      base::MessageLoop::TaskObserver* observer) override;

  blink::PlatformThreadId thread_id_;
  scoped_refptr<SchedulerTaskRunnerDelegate> task_runner_delegate_;
  scoped_ptr<scheduler::WorkerScheduler> worker_scheduler_;
  scoped_ptr<scheduler::WebSchedulerImpl> web_scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<scheduler::SingleThreadIdleTaskRunner> idle_task_runner_;
  scoped_ptr<WebTaskRunnerImpl> web_task_runner_;
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_PPAPI_WEBTHREAD_IMPL_FOR_PPAPI_H_
