// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_PROXY_TASK_RUNNER_H_
#define CONTENT_RENDERER_SCHEDULER_PROXY_TASK_RUNNER_H_

#include "base/debug/task_annotator.h"
#include "base/pending_task.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebSchedulerProxy.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace content {

// Helper for forwarding posted tasks into different WebSchedulerProxy queues.
template <void (blink::WebSchedulerProxy::*ProxyFunction)(
    const blink::WebTraceLocation& location,
    blink::WebThread::Task* task)>
class SchedulerProxyTaskRunner : public base::SingleThreadTaskRunner {
 public:
  SchedulerProxyTaskRunner()
      : main_thread_id_(base::PlatformThread::CurrentId()),
        scheduler_proxy_(blink::WebSchedulerProxy::create()),
        next_sequence_num_(0) {}

  // base::SingleThreadTaskRunner implementation:
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE {
    return base::PlatformThread::CurrentId() == main_thread_id_;
  }

  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE {
    DCHECK(delay == base::TimeDelta());
    base::PendingTask pending_task(from_here, task);
    pending_task.sequence_num = ++next_sequence_num_;
    task_annotator_.DidQueueTask("SchedulerProxyTaskRunner::PostDelayedTask",
                                 pending_task);
    blink::WebTraceLocation location(from_here.function_name(),
                                     from_here.file_name());
    TaskAdapter* task_adapter = new TaskAdapter(&task_annotator_, pending_task);
    (scheduler_proxy_.*ProxyFunction)(location, task_adapter);
    return true;
  }

  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE {
    NOTREACHED();
    return false;
  }

 protected:
  virtual ~SchedulerProxyTaskRunner() {}

 private:
  class TaskAdapter : public blink::WebThread::Task {
   public:
    explicit TaskAdapter(base::debug::TaskAnnotator* task_annotator,
                         const base::PendingTask& pending_task)
        : task_annotator_(task_annotator), pending_task_(pending_task) {}
    virtual ~TaskAdapter() {}

    virtual void run() {
      task_annotator_->RunTask("SchedulerProxyTaskRunner::PostDelayedTask",
                               "SchedulerProxyTaskRunner::RunTask",
                               pending_task_);
    }

   private:
    base::debug::TaskAnnotator* task_annotator_;
    base::PendingTask pending_task_;
  };

  const base::PlatformThreadId main_thread_id_;
  blink::WebSchedulerProxy scheduler_proxy_;
  base::debug::TaskAnnotator task_annotator_;
  int next_sequence_num_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerProxyTaskRunner);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCHEDULER_PROXY_TASK_RUNNER_H_
