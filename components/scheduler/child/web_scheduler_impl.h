// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SCHEDULER_BASE_WEB_SCHEDULER_IMPL_H_
#define CONTENT_CHILD_SCHEDULER_BASE_WEB_SCHEDULER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "components/scheduler/scheduler_export.h"
#include "third_party/WebKit/public/platform/WebScheduler.h"
#include "third_party/WebKit/public/platform/WebThread.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace scheduler {

class ChildScheduler;
class SingleThreadIdleTaskRunner;

class SCHEDULER_EXPORT WebSchedulerImpl : public blink::WebScheduler {
 public:
  WebSchedulerImpl(
      ChildScheduler* child_scheduler,
      scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner);
  ~WebSchedulerImpl() override;

  // blink::WebScheduler implementation:
  virtual void shutdown();
  virtual bool shouldYieldForHighPriorityWork();
  virtual bool canExceedIdleDeadlineIfRequired();
  virtual void postIdleTask(const blink::WebTraceLocation& location,
                            blink::WebThread::IdleTask* task);
  virtual void postNonNestableIdleTask(const blink::WebTraceLocation& location,
                                       blink::WebThread::IdleTask* task);
  virtual void postIdleTaskAfterWakeup(const blink::WebTraceLocation& location,
                                       blink::WebThread::IdleTask* task);
  virtual void postLoadingTask(const blink::WebTraceLocation& location,
                               blink::WebThread::Task* task);
  virtual void postTimerTask(const blink::WebTraceLocation& location,
                             blink::WebThread::Task* task,
                             long long delayMs);

 private:
  static void runIdleTask(scoped_ptr<blink::WebThread::IdleTask> task,
                          base::TimeTicks deadline);
  static void runTask(scoped_ptr<blink::WebThread::Task> task);

  ChildScheduler* child_scheduler_;  // NOT OWNED
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner_;
};

}  // namespace scheduler

#endif  // CONTENT_CHILD_SCHEDULER_BASE_WEB_SCHEDULER_IMPL_H_
