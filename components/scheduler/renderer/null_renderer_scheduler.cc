// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/null_renderer_scheduler.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "components/scheduler/child/null_idle_task_runner.h"

namespace scheduler {

NullRendererScheduler::NullRendererScheduler()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
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

scoped_refptr<base::SingleThreadTaskRunner>
NullRendererScheduler::TimerTaskRunner() {
  return task_runner_;
}

void NullRendererScheduler::WillBeginFrame(const cc::BeginFrameArgs& args) {
}

void NullRendererScheduler::BeginFrameNotExpectedSoon() {
}

void NullRendererScheduler::DidCommitFrameToCompositor() {
}

void NullRendererScheduler::DidHandleInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event,
    InputEventState event_state) {
}

void NullRendererScheduler::DidHandleInputEventOnMainThread(
    const blink::WebInputEvent& web_input_event) {
}

void NullRendererScheduler::DidAnimateForInputOnCompositorThread() {
}

void NullRendererScheduler::OnRendererHidden() {
}

void NullRendererScheduler::OnRendererVisible() {
}

void NullRendererScheduler::OnPageLoadStarted() {
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

void NullRendererScheduler::SuspendTimerQueue() {
}

void NullRendererScheduler::ResumeTimerQueue() {
}

}  // namespace scheduler
