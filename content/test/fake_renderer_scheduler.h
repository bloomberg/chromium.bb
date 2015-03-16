// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_RENDERER_SCHEDULER_H_
#define CONTENT_TEST_FAKE_RENDERER_SCHEDULER_H_

#include "content/renderer/scheduler/renderer_scheduler.h"

namespace content {

class FakeRendererScheduler : public RendererScheduler {
 public:
  FakeRendererScheduler();
  ~FakeRendererScheduler() override;

  // RendererScheduler implementation.
  scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> LoadingTaskRunner() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;
  void WillBeginFrame(const cc::BeginFrameArgs& args) override;
  void BeginFrameNotExpectedSoon() override;
  void DidCommitFrameToCompositor() override;
  void DidReceiveInputEventOnCompositorThread(
      const blink::WebInputEvent& web_input_event) override;
  void DidAnimateForInputOnCompositorThread() override;
  bool IsHighPriorityWorkAnticipated() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  bool ShouldYieldForHighPriorityWork() override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Shutdown() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeRendererScheduler);
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_RENDERER_SCHEDULER_H_
