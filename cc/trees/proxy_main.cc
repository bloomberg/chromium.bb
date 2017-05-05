// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/proxy_main.h"

#include <algorithm>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/trace_event/trace_event_synthetic_delay.h"
#include "cc/base/completion_event.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/benchmarks/benchmark_instrumentation.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/output/swap_promise.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/trees/blocking_task_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/proxy_impl.h"
#include "cc/trees/scoped_abort_remaining_swap_promises.h"

namespace cc {

ProxyMain::ProxyMain(LayerTreeHost* layer_tree_host,
                     TaskRunnerProvider* task_runner_provider)
    : layer_tree_host_(layer_tree_host),
      task_runner_provider_(task_runner_provider),
      layer_tree_host_id_(layer_tree_host->GetId()),
      max_requested_pipeline_stage_(NO_PIPELINE_STAGE),
      current_pipeline_stage_(NO_PIPELINE_STAGE),
      final_pipeline_stage_(NO_PIPELINE_STAGE),
      commit_waits_for_activation_(false),
      started_(false),
      defer_commits_(false),
      frame_sink_bound_weak_factory_(this),
      weak_factory_(this) {
  TRACE_EVENT0("cc", "ProxyMain::ProxyMain");
  DCHECK(task_runner_provider_);
  DCHECK(IsMainThread());
}

ProxyMain::~ProxyMain() {
  TRACE_EVENT0("cc", "ProxyMain::~ProxyMain");
  DCHECK(IsMainThread());
  DCHECK(!started_);
}

void ProxyMain::InitializeOnImplThread(CompletionEvent* completion_event) {
  DCHECK(task_runner_provider_->IsImplThread());
  DCHECK(!proxy_impl_);
  proxy_impl_ = base::MakeUnique<ProxyImpl>(
      weak_factory_.GetWeakPtr(), layer_tree_host_, task_runner_provider_);
  completion_event->Signal();
}

void ProxyMain::DestroyProxyImplOnImplThread(
    CompletionEvent* completion_event) {
  DCHECK(task_runner_provider_->IsImplThread());
  proxy_impl_.reset();
  completion_event->Signal();
}

void ProxyMain::DidReceiveCompositorFrameAck() {
  DCHECK(IsMainThread());
  layer_tree_host_->DidReceiveCompositorFrameAck();
}

void ProxyMain::BeginMainFrameNotExpectedSoon() {
  TRACE_EVENT0("cc", "ProxyMain::BeginMainFrameNotExpectedSoon");
  DCHECK(IsMainThread());
  layer_tree_host_->BeginMainFrameNotExpectedSoon();
}

void ProxyMain::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {
  TRACE_EVENT0("cc", "ProxyMain::BeginMainFrameNotExpectedUntil");
  DCHECK(IsMainThread());
  layer_tree_host_->BeginMainFrameNotExpectedUntil(time);
}

void ProxyMain::DidCommitAndDrawFrame() {
  DCHECK(IsMainThread());
  layer_tree_host_->DidCommitAndDrawFrame();
}

void ProxyMain::SetAnimationEvents(std::unique_ptr<MutatorEvents> events) {
  TRACE_EVENT0("cc", "ProxyMain::SetAnimationEvents");
  DCHECK(IsMainThread());
  layer_tree_host_->SetAnimationEvents(std::move(events));
}

void ProxyMain::DidLoseCompositorFrameSink() {
  TRACE_EVENT0("cc", "ProxyMain::DidLoseCompositorFrameSink");
  DCHECK(IsMainThread());
  layer_tree_host_->DidLoseCompositorFrameSink();
}

void ProxyMain::RequestNewCompositorFrameSink() {
  TRACE_EVENT0("cc", "ProxyMain::RequestNewCompositorFrameSink");
  DCHECK(IsMainThread());
  layer_tree_host_->RequestNewCompositorFrameSink();
}

void ProxyMain::DidInitializeCompositorFrameSink(bool success) {
  TRACE_EVENT0("cc", "ProxyMain::DidInitializeCompositorFrameSink");
  DCHECK(IsMainThread());

  if (!success)
    layer_tree_host_->DidFailToInitializeCompositorFrameSink();
  else
    layer_tree_host_->DidInitializeCompositorFrameSink();
}

void ProxyMain::DidCompletePageScaleAnimation() {
  DCHECK(IsMainThread());
  layer_tree_host_->DidCompletePageScaleAnimation();
}

void ProxyMain::BeginMainFrame(
    std::unique_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) {
  benchmark_instrumentation::ScopedBeginFrameTask begin_frame_task(
      benchmark_instrumentation::kDoBeginFrame,
      begin_main_frame_state->begin_frame_id);

  base::TimeTicks begin_main_frame_start_time = base::TimeTicks::Now();

  TRACE_EVENT_SYNTHETIC_DELAY_BEGIN("cc.BeginMainFrame");
  DCHECK(IsMainThread());
  DCHECK_EQ(NO_PIPELINE_STAGE, current_pipeline_stage_);

  // We need to issue image decode callbacks whether or not we will abort this
  // commit, since the callbacks are only stored in |begin_main_frame_state|.
  for (auto& callback :
       begin_main_frame_state->completed_image_decode_callbacks) {
    callback.Run();
  }

  if (defer_commits_) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_DeferCommit",
                         TRACE_EVENT_SCOPE_THREAD);
    std::vector<std::unique_ptr<SwapPromise>> empty_swap_promises;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProxyImpl::BeginMainFrameAbortedOnImpl,
                                  base::Unretained(proxy_impl_.get()),
                                  CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT,
                                  begin_main_frame_start_time,
                                  base::Passed(&empty_swap_promises)));
    return;
  }

  // If the commit finishes, LayerTreeHost will transfer its swap promises to
  // LayerTreeImpl. The destructor of ScopedSwapPromiseChecker aborts the
  // remaining swap promises.
  ScopedAbortRemainingSwapPromises swap_promise_checker(
      layer_tree_host_->GetSwapPromiseManager());

  final_pipeline_stage_ = max_requested_pipeline_stage_;
  max_requested_pipeline_stage_ = NO_PIPELINE_STAGE;

  if (!layer_tree_host_->IsVisible()) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NotVisible", TRACE_EVENT_SCOPE_THREAD);
    std::vector<std::unique_ptr<SwapPromise>> empty_swap_promises;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProxyImpl::BeginMainFrameAbortedOnImpl,
                                  base::Unretained(proxy_impl_.get()),
                                  CommitEarlyOutReason::ABORTED_NOT_VISIBLE,
                                  begin_main_frame_start_time,
                                  base::Passed(&empty_swap_promises)));
    return;
  }

  current_pipeline_stage_ = ANIMATE_PIPELINE_STAGE;

  layer_tree_host_->ApplyScrollAndScale(
      begin_main_frame_state->scroll_info.get());

  if (begin_main_frame_state->begin_frame_callbacks) {
    for (auto& callback : *begin_main_frame_state->begin_frame_callbacks)
      callback.Run();
  }

  layer_tree_host_->WillBeginMainFrame();

  layer_tree_host_->BeginMainFrame(begin_main_frame_state->begin_frame_args);
  layer_tree_host_->AnimateLayers(
      begin_main_frame_state->begin_frame_args.frame_time);

  // Recreate all UI resources if there were evicted UI resources when the impl
  // thread initiated the commit.
  if (begin_main_frame_state->evicted_ui_resources)
    layer_tree_host_->GetUIResourceManager()->RecreateUIResources();

  layer_tree_host_->RequestMainFrameUpdate();

  // At this point the main frame may have deferred commits to avoid committing
  // right now.
  if (defer_commits_) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_DeferCommit_InsideBeginMainFrame",
                         TRACE_EVENT_SCOPE_THREAD);
    std::vector<std::unique_ptr<SwapPromise>> empty_swap_promises;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProxyImpl::BeginMainFrameAbortedOnImpl,
                                  base::Unretained(proxy_impl_.get()),
                                  CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT,
                                  begin_main_frame_start_time,
                                  base::Passed(&empty_swap_promises)));
    current_pipeline_stage_ = NO_PIPELINE_STAGE;
    // We intentionally don't report CommitComplete() here since it was aborted
    // prematurely and we're waiting to do another commit in the future.
    layer_tree_host_->DidBeginMainFrame();
    return;
  }

  TRACE_EVENT_SYNTHETIC_DELAY_END("cc.BeginMainFrame");

  bool can_cancel_this_commit = final_pipeline_stage_ < COMMIT_PIPELINE_STAGE &&
                                !begin_main_frame_state->evicted_ui_resources;

  current_pipeline_stage_ = UPDATE_LAYERS_PIPELINE_STAGE;
  bool should_update_layers =
      final_pipeline_stage_ >= UPDATE_LAYERS_PIPELINE_STAGE;
  bool updated = should_update_layers && layer_tree_host_->UpdateLayers();

  layer_tree_host_->WillCommit();
  devtools_instrumentation::ScopedCommitTrace commit_task(
      layer_tree_host_->GetId());

  current_pipeline_stage_ = COMMIT_PIPELINE_STAGE;
  if (!updated && can_cancel_this_commit) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NoUpdates", TRACE_EVENT_SCOPE_THREAD);
    std::vector<std::unique_ptr<SwapPromise>> swap_promises =
        layer_tree_host_->GetSwapPromiseManager()->TakeSwapPromises();
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProxyImpl::BeginMainFrameAbortedOnImpl,
                                  base::Unretained(proxy_impl_.get()),
                                  CommitEarlyOutReason::FINISHED_NO_UPDATES,
                                  begin_main_frame_start_time,
                                  base::Passed(&swap_promises)));

    // Although the commit is internally aborted, this is because it has been
    // detected to be a no-op.  From the perspective of an embedder, this commit
    // went through, and input should no longer be throttled, etc.
    current_pipeline_stage_ = NO_PIPELINE_STAGE;
    layer_tree_host_->CommitComplete();
    layer_tree_host_->DidBeginMainFrame();
    return;
  }

  // Notify the impl thread that the main thread is ready to commit. This will
  // begin the commit process, which is blocking from the main thread's
  // point of view, but asynchronously performed on the impl thread,
  // coordinated by the Scheduler.
  {
    TRACE_EVENT0("cc", "ProxyMain::BeginMainFrame::commit");

    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);

    // This CapturePostTasks should be destroyed before CommitComplete() is
    // called since that goes out to the embedder, and we want the embedder
    // to receive its callbacks before that.
    BlockingTaskRunner::CapturePostTasks blocked(
        task_runner_provider_->blocking_main_thread_task_runner());

    bool hold_commit_for_activation = commit_waits_for_activation_;
    commit_waits_for_activation_ = false;
    CompletionEvent completion;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProxyImpl::NotifyReadyToCommitOnImpl,
                       base::Unretained(proxy_impl_.get()), &completion,
                       layer_tree_host_, begin_main_frame_start_time,
                       hold_commit_for_activation));
    completion.Wait();
  }

  current_pipeline_stage_ = NO_PIPELINE_STAGE;
  layer_tree_host_->CommitComplete();
  layer_tree_host_->DidBeginMainFrame();
}

bool ProxyMain::IsStarted() const {
  DCHECK(IsMainThread());
  return started_;
}

bool ProxyMain::CommitToActiveTree() const {
  // With ProxyMain, we use a pending tree and activate it once it's ready to
  // draw to allow input to modify the active tree and draw during raster.
  return false;
}

void ProxyMain::SetCompositorFrameSink(
    CompositorFrameSink* compositor_frame_sink) {
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyImpl::InitializeCompositorFrameSinkOnImpl,
                     base::Unretained(proxy_impl_.get()), compositor_frame_sink,
                     frame_sink_bound_weak_factory_.GetWeakPtr()));
}

void ProxyMain::SetVisible(bool visible) {
  TRACE_EVENT1("cc", "ProxyMain::SetVisible", "visible", visible);
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyImpl::SetVisibleOnImpl,
                                base::Unretained(proxy_impl_.get()), visible));
}

void ProxyMain::SetNeedsAnimate() {
  DCHECK(IsMainThread());
  if (SendCommitRequestToImplThreadIfNeeded(ANIMATE_PIPELINE_STAGE)) {
    TRACE_EVENT_INSTANT0("cc", "ProxyMain::SetNeedsAnimate",
                         TRACE_EVENT_SCOPE_THREAD);
  }
}

void ProxyMain::SetNeedsUpdateLayers() {
  DCHECK(IsMainThread());
  // If we are currently animating, make sure we also update the layers.
  if (current_pipeline_stage_ == ANIMATE_PIPELINE_STAGE) {
    final_pipeline_stage_ =
        std::max(final_pipeline_stage_, UPDATE_LAYERS_PIPELINE_STAGE);
    return;
  }
  if (SendCommitRequestToImplThreadIfNeeded(UPDATE_LAYERS_PIPELINE_STAGE)) {
    TRACE_EVENT_INSTANT0("cc", "ProxyMain::SetNeedsUpdateLayers",
                         TRACE_EVENT_SCOPE_THREAD);
  }
}

void ProxyMain::SetNeedsCommit() {
  DCHECK(IsMainThread());
  // If we are currently animating, make sure we don't skip the commit. Note
  // that requesting a commit during the layer update stage means we need to
  // schedule another full commit.
  if (current_pipeline_stage_ == ANIMATE_PIPELINE_STAGE) {
    final_pipeline_stage_ =
        std::max(final_pipeline_stage_, COMMIT_PIPELINE_STAGE);
    return;
  }
  if (SendCommitRequestToImplThreadIfNeeded(COMMIT_PIPELINE_STAGE)) {
    TRACE_EVENT_INSTANT0("cc", "ProxyMain::SetNeedsCommit",
                         TRACE_EVENT_SCOPE_THREAD);
  }
}

void ProxyMain::SetNeedsRedraw(const gfx::Rect& damage_rect) {
  TRACE_EVENT0("cc", "ProxyMain::SetNeedsRedraw");
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyImpl::SetNeedsRedrawOnImpl,
                     base::Unretained(proxy_impl_.get()), damage_rect));
}

void ProxyMain::SetNextCommitWaitsForActivation() {
  DCHECK(IsMainThread());
  commit_waits_for_activation_ = true;
}

void ProxyMain::NotifyInputThrottledUntilCommit() {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyImpl::SetInputThrottledUntilCommitOnImpl,
                                base::Unretained(proxy_impl_.get()), true));
}

void ProxyMain::SetDeferCommits(bool defer_commits) {
  DCHECK(IsMainThread());
  if (defer_commits_ == defer_commits)
    return;

  defer_commits_ = defer_commits;
  if (defer_commits_)
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ProxyMain::SetDeferCommits", this);
  else
    TRACE_EVENT_ASYNC_END0("cc", "ProxyMain::SetDeferCommits", this);

  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyImpl::SetDeferCommitsOnImpl,
                     base::Unretained(proxy_impl_.get()), defer_commits));
}

bool ProxyMain::CommitRequested() const {
  DCHECK(IsMainThread());
  // TODO(skyostil): Split this into something like CommitRequested() and
  // CommitInProgress().
  return current_pipeline_stage_ != NO_PIPELINE_STAGE ||
         max_requested_pipeline_stage_ >= COMMIT_PIPELINE_STAGE;
}

void ProxyMain::MainThreadHasStoppedFlinging() {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyImpl::MainThreadHasStoppedFlingingOnImpl,
                                base::Unretained(proxy_impl_.get())));
}

void ProxyMain::Start() {
  DCHECK(IsMainThread());
  DCHECK(layer_tree_host_->IsThreaded());

  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    CompletionEvent completion;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProxyMain::InitializeOnImplThread,
                                  base::Unretained(this), &completion));
    completion.Wait();
  }

  started_ = true;
}

void ProxyMain::Stop() {
  TRACE_EVENT0("cc", "ProxyMain::Stop");
  DCHECK(IsMainThread());
  DCHECK(started_);

  // Synchronously finishes pending GL operations and deletes the impl.
  // The two steps are done as separate post tasks, so that tasks posted
  // by the GL implementation due to the Finish can be executed by the
  // renderer before shutting it down.
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    CompletionEvent completion;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProxyImpl::FinishGLOnImpl,
                       base::Unretained(proxy_impl_.get()), &completion));
    completion.Wait();
  }
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    CompletionEvent completion;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProxyMain::DestroyProxyImplOnImplThread,
                                  base::Unretained(this), &completion));
    completion.Wait();
  }

  weak_factory_.InvalidateWeakPtrs();
  layer_tree_host_ = nullptr;
  started_ = false;
}

void ProxyMain::SetMutator(std::unique_ptr<LayerTreeMutator> mutator) {
  TRACE_EVENT0("compositor-worker", "ThreadProxy::SetMutator");
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyImpl::InitializeMutatorOnImpl,
                                base::Unretained(proxy_impl_.get()),
                                base::Passed(std::move(mutator))));
}

bool ProxyMain::SupportsImplScrolling() const {
  return true;
}

bool ProxyMain::MainFrameWillHappenForTesting() {
  DCHECK(IsMainThread());
  bool main_frame_will_happen = false;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    CompletionEvent completion;
    ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProxyImpl::MainFrameWillHappenOnImplForTesting,
                       base::Unretained(proxy_impl_.get()), &completion,
                       &main_frame_will_happen));
    completion.Wait();
  }
  return main_frame_will_happen;
}

void ProxyMain::ReleaseCompositorFrameSink() {
  DCHECK(IsMainThread());
  frame_sink_bound_weak_factory_.InvalidateWeakPtrs();
  DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
  CompletionEvent completion;
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyImpl::ReleaseCompositorFrameSinkOnImpl,
                     base::Unretained(proxy_impl_.get()), &completion));
  completion.Wait();
}

void ProxyMain::UpdateBrowserControlsState(BrowserControlsState constraints,
                                           BrowserControlsState current,
                                           bool animate) {
  DCHECK(IsMainThread());
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyImpl::UpdateBrowserControlsStateOnImpl,
                                base::Unretained(proxy_impl_.get()),
                                constraints, current, animate));
}

bool ProxyMain::SendCommitRequestToImplThreadIfNeeded(
    CommitPipelineStage required_stage) {
  DCHECK(IsMainThread());
  DCHECK_NE(NO_PIPELINE_STAGE, required_stage);
  bool already_posted = max_requested_pipeline_stage_ != NO_PIPELINE_STAGE;
  max_requested_pipeline_stage_ =
      std::max(max_requested_pipeline_stage_, required_stage);
  if (already_posted)
    return false;
  ImplThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ProxyImpl::SetNeedsCommitOnImpl,
                                base::Unretained(proxy_impl_.get())));
  return true;
}

bool ProxyMain::IsMainThread() const {
  return task_runner_provider_->IsMainThread();
}

bool ProxyMain::IsImplThread() const {
  return task_runner_provider_->IsImplThread();
}

base::SingleThreadTaskRunner* ProxyMain::ImplThreadTaskRunner() {
  return task_runner_provider_->ImplThreadTaskRunner();
}

}  // namespace cc
