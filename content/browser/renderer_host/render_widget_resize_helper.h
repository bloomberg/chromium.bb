// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_RESIZE_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_RESIZE_HELPER_H_

#include <deque>

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "ipc/ipc_message.h"

namespace content {

// RenderWidgetResizeHelper is used to make resize appear smooth. That is to
// say, make sure that the window size and the size of the content being drawn
// in that window are resized in lock-step. This is accomplished by waiting
// inside -[RenderWidgetHostViewCocoa setFrameSize:] for the renderer (and
// potentially browser compositor as well) to produce a frame of same size
// as the RenderWidgetHostViewCocoa.
//
// The function of waiting for a frame of the correct size is done inside
// RenderWidgetHostImpl::WaitForSurface. That function will call
// RenderWidgetResizeHelper::WaitForSingleTaskToRun until a timeout occurs,
// or the corresponding RenderWidgetHostViewCocoa has a renderer frame of the
// same size as its NSView.
//
// This is somewhat complicated because waiting for frames requires that
// that the browser handle the IPCs (from the renderer and the GPU processes)
// that are required to pick up a new frame. In the ordinary run of things
// (ignoring RenderWidgetResizeHelper), those IPCs arrive on the IO thread
// and are posted as tasks to the UI thread either by the RenderMessageFilter
// (for renderer processes) or the GpuProcessHostUIShim (for the GPU process).
// The IPCs that are required to create new frames for smooth resize are sent
// to the RenderWidgetResizeHelper using the PostRendererProcessMsg and
// PostGpuProcessMsg methods. These functions will post them as tasks to the UI
// thread (as usual), and will also enqueue them into a queue which will be
// read and run in RenderWidgetResizeHelper::WaitForSingleTaskToRun, potentially
// before the task posted to the UI thread is run. Some care is taken (see
// EnqueuedTask) to make sure that the messages are only executed once.
//
// TODO(ccameron): This does not support smooth resize when using the
// ui::Compositor yet. To support this, it will be necessary that the
// RenderWidgetResizeHelper have a base::TaskRunner to send to the
// cc::ThreadProxy. The tasks that cc then posts can be pumped in
// WaitForSingleTaskToRun in a way similar to the one in which IPCs are handled.
//
class RenderWidgetResizeHelper {
 public:
  static RenderWidgetResizeHelper* Get();

  // UI THREAD ONLY -----------------------------------------------------------

  // Waits at most |max_delay| for a task to run. Returns true if a task ran,
  // false if no task ran.
  bool WaitForSingleTaskToRun(const base::TimeDelta& max_delay);

  // IO THREAD ONLY -----------------------------------------------------------

  // This will cause |msg| to be handled by the RenderProcessHost corresponding
  // to |render_process_id|, on the UI thread. This will either happen when the
  // ordinary message loop would run it, or potentially earlier in a call to
  // WaitForSingleTaskToRun .
  void PostRendererProcessMsg(int render_process_id, const IPC::Message& msg);

  // This is similar to PostRendererProcessMsg, but will handle the message in
  // the GpuProcessHostUIShim corresponding to |gpu_host_id|.
  void PostGpuProcessMsg(int gpu_host_id, const IPC::Message& msg);

 private:
  friend struct base::DefaultLazyInstanceTraits<RenderWidgetResizeHelper>;
  RenderWidgetResizeHelper();
  ~RenderWidgetResizeHelper();

  // A classed used to wrap an IPC or a task.
  class EnqueuedTask;
  friend class EnqueuedTask;

  // Called on the IO thread to add a task to the queue.
  void PostEnqueuedTask(EnqueuedTask* proxy);

  // Called on the UI to remove the task from the queue when it is run.
  void RemoveEnqueuedTaskFromQueue(EnqueuedTask* proxy);

  // A queue of live messages.  Must hold |task_queue_lock_| to access. Tasks
  // are added only on the IO thread and removed only on the UI thread.  The
  // EnqueuedTask objects are removed from the front of the queue when they are
  // run (by TaskRunner they were posted to, by a call to WaitForSingleTaskToRun
  // pulling them off of the queue, or by TaskRunner when it is destroyed).
  typedef std::deque<EnqueuedTask*> EnqueuedTaskQueue;
  EnqueuedTaskQueue task_queue_;
  base::Lock task_queue_lock_;

  // Event used to wake up the UI thread if it is sleeping in
  // WaitForSingleTaskToRun.
  base::WaitableEvent event_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_RESIZE_HELPER_H_
