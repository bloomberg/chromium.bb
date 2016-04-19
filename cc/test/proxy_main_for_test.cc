// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/proxy_main_for_test.h"

#include "cc/animation/animation_events.h"
#include "cc/test/threaded_channel_for_test.h"
#include "cc/trees/remote_channel_main.h"

namespace cc {

std::unique_ptr<ProxyMainForTest> ProxyMainForTest::CreateThreaded(
    TestHooks* test_hooks,
    LayerTreeHost* host,
    TaskRunnerProvider* task_runner_provider) {
  std::unique_ptr<ProxyMainForTest> proxy_main(
      new ProxyMainForTest(test_hooks, host, task_runner_provider));
  std::unique_ptr<ThreadedChannelForTest> channel =
      ThreadedChannelForTest::Create(test_hooks, proxy_main.get(),
                                     task_runner_provider);
  proxy_main->threaded_channel_for_test_ = channel.get();
  proxy_main->SetChannel(std::move(channel));
  return proxy_main;
}

std::unique_ptr<ProxyMainForTest> ProxyMainForTest::CreateRemote(
    TestHooks* test_hooks,
    RemoteProtoChannel* remote_proto_channel,
    LayerTreeHost* host,
    TaskRunnerProvider* task_runner_provider) {
  std::unique_ptr<ProxyMainForTest> proxy_main(
      new ProxyMainForTest(test_hooks, host, task_runner_provider));
  proxy_main->SetChannel(RemoteChannelMain::Create(
      remote_proto_channel, proxy_main.get(), task_runner_provider));
  return proxy_main;
}

ProxyMainForTest::~ProxyMainForTest() {}

ProxyMainForTest::ProxyMainForTest(TestHooks* test_hooks,
                                   LayerTreeHost* host,
                                   TaskRunnerProvider* task_runner_provider)
    : ProxyMain(host, task_runner_provider),
      test_hooks_(test_hooks),
      threaded_channel_for_test_(nullptr) {}

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
    std::unique_ptr<AnimationEvents> events) {
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

void ProxyMainForTest::BeginMainFrame(
    std::unique_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) {
  test_hooks_->ReceivedBeginMainFrame();
  ProxyMain::BeginMainFrame(std::move(begin_main_frame_state));
}

}  // namespace cc
