// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/threaded_channel.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"

namespace cc {

scoped_ptr<ThreadedChannel> ThreadedChannel::Create(
    ThreadProxy* thread_proxy,
    TaskRunnerProvider* task_runner_provider) {
  return make_scoped_ptr(
      new ThreadedChannel(thread_proxy, task_runner_provider));
}

ThreadedChannel::ThreadedChannel(ThreadProxy* thread_proxy,
                                 TaskRunnerProvider* task_runner_provider)
    : proxy_main_(thread_proxy),
      proxy_impl_(thread_proxy),
      task_runner_provider_(task_runner_provider) {}

void ThreadedChannel::SetThrottleFrameProductionOnImpl(bool throttle) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::SetThrottleFrameProductionOnImpl,
                            proxy_impl_->GetImplWeakPtr(), throttle));
}

void ThreadedChannel::UpdateTopControlsStateOnImpl(TopControlsState constraints,
                                                   TopControlsState current,
                                                   bool animate) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyImpl::UpdateTopControlsStateOnImpl,
                 proxy_impl_->GetImplWeakPtr(), constraints, current, animate));
}

void ThreadedChannel::InitializeOutputSurfaceOnImpl(
    OutputSurface* output_surface) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::InitializeOutputSurfaceOnImpl,
                            proxy_impl_->GetImplWeakPtr(), output_surface));
}

void ThreadedChannel::MainThreadHasStoppedFlingingOnImpl() {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::MainThreadHasStoppedFlingingOnImpl,
                            proxy_impl_->GetImplWeakPtr()));
}

void ThreadedChannel::SetInputThrottledUntilCommitOnImpl(bool is_throttled) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::SetInputThrottledUntilCommitOnImpl,
                            proxy_impl_->GetImplWeakPtr(), is_throttled));
}

void ThreadedChannel::SetDeferCommitsOnImpl(bool defer_commits) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::SetDeferCommitsOnImpl,
                            proxy_impl_->GetImplWeakPtr(), defer_commits));
}

void ThreadedChannel::FinishAllRenderingOnImpl(CompletionEvent* completion) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::FinishAllRenderingOnImpl,
                            proxy_impl_->GetImplWeakPtr(), completion));
}

void ThreadedChannel::SetNeedsCommitOnImpl() {
  ImplThreadTaskRunner()->PostTask(FROM_HERE,
                                   base::Bind(&ProxyImpl::SetNeedsCommitOnImpl,
                                              proxy_impl_->GetImplWeakPtr()));
}

void ThreadedChannel::BeginMainFrameAbortedOnImpl(
    CommitEarlyOutReason reason,
    base::TimeTicks main_thread_start_time) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::BeginMainFrameAbortedOnImpl,
                            proxy_impl_->GetImplWeakPtr(), reason,
                            main_thread_start_time));
}

void ThreadedChannel::SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::SetNeedsRedrawOnImpl,
                            proxy_impl_->GetImplWeakPtr(), damage_rect));
}

void ThreadedChannel::StartCommitOnImpl(CompletionEvent* completion,
                                        LayerTreeHost* layer_tree_host,
                                        base::TimeTicks main_thread_start_time,
                                        bool hold_commit_for_activation) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyImpl::StartCommitOnImpl, proxy_impl_->GetImplWeakPtr(),
                 completion, layer_tree_host, main_thread_start_time,
                 hold_commit_for_activation));
}

void ThreadedChannel::InitializeImplOnImpl(CompletionEvent* completion,
                                           LayerTreeHost* layer_tree_host) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyImpl::InitializeImplOnImpl,
                 base::Unretained(proxy_impl_), completion, layer_tree_host));
}

void ThreadedChannel::LayerTreeHostClosedOnImpl(CompletionEvent* completion) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::LayerTreeHostClosedOnImpl,
                            proxy_impl_->GetImplWeakPtr(), completion));
  proxy_impl_ = nullptr;
}

void ThreadedChannel::SetVisibleOnImpl(bool visible) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::SetVisibleOnImpl,
                            proxy_impl_->GetImplWeakPtr(), visible));
}

void ThreadedChannel::ReleaseOutputSurfaceOnImpl(CompletionEvent* completion) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::ReleaseOutputSurfaceOnImpl,
                            proxy_impl_->GetImplWeakPtr(), completion));
}

void ThreadedChannel::FinishGLOnImpl(CompletionEvent* completion) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::FinishGLOnImpl,
                            proxy_impl_->GetImplWeakPtr(), completion));
}

void ThreadedChannel::MainFrameWillHappenOnImplForTesting(
    CompletionEvent* completion,
    bool* main_frame_will_happen) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyImpl::MainFrameWillHappenOnImplForTesting,
                            proxy_impl_->GetImplWeakPtr(), completion,
                            main_frame_will_happen));
}

void ThreadedChannel::DidCompleteSwapBuffers() {
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyMain::DidCompleteSwapBuffers,
                            proxy_main_->GetMainWeakPtr()));
}

void ThreadedChannel::SetRendererCapabilitiesMainCopy(
    const RendererCapabilities& capabilities) {
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyMain::SetRendererCapabilitiesMainCopy,
                            proxy_main_->GetMainWeakPtr(), capabilities));
}

void ThreadedChannel::BeginMainFrameNotExpectedSoon() {
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyMain::BeginMainFrameNotExpectedSoon,
                            proxy_main_->GetMainWeakPtr()));
}

void ThreadedChannel::DidCommitAndDrawFrame() {
  MainThreadTaskRunner()->PostTask(FROM_HERE,
                                   base::Bind(&ProxyMain::DidCommitAndDrawFrame,
                                              proxy_main_->GetMainWeakPtr()));
}

void ThreadedChannel::SetAnimationEvents(
    scoped_ptr<AnimationEventsVector> queue) {
  MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyMain::SetAnimationEvents, proxy_main_->GetMainWeakPtr(),
                 base::Passed(&queue)));
}

void ThreadedChannel::DidLoseOutputSurface() {
  MainThreadTaskRunner()->PostTask(FROM_HERE,
                                   base::Bind(&ProxyMain::DidLoseOutputSurface,
                                              proxy_main_->GetMainWeakPtr()));
}

void ThreadedChannel::RequestNewOutputSurface() {
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyMain::RequestNewOutputSurface,
                            proxy_main_->GetMainWeakPtr()));
}

void ThreadedChannel::DidInitializeOutputSurface(
    bool success,
    const RendererCapabilities& capabilities) {
  MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyMain::DidInitializeOutputSurface,
                 proxy_main_->GetMainWeakPtr(), success, capabilities));
}

void ThreadedChannel::DidCompletePageScaleAnimation() {
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyMain::DidCompletePageScaleAnimation,
                            proxy_main_->GetMainWeakPtr()));
}

void ThreadedChannel::PostFrameTimingEventsOnMain(
    scoped_ptr<FrameTimingTracker::CompositeTimingSet> composite_events,
    scoped_ptr<FrameTimingTracker::MainFrameTimingSet> main_frame_events) {
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ProxyMain::PostFrameTimingEventsOnMain,
                            proxy_main_->GetMainWeakPtr(),
                            base::Passed(composite_events.Pass()),
                            base::Passed(main_frame_events.Pass())));
}

void ThreadedChannel::BeginMainFrame(
    scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) {
  MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ProxyMain::BeginMainFrame, proxy_main_->GetMainWeakPtr(),
                 base::Passed(&begin_main_frame_state)));
}

ThreadedChannel::~ThreadedChannel() {
  TRACE_EVENT0("cc", "ThreadChannel::~ThreadChannel");
}

base::SingleThreadTaskRunner* ThreadedChannel::MainThreadTaskRunner() const {
  return task_runner_provider_->MainThreadTaskRunner();
}

base::SingleThreadTaskRunner* ThreadedChannel::ImplThreadTaskRunner() const {
  return task_runner_provider_->ImplThreadTaskRunner();
}

}  // namespace cc
