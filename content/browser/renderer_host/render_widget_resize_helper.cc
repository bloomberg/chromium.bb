// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_resize_helper.h"

#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace {
base::LazyInstance<RenderWidgetResizeHelper> g_render_widget_task_runner =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

// A wrapper for IPCs and tasks that we may potentially execute in
// WaitForSingleTaskToRun. Because these tasks are sent to two places to run,
// we to wrap them in this structure and track whether or not they have run
// yet, to avoid running them twice.
class RenderWidgetResizeHelper::EnqueuedTask {
 public:
  enum Type {
    RENDERER_IPC,
    GPU_IPC,
  };
  EnqueuedTask(Type type, int process_id, const IPC::Message& m);
  ~EnqueuedTask();
  void Run();
  void InvalidateHelper();

 private:
  Type type_;
  int process_id_;
  IPC::Message message_;
  bool has_run_;

  // Back-pointer to the ResizeHelper which has this task in its queue. Set
  // to NULL when this task is removed from the queue.
  RenderWidgetResizeHelper* helper_;

  DISALLOW_COPY_AND_ASSIGN(EnqueuedTask);
};

RenderWidgetResizeHelper::EnqueuedTask::EnqueuedTask(
    Type type,
    int process_id,
    const IPC::Message& m)
    : type_(type),
      process_id_(process_id),
      message_(m),
      has_run_(false),
      helper_(RenderWidgetResizeHelper::Get()) {
}

RenderWidgetResizeHelper::EnqueuedTask::~EnqueuedTask() {
  // Note that if the MessageLoop into which this task was posted is destroyed
  // before the RenderWidgetResizeHelper, then the helper's list of tasks will
  // point to freed data. Avoid this by removing tasks when they are freed, if
  // they weren't already removed when they were run.
  if (helper_)
    helper_->RemoveEnqueuedTaskFromQueue(this);
}

void RenderWidgetResizeHelper::EnqueuedTask::Run() {
  if (has_run_)
    return;

  if (helper_)
    helper_->RemoveEnqueuedTaskFromQueue(this);
  has_run_ = true;

  switch (type_) {
    case RENDERER_IPC: {
      RenderProcessHost* host = RenderProcessHost::FromID(process_id_);
      if (host)
        host->OnMessageReceived(message_);
      break;
    }
    case GPU_IPC: {
      GpuProcessHostUIShim* host = GpuProcessHostUIShim::FromID(process_id_);
      if (host)
        host->OnMessageReceived(message_);
      break;
    }
  }
}

void RenderWidgetResizeHelper::EnqueuedTask::InvalidateHelper() {
  helper_ = NULL;
}

// static
RenderWidgetResizeHelper* RenderWidgetResizeHelper::Get() {
  return g_render_widget_task_runner.Pointer();
}

bool RenderWidgetResizeHelper::WaitForSingleTaskToRun(
    const base::TimeDelta& max_delay) {
  base::TimeTicks time_start = base::TimeTicks::Now();

  for (;;) {
    // Peek at the message from the front of the queue. Running it will remove
    // it from the queue.
    EnqueuedTask* task = NULL;
    {
      base::AutoLock lock(task_queue_lock_);
      if (!task_queue_.empty())
        task = task_queue_.front();
    }

    if (task) {
      task->Run();
      return true;
    }

    // Calculate the maximum amount of time that we are willing to sleep.
    base::TimeDelta max_sleep_time =
        max_delay - (base::TimeTicks::Now() - time_start);
    if (max_sleep_time <= base::TimeDelta::FromMilliseconds(0))
      break;

    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    event_.TimedWait(max_sleep_time);
  }

  return false;
}

void RenderWidgetResizeHelper::PostEnqueuedTask(EnqueuedTask* task) {
  {
    base::AutoLock lock(task_queue_lock_);
    task_queue_.push_back(task);
  }

  // Notify anyone waiting on the UI thread that there is a new entry in the
  // task map.  If they don't find the entry they are looking for, then they
  // will just continue waiting.
  event_.Signal();

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&EnqueuedTask::Run, base::Owned(task)));
}

void RenderWidgetResizeHelper::RemoveEnqueuedTaskFromQueue(EnqueuedTask* task) {
  base::AutoLock lock(task_queue_lock_);
  DCHECK(task_queue_.front() == task);
  task_queue_.pop_front();
  task->InvalidateHelper();
}

void RenderWidgetResizeHelper::PostRendererProcessMsg(
    int render_process_id, const IPC::Message& msg) {
  PostEnqueuedTask(new EnqueuedTask(
      EnqueuedTask::RENDERER_IPC, render_process_id, msg));
}

void RenderWidgetResizeHelper::PostGpuProcessMsg(
    int gpu_host_id, const IPC::Message& msg) {
  PostEnqueuedTask(new EnqueuedTask(EnqueuedTask::GPU_IPC, gpu_host_id, msg));
}

RenderWidgetResizeHelper::RenderWidgetResizeHelper()
    : event_(false /* auto-reset */, false /* initially signalled */) {}

RenderWidgetResizeHelper::~RenderWidgetResizeHelper() {
  // Ensure that any tasks that outlive this do not reach back into it.
  for (EnqueuedTaskQueue::iterator it = task_queue_.begin();
       it != task_queue_.end(); ++it) {
    EnqueuedTask* task = *it;
    task->InvalidateHelper();
  }
}

}  // namespace content

