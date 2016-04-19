// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PROXY_IMPL_FOR_TEST_H_
#define CC_TEST_PROXY_IMPL_FOR_TEST_H_

#include "base/macros.h"
#include "cc/test/test_hooks.h"
#include "cc/trees/proxy_impl.h"

namespace cc {
// Creates a ProxyImpl that notifies the supplied |test_hooks| of various
// actions.
class ProxyImplForTest : public ProxyImpl {
 public:
  static std::unique_ptr<ProxyImplForTest> Create(
      TestHooks* test_hooks,
      ChannelImpl* channel_impl,
      LayerTreeHost* layer_tree_host,
      TaskRunnerProvider* task_runner_provider,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source);

  using ProxyImpl::PostAnimationEventsToMainThreadOnImplThread;
  using ProxyImpl::DidLoseOutputSurfaceOnImplThread;
  using ProxyImpl::DidCompletePageScaleAnimationOnImplThread;
  using ProxyImpl::SendBeginMainFrameNotExpectedSoon;

  bool HasCommitCompletionEvent() const;
  bool GetNextCommitWaitsForActivation() const;
  const DelayedUniqueNotifier& smoothness_priority_expiration_notifier() const {
    return smoothness_priority_expiration_notifier_;
  }

  ProxyImplForTest(
      TestHooks* test_hooks,
      ChannelImpl* channel_impl,
      LayerTreeHost* layer_tree_host,
      TaskRunnerProvider* task_runner_provider,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source);

  void ScheduledActionSendBeginMainFrame(const BeginFrameArgs& args) override;
  DrawResult ScheduledActionDrawAndSwapIfPossible() override;
  void ScheduledActionCommit() override;
  void ScheduledActionBeginOutputSurfaceCreation() override;
  void ScheduledActionPrepareTiles() override;
  void ScheduledActionInvalidateOutputSurface() override;
  void SendBeginMainFrameNotExpectedSoon() override;
  void DidActivateSyncTree() override;
  void InitializeOutputSurfaceOnImpl(OutputSurface* output_surface) override;
  void MainThreadHasStoppedFlingingOnImpl() override;
  void SetInputThrottledUntilCommitOnImpl(bool is_throttled) override;
  void UpdateTopControlsStateOnImpl(TopControlsState constraints,
                                    TopControlsState current,
                                    bool animate) override;
  void SetDeferCommitsOnImpl(bool defer_commits) const override;
  void BeginMainFrameAbortedOnImpl(
      CommitEarlyOutReason reason,
      base::TimeTicks main_thread_start_time) override;
  void SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) override;
  void SetNeedsCommitOnImpl() override;
  void FinishAllRenderingOnImpl(CompletionEvent* completion) override;
  void SetVisibleOnImpl(bool visible) override;
  void ReleaseOutputSurfaceOnImpl(CompletionEvent* completion) override;
  void FinishGLOnImpl(CompletionEvent* completion) override;
  void StartCommitOnImpl(CompletionEvent* completion,
                         LayerTreeHost* layer_tree_host,
                         base::TimeTicks main_thread_start_time,
                         bool hold_commit_for_activation) override;

  TestHooks* test_hooks_;
};

}  // namespace cc

#endif  // CC_TEST_PROXY_IMPL_FOR_TEST_H_
