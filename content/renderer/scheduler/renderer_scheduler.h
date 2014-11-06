// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_RENDERER_SCHEDULER_H_
#define CONTENT_RENDERER_SCHEDULER_RENDERER_SCHEDULER_H_

#include "content/renderer/scheduler/single_thread_idle_task_runner.h"
#include "content/renderer/scheduler/task_queue_manager.h"

namespace cc {
struct BeginFrameArgs;
}

namespace content {

class CONTENT_EXPORT RendererScheduler {
 public:
  virtual ~RendererScheduler();
  static scoped_ptr<RendererScheduler> Create();

  // Returns the default task runner.
  virtual scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() = 0;

  // Returns the compositor task runner.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  CompositorTaskRunner() = 0;

  // Returns the idle task runner. Tasks posted to this runner may be reordered
  // relative to other task types and may be starved for an arbitrarily long
  // time if no idle time is available.
  virtual scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() = 0;

  // Called to notify about the start of a new frame.  Must be called from the
  // main thread.
  virtual void WillBeginFrame(const cc::BeginFrameArgs& args) = 0;

  // Called to notify that a previously begun frame was committed. Must be
  // called from the main thread.
  virtual void DidCommitFrameToCompositor() = 0;

  // Tells the scheduler that the system received an input event. Called by the
  // compositor (impl) thread.
  virtual void DidReceiveInputEventOnCompositorThread() = 0;

  // Returns true if there is high priority work pending on the main thread
  // and the caller should yield to let the scheduler service that work.
  // Must be called from the main thread.
  virtual bool ShouldYieldForHighPriorityWork() = 0;

  // Shuts down the scheduler by dropping any remaining pending work in the work
  // queues. After this call any work posted to the task runners will be
  // silently dropped.
  virtual void Shutdown() = 0;

 protected:
  RendererScheduler();
  DISALLOW_COPY_AND_ASSIGN(RendererScheduler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCHEDULER_RENDERER_SCHEDULER_H_
