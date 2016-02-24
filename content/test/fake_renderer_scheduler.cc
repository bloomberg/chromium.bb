// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_renderer_scheduler.h"

#include "third_party/WebKit/public/platform/WebThread.h"

namespace content {

FakeRendererScheduler::FakeRendererScheduler() {
}

FakeRendererScheduler::~FakeRendererScheduler() {
}

scoped_ptr<blink::WebThread> FakeRendererScheduler::CreateMainThread() {
  return nullptr;
}

scoped_refptr<scheduler::TaskQueue> FakeRendererScheduler::DefaultTaskRunner() {
  return nullptr;
}

scoped_refptr<scheduler::TaskQueue>
FakeRendererScheduler::CompositorTaskRunner() {
  return nullptr;
}

scoped_refptr<scheduler::TaskQueue> FakeRendererScheduler::LoadingTaskRunner() {
  return nullptr;
}

scoped_refptr<scheduler::SingleThreadIdleTaskRunner>
FakeRendererScheduler::IdleTaskRunner() {
  return nullptr;
}

scoped_refptr<scheduler::TaskQueue> FakeRendererScheduler::TimerTaskRunner() {
  return nullptr;
}

scoped_refptr<scheduler::TaskQueue> FakeRendererScheduler::NewLoadingTaskRunner(
    const char* name) {
  return nullptr;
}

scoped_refptr<scheduler::TaskQueue> FakeRendererScheduler::NewTimerTaskRunner(
    const char* name) {
  return nullptr;
}

scoped_ptr<scheduler::RenderWidgetSchedulingState>
FakeRendererScheduler::NewRenderWidgetSchedulingState() {
  return nullptr;
}

void FakeRendererScheduler::WillBeginFrame(const cc::BeginFrameArgs& args) {
}

void FakeRendererScheduler::BeginFrameNotExpectedSoon() {
}

void FakeRendererScheduler::DidCommitFrameToCompositor() {
}

void FakeRendererScheduler::DidHandleInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event,
    InputEventState event_state) {
}

void FakeRendererScheduler::DidHandleInputEventOnMainThread(
    const blink::WebInputEvent& web_input_event) {
}

void FakeRendererScheduler::DidAnimateForInputOnCompositorThread() {
}

bool FakeRendererScheduler::IsHighPriorityWorkAnticipated() {
  return false;
}

void FakeRendererScheduler::OnRendererBackgrounded() {
}

void FakeRendererScheduler::OnRendererForegrounded() {
}

void FakeRendererScheduler::AddPendingNavigation() {
}

void FakeRendererScheduler::RemovePendingNavigation() {
}

void FakeRendererScheduler::OnNavigationStarted() {
}

bool FakeRendererScheduler::ShouldYieldForHighPriorityWork() {
  return false;
}

bool FakeRendererScheduler::CanExceedIdleDeadlineIfRequired() const {
  return false;
}

void FakeRendererScheduler::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
}

void FakeRendererScheduler::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
}

void FakeRendererScheduler::Shutdown() {
}

void FakeRendererScheduler::SuspendTimerQueue() {
}

void FakeRendererScheduler::ResumeTimerQueue() {
}

void FakeRendererScheduler::SetTimerQueueSuspensionWhenBackgroundedEnabled(
    bool enabled) {}

double FakeRendererScheduler::VirtualTimeSeconds() const {
  return 0.0;
}

double FakeRendererScheduler::
    MonotonicallyIncreasingVirtualTimeSeconds() const {
  return 0.0;
}

}  // namespace content
