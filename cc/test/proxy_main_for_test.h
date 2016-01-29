// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PROXY_MAIN_FOR_TEST_H_
#define CC_TEST_PROXY_MAIN_FOR_TEST_H_

#include "base/macros.h"
#include "cc/test/test_hooks.h"
#include "cc/trees/proxy_main.h"

namespace cc {
class ThreadedChannelForTest;

// Creates a ProxyMain that notifies the supplied |test_hooks| of various
// actions.
class ProxyMainForTest : public ProxyMain {
 public:
  static scoped_ptr<ProxyMainForTest> CreateThreaded(
      TestHooks* test_hooks,
      LayerTreeHost* host,
      TaskRunnerProvider* task_runner_provider);

  static scoped_ptr<ProxyMainForTest> CreateRemote(
      TestHooks* test_hooks,
      RemoteProtoChannel* remote_proto_channel,
      LayerTreeHost* host,
      TaskRunnerProvider* task_runner_provider);

  ~ProxyMainForTest() override;

  ProxyMainForTest(TestHooks* test_hooks,
                   LayerTreeHost* host,
                   TaskRunnerProvider* task_runner_provider);

  ThreadedChannelForTest* threaded_channel_for_test() const {
    return threaded_channel_for_test_;
  }

  void SetNeedsUpdateLayers() override;
  void DidCompleteSwapBuffers() override;
  void SetRendererCapabilities(
      const RendererCapabilities& capabilities) override;
  void BeginMainFrameNotExpectedSoon() override;
  void DidCommitAndDrawFrame() override;
  void SetAnimationEvents(scoped_ptr<AnimationEvents> events) override;
  void DidLoseOutputSurface() override;
  void RequestNewOutputSurface() override;
  void DidInitializeOutputSurface(
      bool success,
      const RendererCapabilities& capabilities) override;
  void DidCompletePageScaleAnimation() override;
  void PostFrameTimingEventsOnMain(
      scoped_ptr<FrameTimingTracker::CompositeTimingSet> composite_events,
      scoped_ptr<FrameTimingTracker::MainFrameTimingSet> main_frame_events)
      override;
  void BeginMainFrame(
      scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) override;

  TestHooks* test_hooks_;
  ThreadedChannelForTest* threaded_channel_for_test_;
};

}  // namespace cc

#endif  // CC_TEST_PROXY_MAIN_FOR_TEST_H_
