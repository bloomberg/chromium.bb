// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_RESIZE_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_RESIZE_HELPER_H_

#include "base/lazy_instance.h"
#include "base/single_thread_task_runner.h"
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
// WrappedTask) to make sure that the messages are only executed once.
//
// This is further complicated because, in order for a frame to appear, it is
// necessary to run tasks posted by the ui::Compositor. To accomplish this, the
// RenderWidgetResizeHelper provides a base::SingleThreadTaskRunner which,
// when a task is posted to it, enqueues the task in the aforementioned queue,
// which may be pumped by RenderWidgetResizeHelper::WaitForSingleTaskToRun.
//
class RenderWidgetResizeHelper {
 public:
  static RenderWidgetResizeHelper* Get();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const;

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

  // This helper is needed to create a ScopedAllowWait inside the scope of a
  // class where it is allowed.
  static void EventTimedWait(
      base::WaitableEvent* event,
      base::TimeDelta delay);

  // The task runner to which the helper will post tasks. This also maintains
  // the task queue and does the actual work for WaitForSingleTaskToRun.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetResizeHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_RESIZE_HELPER_H_
