// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_RENDERER_SCHEDULER_H_
#define CONTENT_TEST_FAKE_RENDERER_SCHEDULER_H_

#include "base/macros.h"
#include "components/scheduler/renderer/renderer_scheduler.h"

namespace content {

class FakeRendererScheduler : public scheduler::RendererScheduler {
 public:
  FakeRendererScheduler();
  ~FakeRendererScheduler() override;

  // RendererScheduler implementation.
  scoped_ptr<blink::WebThread> CreateMainThread() override;
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> LoadingTaskRunner() override;
  scoped_refptr<scheduler::SingleThreadIdleTaskRunner> IdleTaskRunner()
      override;
  scoped_refptr<scheduler::TaskQueue> TimerTaskRunner() override;
  scoped_refptr<scheduler::TaskQueue> NewLoadingTaskRunner(
      const char* name) override;
  scoped_refptr<scheduler::TaskQueue> NewTimerTaskRunner(
      const char* name) override;
  scoped_ptr<scheduler::RenderWidgetSchedulingState>
  NewRenderWidgetSchedulingState() override;
  void WillBeginFrame(const cc::BeginFrameArgs& args) override;
  void BeginFrameNotExpectedSoon() override;
  void DidCommitFrameToCompositor() override;
  void DidHandleInputEventOnCompositorThread(
      const blink::WebInputEvent& web_input_event,
      InputEventState event_state) override;
  void DidHandleInputEventOnMainThread(
      const blink::WebInputEvent& web_input_event) override;
  void DidAnimateForInputOnCompositorThread() override;
  void OnRendererBackgrounded() override;
  void OnRendererForegrounded() override;
  void AddPendingNavigation() override;
  void RemovePendingNavigation() override;
  void OnNavigationStarted() override;
  bool IsHighPriorityWorkAnticipated() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  bool ShouldYieldForHighPriorityWork() override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Shutdown() override;
  void SuspendTimerQueue() override;
  void ResumeTimerQueue() override;
  void SetTimerQueueSuspensionWhenBackgroundedEnabled(bool enabled) override;
  double VirtualTimeSeconds() const override;
  double MonotonicallyIncreasingVirtualTimeSeconds() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeRendererScheduler);
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_RENDERER_SCHEDULER_H_
