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

scoped_refptr<SingleThreadIdleTaskRunner>
FakeRendererScheduler::IdleTaskRunner() {
  return nullptr;
}

void FakeRendererScheduler::WillBeginFrame(const cc::BeginFrameArgs& args) {
}

void FakeRendererScheduler::DidCommitFrameToCompositor() {
}

void FakeRendererScheduler::DidReceiveInputEventOnCompositorThread(
    blink::WebInputEvent::Type type) {
}

void FakeRendererScheduler::DidAnimateForInputOnCompositorThread() {
}

bool FakeRendererScheduler::ShouldYieldForHighPriorityWork() {
  return false;
}

void FakeRendererScheduler::Shutdown() {
}

}  // namespace content
