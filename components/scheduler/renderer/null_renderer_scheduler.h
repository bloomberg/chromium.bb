// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_NULL_RENDERER_SCHEDULER_H_
#define COMPONENTS_SCHEDULER_RENDERER_NULL_RENDERER_SCHEDULER_H_

#include "components/scheduler/renderer/renderer_scheduler.h"

namespace scheduler {

class NullRendererScheduler : public RendererScheduler {
 public:
  NullRendererScheduler();
  ~NullRendererScheduler() override;

  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> LoadingTaskRunner() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> TimerTaskRunner() override;

  void WillBeginFrame(const cc::BeginFrameArgs& args) override;
  void BeginFrameNotExpectedSoon() override;
  void DidCommitFrameToCompositor() override;
  void DidHandleInputEventOnCompositorThread(
      const blink::WebInputEvent& web_input_event,
      InputEventState event_state) override;
  void DidHandleInputEventOnMainThread(
      const blink::WebInputEvent& web_input_event) override;
  void OnRendererHidden() override;
  void OnRendererVisible() override;
  void OnPageLoadStarted() override;
  void DidAnimateForInputOnCompositorThread() override;
  bool IsHighPriorityWorkAnticipated() override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Shutdown() override;
  void SuspendTimerQueue() override;
  void ResumeTimerQueue() override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(NullRendererScheduler);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_NULL_RENDERER_SCHEDULER_H_
