// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/proxy_main_for_test.h"

#include "cc/test/threaded_channel_for_test.h"

namespace cc {

scoped_ptr<ProxyMain> ProxyMainForTest::CreateThreaded(
    TestHooks* test_hooks,
    LayerTreeHost* host,
    TaskRunnerProvider* task_runner_provider,
    scoped_ptr<BeginFrameSource> external_begin_frame_source) {
  scoped_ptr<ProxyMain> proxy_main(
      new ProxyMainForTest(test_hooks, host, task_runner_provider,
                           std::move(external_begin_frame_source)));
  proxy_main->SetChannel(ThreadedChannelForTest::Create(
      test_hooks, proxy_main.get(), task_runner_provider));
  return proxy_main;
}

ProxyMainForTest::~ProxyMainForTest() {}

ProxyMainForTest::ProxyMainForTest(
    TestHooks* test_hooks,
    LayerTreeHost* host,
    TaskRunnerProvider* task_runner_provider,
    scoped_ptr<BeginFrameSource> external_begin_frame_source)
    : ProxyMain(host,
                task_runner_provider,
                std::move(external_begin_frame_source)),
      test_hooks_(test_hooks) {}

void ProxyMainForTest::SetNeedsUpdateLayers() {
  ProxyMain::SetNeedsUpdateLayers();
  test_hooks_->DidSetNeedsUpdateLayers();
}

void ProxyMainForTest::DidCompleteSwapBuffers() {
  test_hooks_->ReceivedDidCompleteSwapBuffers();
  ProxyMain::DidCompleteSwapBuffers();
}

void ProxyMainForTest::SetRendererCapabilities(
    const RendererCapabilities& capabilities) {
  test_hooks_->ReceivedSetRendererCapabilitiesMainCopy(capabilities);
  ProxyMain::SetRendererCapabilities(capabilities);
}

void ProxyMainForTest::BeginMainFrameNotExpectedSoon() {
  test_hooks_->ReceivedBeginMainFrameNotExpectedSoon();
  ProxyMain::BeginMainFrameNotExpectedSoon();
}

void ProxyMainForTest::DidCommitAndDrawFrame() {
  test_hooks_->ReceivedDidCommitAndDrawFrame();
  ProxyMain::DidCommitAndDrawFrame();
}

void ProxyMainForTest::SetAnimationEvents(
    scoped_ptr<AnimationEventsVector> events) {
  test_hooks_->ReceivedSetAnimationEvents();
  ProxyMain::SetAnimationEvents(std::move(events));
}

void ProxyMainForTest::DidLoseOutputSurface() {
  test_hooks_->ReceivedDidLoseOutputSurface();
  ProxyMain::DidLoseOutputSurface();
}

void ProxyMainForTest::RequestNewOutputSurface() {
  test_hooks_->ReceivedRequestNewOutputSurface();
  ProxyMain::RequestNewOutputSurface();
}

void ProxyMainForTest::DidInitializeOutputSurface(
    bool success,
    const RendererCapabilities& capabilities) {
  test_hooks_->ReceivedDidInitializeOutputSurface(success, capabilities);
  ProxyMain::DidInitializeOutputSurface(success, capabilities);
}

void ProxyMainForTest::DidCompletePageScaleAnimation() {
  test_hooks_->ReceivedDidCompletePageScaleAnimation();
  ProxyMain::DidCompletePageScaleAnimation();
}

void ProxyMainForTest::PostFrameTimingEventsOnMain(
    scoped_ptr<FrameTimingTracker::CompositeTimingSet> composite_events,
    scoped_ptr<FrameTimingTracker::MainFrameTimingSet> main_frame_events) {
  test_hooks_->ReceivedPostFrameTimingEventsOnMain();
  ProxyMain::PostFrameTimingEventsOnMain(std::move(composite_events),
                                         std::move(main_frame_events));
}

void ProxyMainForTest::BeginMainFrame(
    scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) {
  test_hooks_->ReceivedBeginMainFrame();
  ProxyMain::BeginMainFrame(std::move(begin_main_frame_state));
}

}  // namespace cc
