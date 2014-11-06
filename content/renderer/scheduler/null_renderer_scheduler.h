// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCHEDULER_NULL_RENDERER_SCHEDULER_H_
#define CONTENT_RENDERER_SCHEDULER_NULL_RENDERER_SCHEDULER_H_

#include "content/renderer/scheduler/renderer_scheduler.h"

namespace content {

class NullRendererScheduler : public RendererScheduler {
 public:
  NullRendererScheduler();
  ~NullRendererScheduler() override;

  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;

  void WillBeginFrame(const cc::BeginFrameArgs& args) override;
  void DidCommitFrameToCompositor() override;
  void DidReceiveInputEventOnCompositorThread() override;
  bool ShouldYieldForHighPriorityWork() override;
  void Shutdown() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(NullRendererScheduler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCHEDULER_NULL_RENDERER_SCHEDULER_H_
