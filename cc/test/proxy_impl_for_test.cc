// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/proxy_impl_for_test.h"

namespace cc {
scoped_ptr<ProxyImplForTest> ProxyImplForTest::Create(
    TestHooks* test_hooks,
    ChannelImpl* channel_impl,
    LayerTreeHost* layer_tree_host,
    TaskRunnerProvider* task_runner_provider,
    scoped_ptr<BeginFrameSource> external_begin_frame_source) {
  return make_scoped_ptr(new ProxyImplForTest(
      test_hooks, channel_impl, layer_tree_host, task_runner_provider,
      std::move(external_begin_frame_source)));
}

bool ProxyImplForTest::HasCommitCompletionEvent() const {
  return commit_completion_event_ != nullptr;
}

bool ProxyImplForTest::GetNextCommitWaitsForActivation() const {
  return next_commit_waits_for_activation_;
}

ProxyImplForTest::ProxyImplForTest(
    TestHooks* test_hooks,
    ChannelImpl* channel_impl,
    LayerTreeHost* layer_tree_host,
    TaskRunnerProvider* task_runner_provider,
    scoped_ptr<BeginFrameSource> external_begin_frame_source)
    : ProxyImpl(channel_impl,
                layer_tree_host,
                task_runner_provider,
                std::move(external_begin_frame_source)),
      test_hooks_(test_hooks) {}

void ProxyImplForTest::ScheduledActionSendBeginMainFrame(
    const BeginFrameArgs& args) {
  test_hooks_->ScheduledActionWillSendBeginMainFrame();
  ProxyImpl::ScheduledActionSendBeginMainFrame(args);
  test_hooks_->ScheduledActionSendBeginMainFrame();
}

DrawResult ProxyImplForTest::ScheduledActionDrawAndSwapIfPossible() {
  DrawResult result = ProxyImpl::ScheduledActionDrawAndSwapIfPossible();
  test_hooks_->ScheduledActionDrawAndSwapIfPossible();
  return result;
}

void ProxyImplForTest::ScheduledActionCommit() {
  ProxyImpl::ScheduledActionCommit();
  test_hooks_->ScheduledActionCommit();
}

void ProxyImplForTest::ScheduledActionBeginOutputSurfaceCreation() {
  ProxyImpl::ScheduledActionBeginOutputSurfaceCreation();
  test_hooks_->ScheduledActionBeginOutputSurfaceCreation();
}

void ProxyImplForTest::ScheduledActionPrepareTiles() {
  ProxyImpl::ScheduledActionPrepareTiles();
  test_hooks_->ScheduledActionPrepareTiles();
}

void ProxyImplForTest::ScheduledActionInvalidateOutputSurface() {
  ProxyImpl::ScheduledActionInvalidateOutputSurface();
  test_hooks_->ScheduledActionInvalidateOutputSurface();
}

void ProxyImplForTest::SendBeginMainFrameNotExpectedSoon() {
  ProxyImpl::SendBeginMainFrameNotExpectedSoon();
  test_hooks_->SendBeginMainFrameNotExpectedSoon();
}

void ProxyImplForTest::DidActivateSyncTree() {
  ProxyImpl::DidActivateSyncTree();
  test_hooks_->DidActivateSyncTree();
}

void ProxyImplForTest::SetThrottleFrameProductionOnImpl(bool throttle) {
  test_hooks_->SetThrottleFrameProductionOnImpl(throttle);
  ProxyImpl::SetThrottleFrameProductionOnImpl(throttle);
}

void ProxyImplForTest::InitializeOutputSurfaceOnImpl(
    OutputSurface* output_surface) {
  test_hooks_->InitializeOutputSurfaceOnImpl(output_surface);
  ProxyImpl::InitializeOutputSurfaceOnImpl(output_surface);
}

void ProxyImplForTest::MainThreadHasStoppedFlingingOnImpl() {
  test_hooks_->MainThreadHasStoppedFlingingOnImpl();
  ProxyImpl::MainThreadHasStoppedFlingingOnImpl();
}

void ProxyImplForTest::SetInputThrottledUntilCommitOnImpl(bool is_throttled) {
  test_hooks_->SetInputThrottledUntilCommitOnImpl(is_throttled);
  ProxyImpl::SetInputThrottledUntilCommitOnImpl(is_throttled);
}

void ProxyImplForTest::UpdateTopControlsStateOnImpl(
    TopControlsState constraints,
    TopControlsState current,
    bool animate) {
  test_hooks_->UpdateTopControlsStateOnImpl(constraints, current, animate);
  ProxyImpl::UpdateTopControlsStateOnImpl(constraints, current, animate);
}

void ProxyImplForTest::SetDeferCommitsOnImpl(bool defer_commits) const {
  test_hooks_->SetDeferCommitsOnImpl(defer_commits);
  ProxyImpl::SetDeferCommitsOnImpl(defer_commits);
}

void ProxyImplForTest::BeginMainFrameAbortedOnImpl(
    CommitEarlyOutReason reason,
    base::TimeTicks main_thread_start_time) {
  test_hooks_->BeginMainFrameAbortedOnImpl(reason);
  ProxyImpl::BeginMainFrameAbortedOnImpl(reason, main_thread_start_time);
}

void ProxyImplForTest::SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) {
  test_hooks_->SetNeedsRedrawOnImpl(damage_rect);
  ProxyImpl::SetNeedsRedrawOnImpl(damage_rect);
}

void ProxyImplForTest::SetNeedsCommitOnImpl() {
  test_hooks_->SetNeedsCommitOnImpl();
  ProxyImpl::SetNeedsCommitOnImpl();
}

void ProxyImplForTest::FinishAllRenderingOnImpl(CompletionEvent* completion) {
  test_hooks_->FinishAllRenderingOnImpl();
  ProxyImpl::FinishAllRenderingOnImpl(completion);
}

void ProxyImplForTest::SetVisibleOnImpl(bool visible) {
  test_hooks_->SetVisibleOnImpl(visible);
  ProxyImpl::SetVisibleOnImpl(visible);
}

void ProxyImplForTest::ReleaseOutputSurfaceOnImpl(CompletionEvent* completion) {
  test_hooks_->ReleaseOutputSurfaceOnImpl();
  ProxyImpl::ReleaseOutputSurfaceOnImpl(completion);
}

void ProxyImplForTest::FinishGLOnImpl(CompletionEvent* completion) {
  test_hooks_->FinishGLOnImpl();
  ProxyImpl::FinishGLOnImpl(completion);
}

void ProxyImplForTest::StartCommitOnImpl(CompletionEvent* completion,
                                         LayerTreeHost* layer_tree_host,
                                         base::TimeTicks main_thread_start_time,
                                         bool hold_commit_for_activation) {
  test_hooks_->StartCommitOnImpl();
  ProxyImpl::StartCommitOnImpl(completion, layer_tree_host,
                               main_thread_start_time,
                               hold_commit_for_activation);
}

}  // namespace cc
