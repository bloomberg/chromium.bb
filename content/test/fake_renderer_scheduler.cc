// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_renderer_scheduler.h"

namespace content {

FakeRendererScheduler::FakeRendererScheduler() {
}

FakeRendererScheduler::~FakeRendererScheduler() {
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeRendererScheduler::DefaultTaskRunner() {
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeRendererScheduler::CompositorTaskRunner() {
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeRendererScheduler::LoadingTaskRunner() {
  return nullptr;
}

scoped_refptr<SingleThreadIdleTaskRunner>
FakeRendererScheduler::IdleTaskRunner() {
  return nullptr;
}

void FakeRendererScheduler::WillBeginFrame(const cc::BeginFrameArgs& args) {
}

void FakeRendererScheduler::BeginFrameNotExpectedSoon() {
}

void FakeRendererScheduler::DidCommitFrameToCompositor() {
}

void FakeRendererScheduler::DidReceiveInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event) {
}

void FakeRendererScheduler::DidAnimateForInputOnCompositorThread() {
}

bool FakeRendererScheduler::IsHighPriorityWorkAnticipated() {
  return false;
}

bool FakeRendererScheduler::ShouldYieldForHighPriorityWork() {
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

}  // namespace content
