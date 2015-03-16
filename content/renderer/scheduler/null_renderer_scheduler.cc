// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scheduler/null_renderer_scheduler.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"

namespace content {

namespace {

class NullIdleTaskRunner : public SingleThreadIdleTaskRunner {
 public:
  NullIdleTaskRunner();
  void PostIdleTask(const tracked_objects::Location& from_here,
                    const IdleTask& idle_task) override;

 protected:
  ~NullIdleTaskRunner() override;
};

}  // namespace

NullIdleTaskRunner::NullIdleTaskRunner()
    : SingleThreadIdleTaskRunner(nullptr,
                                 nullptr,
                                 base::Callback<void(base::TimeTicks*)>()) {
}

NullIdleTaskRunner::~NullIdleTaskRunner() {
}

void NullIdleTaskRunner::PostIdleTask(
    const tracked_objects::Location& from_here,
    const IdleTask& idle_task) {
}

NullRendererScheduler::NullRendererScheduler()
    : task_runner_(base::MessageLoopProxy::current()),
      idle_task_runner_(new NullIdleTaskRunner()) {
}

NullRendererScheduler::~NullRendererScheduler() {
}

scoped_refptr<base::SingleThreadTaskRunner>
NullRendererScheduler::DefaultTaskRunner() {
  return task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
NullRendererScheduler::CompositorTaskRunner() {
  return task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
NullRendererScheduler::LoadingTaskRunner() {
  return task_runner_;
}

scoped_refptr<SingleThreadIdleTaskRunner>
NullRendererScheduler::IdleTaskRunner() {
  return idle_task_runner_;
}

void NullRendererScheduler::WillBeginFrame(const cc::BeginFrameArgs& args) {
}

void NullRendererScheduler::BeginFrameNotExpectedSoon() {
}

void NullRendererScheduler::DidCommitFrameToCompositor() {
}

void NullRendererScheduler::DidReceiveInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event) {
}

void NullRendererScheduler::DidAnimateForInputOnCompositorThread() {
}

bool NullRendererScheduler::IsHighPriorityWorkAnticipated() {
  return false;
}

bool NullRendererScheduler::ShouldYieldForHighPriorityWork() {
  return false;
}

bool NullRendererScheduler::CanExceedIdleDeadlineIfRequired() const {
  return false;
}

void NullRendererScheduler::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  base::MessageLoop::current()->AddTaskObserver(task_observer);
}

void NullRendererScheduler::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  base::MessageLoop::current()->RemoveTaskObserver(task_observer);
}

void NullRendererScheduler::Shutdown() {
}

}  // namespace content
