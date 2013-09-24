// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/thread_proxy.h"

#include <string>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "cc/input/input_handler.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/quads/draw_quad.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/frame_rate_controller.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/blocking_task_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"

namespace {

// Measured in seconds.
const double kContextRecreationTickRate = 0.03;

// Measured in seconds.
const double kSmoothnessTakesPriorityExpirationDelay = 0.25;

const size_t kDurationHistorySize = 60;
const double kCommitAndActivationDurationEstimationPercentile = 50.0;
const double kDrawDurationEstimationPercentile = 100.0;
const int kDrawDurationEstimatePaddingInMicroseconds = 0;

}  // namespace

namespace cc {

struct ThreadProxy::ReadbackRequest {
  CompletionEvent completion;
  bool success;
  void* pixels;
  gfx::Rect rect;
};

struct ThreadProxy::CommitPendingRequest {
  CompletionEvent completion;
  bool commit_pending;
};

struct ThreadProxy::SchedulerStateRequest {
  CompletionEvent completion;
  scoped_ptr<base::Value> state;
};

scoped_ptr<Proxy> ThreadProxy::Create(
    LayerTreeHost* layer_tree_host,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner) {
  return make_scoped_ptr(
      new ThreadProxy(layer_tree_host, impl_task_runner)).PassAs<Proxy>();
}

ThreadProxy::ThreadProxy(
    LayerTreeHost* layer_tree_host,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner)
    : Proxy(impl_task_runner),
      animate_requested_(false),
      commit_requested_(false),
      commit_request_sent_to_impl_thread_(false),
      created_offscreen_context_provider_(false),
      layer_tree_host_(layer_tree_host),
      started_(false),
      textures_acquired_(true),
      in_composite_and_readback_(false),
      manage_tiles_pending_(false),
      commit_waits_for_activation_(false),
      inside_commit_(false),
      weak_factory_on_impl_thread_(this),
      weak_factory_(this),
      begin_frame_sent_to_main_thread_completion_event_on_impl_thread_(NULL),
      readback_request_on_impl_thread_(NULL),
      commit_completion_event_on_impl_thread_(NULL),
      completion_event_for_commit_held_on_tree_activation_(NULL),
      texture_acquisition_completion_event_on_impl_thread_(NULL),
      next_frame_is_newly_committed_frame_on_impl_thread_(false),
      throttle_frame_production_(
          layer_tree_host->settings().throttle_frame_production),
      begin_frame_scheduling_enabled_(
          layer_tree_host->settings().begin_frame_scheduling_enabled),
      using_synchronous_renderer_compositor_(
          layer_tree_host->settings().using_synchronous_renderer_compositor),
      inside_draw_(false),
      can_cancel_commit_(true),
      defer_commits_(false),
      input_throttled_until_commit_(false),
      renew_tree_priority_on_impl_thread_pending_(false),
      draw_duration_history_(kDurationHistorySize),
      begin_frame_to_commit_duration_history_(kDurationHistorySize),
      commit_to_activate_duration_history_(kDurationHistorySize) {
  TRACE_EVENT0("cc", "ThreadProxy::ThreadProxy");
  DCHECK(IsMainThread());
  DCHECK(layer_tree_host_);
}

ThreadProxy::~ThreadProxy() {
  TRACE_EVENT0("cc", "ThreadProxy::~ThreadProxy");
  DCHECK(IsMainThread());
  DCHECK(!started_);
}

bool ThreadProxy::CompositeAndReadback(void* pixels, gfx::Rect rect) {
  TRACE_EVENT0("cc", "ThreadProxy::CompositeAndReadback");
  DCHECK(IsMainThread());
  DCHECK(layer_tree_host_);

  if (defer_commits_) {
    TRACE_EVENT0("cc", "CompositeAndReadback_DeferCommit");
    return false;
  }

  if (!layer_tree_host_->InitializeOutputSurfaceIfNeeded()) {
    TRACE_EVENT0("cc", "CompositeAndReadback_EarlyOut_LR_Uninitialized");
    return false;
  }

  // Perform a synchronous commit with an associated readback.
  ReadbackRequest request;
  request.rect = rect;
  request.pixels = pixels;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    CompletionEvent begin_frame_sent_to_main_thread_completion;
    Proxy::ImplThreadTaskRunner()
        ->PostTask(FROM_HERE,
                   base::Bind(&ThreadProxy::ForceCommitForReadbackOnImplThread,
                              impl_thread_weak_ptr_,
                              &begin_frame_sent_to_main_thread_completion,
                              &request));
    begin_frame_sent_to_main_thread_completion.Wait();
  }

  in_composite_and_readback_ = true;
  // This is the forced commit.
  // Note: The Impl thread also queues a separate BeginFrameOnMainThread on the
  // main thread, which will be called after this CompositeAndReadback
  // completes, to replace the forced commit.
  BeginFrameOnMainThread(scoped_ptr<BeginFrameAndCommitState>());
  in_composite_and_readback_ = false;

  // Composite and readback requires a second commit to undo any changes
  // that it made.
  can_cancel_commit_ = false;

  request.completion.Wait();
  return request.success;
}

void ThreadProxy::ForceCommitForReadbackOnImplThread(
    CompletionEvent* begin_frame_sent_completion,
    ReadbackRequest* request) {
  TRACE_EVENT0("cc", "ThreadProxy::ForceCommitForReadbackOnImplThread");
  DCHECK(IsImplThread());
  DCHECK(!begin_frame_sent_to_main_thread_completion_event_on_impl_thread_);
  DCHECK(!readback_request_on_impl_thread_);

  if (!layer_tree_host_impl_) {
    begin_frame_sent_completion->Signal();
    request->success = false;
    request->completion.Signal();
    return;
  }

  readback_request_on_impl_thread_ = request;

  scheduler_on_impl_thread_->SetNeedsForcedCommitForReadback();
  if (scheduler_on_impl_thread_->CommitPending()) {
    begin_frame_sent_completion->Signal();
    return;
  }

  begin_frame_sent_to_main_thread_completion_event_on_impl_thread_ =
      begin_frame_sent_completion;
}

void ThreadProxy::FinishAllRendering() {
  DCHECK(Proxy::IsMainThread());
  DCHECK(!defer_commits_);

  // Make sure all GL drawing is finished on the impl thread.
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  CompletionEvent completion;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::FinishAllRenderingOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion));
  completion.Wait();
}

bool ThreadProxy::IsStarted() const {
  DCHECK(Proxy::IsMainThread());
  return started_;
}

void ThreadProxy::SetLayerTreeHostClientReady() {
  TRACE_EVENT0("cc", "ThreadProxy::SetLayerTreeHostClientReady");
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetLayerTreeHostClientReadyOnImplThread,
                 impl_thread_weak_ptr_));
}

void ThreadProxy::SetLayerTreeHostClientReadyOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::SetLayerTreeHostClientReadyOnImplThread");
  scheduler_on_impl_thread_->SetCanStart();
}

void ThreadProxy::SetVisible(bool visible) {
  TRACE_EVENT0("cc", "ThreadProxy::SetVisible");
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);

  CompletionEvent completion;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetVisibleOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion,
                 visible));
  completion.Wait();
}

void ThreadProxy::SetVisibleOnImplThread(CompletionEvent* completion,
                                         bool visible) {
  TRACE_EVENT0("cc", "ThreadProxy::SetVisibleOnImplThread");
  layer_tree_host_impl_->SetVisible(visible);
  scheduler_on_impl_thread_->SetVisible(visible);
  UpdateBackgroundAnimateTicking();
  completion->Signal();
}

void ThreadProxy::UpdateBackgroundAnimateTicking() {
  layer_tree_host_impl_->UpdateBackgroundAnimateTicking(
      !scheduler_on_impl_thread_->WillDrawIfNeeded() &&
      layer_tree_host_impl_->active_tree()->root_layer());
}

void ThreadProxy::DoCreateAndInitializeOutputSurface() {
  TRACE_EVENT0("cc", "ThreadProxy::DoCreateAndInitializeOutputSurface");
  DCHECK(IsMainThread());

  scoped_ptr<OutputSurface> output_surface = first_output_surface_.Pass();
  if (!output_surface)
    output_surface = layer_tree_host_->CreateOutputSurface();

  RendererCapabilities capabilities;
  bool success = !!output_surface;
  if (!success) {
    OnOutputSurfaceInitializeAttempted(false, capabilities);
    return;
  }

  scoped_refptr<ContextProvider> offscreen_context_provider;
  if (created_offscreen_context_provider_) {
    offscreen_context_provider = layer_tree_host_->client()->
        OffscreenContextProviderForCompositorThread();
    success = !!offscreen_context_provider.get();
    if (!success) {
      OnOutputSurfaceInitializeAttempted(false, capabilities);
      return;
    }
  }

  success = false;
  {
    // Make a blocking call to InitializeOutputSurfaceOnImplThread. The results
    // of that call are pushed into the success and capabilities local
    // variables.
    CompletionEvent completion;
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);

    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::InitializeOutputSurfaceOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion,
                   base::Passed(&output_surface),
                   offscreen_context_provider,
                   &success,
                   &capabilities));
    completion.Wait();
  }

  OnOutputSurfaceInitializeAttempted(success, capabilities);
}

void ThreadProxy::OnOutputSurfaceInitializeAttempted(
    bool success,
    const RendererCapabilities& capabilities) {
  DCHECK(IsMainThread());
  DCHECK(layer_tree_host_);

  if (success) {
    renderer_capabilities_main_thread_copy_ = capabilities;
  }

  LayerTreeHost::CreateResult result =
      layer_tree_host_->OnCreateAndInitializeOutputSurfaceAttempted(success);
  if (result == LayerTreeHost::CreateFailedButTryAgain) {
    if (!output_surface_creation_callback_.callback().is_null()) {
      Proxy::MainThreadTaskRunner()->PostTask(
          FROM_HERE, output_surface_creation_callback_.callback());
    }
  } else {
    output_surface_creation_callback_.Cancel();
  }
}

void ThreadProxy::SendCommitRequestToImplThreadIfNeeded() {
  DCHECK(IsMainThread());
  if (commit_request_sent_to_impl_thread_)
    return;
  commit_request_sent_to_impl_thread_ = true;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetNeedsCommitOnImplThread,
                 impl_thread_weak_ptr_));
}

const RendererCapabilities& ThreadProxy::GetRendererCapabilities() const {
  DCHECK(IsMainThread());
  DCHECK(!layer_tree_host_->output_surface_lost());
  return renderer_capabilities_main_thread_copy_;
}

void ThreadProxy::SetNeedsAnimate() {
  DCHECK(IsMainThread());
  if (animate_requested_)
    return;

  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsAnimate");
  animate_requested_ = true;
  can_cancel_commit_ = false;
  SendCommitRequestToImplThreadIfNeeded();
}

void ThreadProxy::SetNeedsUpdateLayers() {
  DCHECK(IsMainThread());
  SendCommitRequestToImplThreadIfNeeded();
}

void ThreadProxy::SetNeedsCommit() {
  DCHECK(IsMainThread());
  // Unconditionally set here to handle SetNeedsCommit calls during a commit.
  can_cancel_commit_ = false;

  if (commit_requested_)
    return;
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsCommit");
  commit_requested_ = true;

  SendCommitRequestToImplThreadIfNeeded();
}

void ThreadProxy::DidLoseOutputSurfaceOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::DidLoseOutputSurfaceOnImplThread");
  CheckOutputSurfaceStatusOnImplThread();
}

void ThreadProxy::CheckOutputSurfaceStatusOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::CheckOutputSurfaceStatusOnImplThread");
  if (!layer_tree_host_impl_->IsContextLost())
    return;
  if (cc::ContextProvider* offscreen_contexts =
          layer_tree_host_impl_->offscreen_context_provider())
    offscreen_contexts->VerifyContexts();
  scheduler_on_impl_thread_->DidLoseOutputSurface();
}

void ThreadProxy::OnSwapBuffersCompleteOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::OnSwapBuffersCompleteOnImplThread");
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::DidCompleteSwapBuffers, main_thread_weak_ptr_));
}

void ThreadProxy::SetNeedsBeginFrameOnImplThread(bool enable) {
  DCHECK(IsImplThread());
  TRACE_EVENT1("cc", "ThreadProxy::SetNeedsBeginFrameOnImplThread",
               "enable", enable);
  layer_tree_host_impl_->SetNeedsBeginFrame(enable);
  UpdateBackgroundAnimateTicking();
}

void ThreadProxy::BeginFrameOnImplThread(const BeginFrameArgs& args) {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::BeginFrameOnImplThread");

  // Sample the frame time now. This time will be used for updating animations
  // when we draw.
  layer_tree_host_impl_->CurrentFrameTimeTicks();

  scheduler_on_impl_thread_->BeginFrame(args);
}

void ThreadProxy::DidBeginFrameDeadlineOnImplThread() {
  layer_tree_host_impl_->ResetCurrentFrameTimeForNextFrame();
}

void ThreadProxy::OnCanDrawStateChanged(bool can_draw) {
  DCHECK(IsImplThread());
  TRACE_EVENT1(
      "cc", "ThreadProxy::OnCanDrawStateChanged", "can_draw", can_draw);
  scheduler_on_impl_thread_->SetCanDraw(can_draw);
  UpdateBackgroundAnimateTicking();
}

void ThreadProxy::NotifyReadyToActivate() {
  TRACE_EVENT0("cc", "ThreadProxy::NotifyReadyToActivate");
  scheduler_on_impl_thread_->NotifyReadyToActivate();
}

void ThreadProxy::SetNeedsCommitOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsCommitOnImplThread");
  scheduler_on_impl_thread_->SetNeedsCommit();
}

void ThreadProxy::PostAnimationEventsToMainThreadOnImplThread(
    scoped_ptr<AnimationEventsVector> events,
    base::Time wall_clock_time) {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc",
               "ThreadProxy::PostAnimationEventsToMainThreadOnImplThread");
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetAnimationEvents,
                 main_thread_weak_ptr_,
                 base::Passed(&events),
                 wall_clock_time));
}

bool ThreadProxy::ReduceContentsTextureMemoryOnImplThread(size_t limit_bytes,
                                                          int priority_cutoff) {
  DCHECK(IsImplThread());

  if (!layer_tree_host_->contents_texture_manager())
    return false;

  bool reduce_result = layer_tree_host_->contents_texture_manager()->
      ReduceMemoryOnImplThread(limit_bytes,
                               priority_cutoff,
                               layer_tree_host_impl_->resource_provider());
  if (!reduce_result)
    return false;

  // The texture upload queue may reference textures that were just purged,
  // clear them from the queue.
  if (current_resource_update_controller_on_impl_thread_) {
    current_resource_update_controller_on_impl_thread_->
        DiscardUploadsToEvictedResources();
  }
  return true;
}

void ThreadProxy::ReduceWastedContentsTextureMemoryOnImplThread() {
  DCHECK(IsImplThread());

  if (!layer_tree_host_->contents_texture_manager())
    return;

  layer_tree_host_->contents_texture_manager()->ReduceWastedMemoryOnImplThread(
      layer_tree_host_impl_->resource_provider());
}

void ThreadProxy::SendManagedMemoryStats() {
  DCHECK(IsImplThread());
  if (!layer_tree_host_impl_)
    return;
  if (!layer_tree_host_->contents_texture_manager())
    return;

  // If we are using impl-side painting, then SendManagedMemoryStats is called
  // directly after the tile manager's manage function, and doesn't need to
  // interact with main thread's layer tree.
  if (layer_tree_host_->settings().impl_side_painting)
    return;

  layer_tree_host_impl_->SendManagedMemoryStats(
      layer_tree_host_->contents_texture_manager()->MemoryVisibleBytes(),
      layer_tree_host_->contents_texture_manager()->
          MemoryVisibleAndNearbyBytes(),
      layer_tree_host_->contents_texture_manager()->MemoryUseBytes());
}

bool ThreadProxy::IsInsideDraw() { return inside_draw_; }

void ThreadProxy::SetNeedsRedraw(gfx::Rect damage_rect) {
  DCHECK(IsMainThread());
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsRedraw");
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetNeedsRedrawRectOnImplThread,
                 impl_thread_weak_ptr_,
                 damage_rect));
}

void ThreadProxy::SetNextCommitWaitsForActivation() {
  DCHECK(IsMainThread());
  DCHECK(!inside_commit_);
  commit_waits_for_activation_ = true;
}

void ThreadProxy::SetDeferCommits(bool defer_commits) {
  DCHECK(IsMainThread());
  DCHECK_NE(defer_commits_, defer_commits);
  defer_commits_ = defer_commits;

  if (defer_commits_)
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ThreadProxy::SetDeferCommits", this);
  else
    TRACE_EVENT_ASYNC_END0("cc", "ThreadProxy::SetDeferCommits", this);

  if (!defer_commits_ && pending_deferred_commit_)
    Proxy::MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::BeginFrameOnMainThread,
                   main_thread_weak_ptr_,
                   base::Passed(&pending_deferred_commit_)));
}

bool ThreadProxy::CommitRequested() const {
  DCHECK(IsMainThread());
  return commit_requested_;
}

void ThreadProxy::SetNeedsRedrawOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsRedrawOnImplThread");
  scheduler_on_impl_thread_->SetNeedsRedraw();
}

void ThreadProxy::SetNeedsManageTilesOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsManageTilesOnImplThread");
  scheduler_on_impl_thread_->SetNeedsManageTiles();
}

void ThreadProxy::SetNeedsRedrawRectOnImplThread(gfx::Rect damage_rect) {
  DCHECK(IsImplThread());
  layer_tree_host_impl_->SetViewportDamage(damage_rect);
  SetNeedsRedrawOnImplThread();
}

void ThreadProxy::SetSwapUsedIncompleteTileOnImplThread(
    bool used_incomplete_tile) {
  DCHECK(IsImplThread());
  if (used_incomplete_tile) {
    TRACE_EVENT_INSTANT0(
        "cc",
        "ThreadProxy::SetSwapUsedIncompleteTileOnImplThread",
        TRACE_EVENT_SCOPE_THREAD);
  }
  scheduler_on_impl_thread_->SetSwapUsedIncompleteTile(
    used_incomplete_tile);
}

void ThreadProxy::DidInitializeVisibleTileOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::DidInitializeVisibleTileOnImplThread");
  scheduler_on_impl_thread_->SetNeedsRedraw();
}

void ThreadProxy::MainThreadHasStoppedFlinging() {
  DCHECK(IsMainThread());
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::MainThreadHasStoppedFlingingOnImplThread,
                 impl_thread_weak_ptr_));
}

void ThreadProxy::MainThreadHasStoppedFlingingOnImplThread() {
  DCHECK(IsImplThread());
  layer_tree_host_impl_->MainThreadHasStoppedFlinging();
}

void ThreadProxy::NotifyInputThrottledUntilCommit() {
  DCHECK(IsMainThread());
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetInputThrottledUntilCommitOnImplThread,
                 impl_thread_weak_ptr_,
                 true));
}

void ThreadProxy::SetInputThrottledUntilCommitOnImplThread(
    bool is_throttled) {
  DCHECK(IsImplThread());
  if (is_throttled == input_throttled_until_commit_)
    return;
  input_throttled_until_commit_ = is_throttled;
  RenewTreePriority();
}

void ThreadProxy::Start(scoped_ptr<OutputSurface> first_output_surface) {
  DCHECK(IsMainThread());
  DCHECK(Proxy::HasImplThread());
  DCHECK(first_output_surface);

  // Create LayerTreeHostImpl.
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  CompletionEvent completion;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::InitializeImplOnImplThread,
                 base::Unretained(this),
                 &completion));
  completion.Wait();

  main_thread_weak_ptr_ = weak_factory_.GetWeakPtr();
  first_output_surface_ = first_output_surface.Pass();

  started_ = true;
}

void ThreadProxy::Stop() {
  TRACE_EVENT0("cc", "ThreadProxy::Stop");
  DCHECK(IsMainThread());
  DCHECK(started_);

  // Synchronously finishes pending GL operations and deletes the impl.
  // The two steps are done as separate post tasks, so that tasks posted
  // by the GL implementation due to the Finish can be executed by the
  // renderer before shutting it down.
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);

    CompletionEvent completion;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::FinishGLOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion));
    completion.Wait();
  }
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);

    CompletionEvent completion;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::LayerTreeHostClosedOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion));
    completion.Wait();
  }

  weak_factory_.InvalidateWeakPtrs();

  DCHECK(!layer_tree_host_impl_.get());  // verify that the impl deleted.
  layer_tree_host_ = NULL;
  started_ = false;
}

void ThreadProxy::ForceSerializeOnSwapBuffers() {
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  CompletionEvent completion;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::ForceSerializeOnSwapBuffersOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion));
  completion.Wait();
}

void ThreadProxy::ForceSerializeOnSwapBuffersOnImplThread(
    CompletionEvent* completion) {
  if (layer_tree_host_impl_->renderer())
    layer_tree_host_impl_->renderer()->DoNoOp();
  completion->Signal();
}

void ThreadProxy::FinishAllRenderingOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::FinishAllRenderingOnImplThread");
  DCHECK(IsImplThread());
  layer_tree_host_impl_->FinishAllRendering();
  completion->Signal();
}

void ThreadProxy::ScheduledActionSendBeginFrameToMainThread() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionSendBeginFrameToMainThread");
  scoped_ptr<BeginFrameAndCommitState> begin_frame_state(
      new BeginFrameAndCommitState);
  begin_frame_state->monotonic_frame_begin_time =
      layer_tree_host_impl_->CurrentPhysicalTimeTicks();
  begin_frame_state->scroll_info =
      layer_tree_host_impl_->ProcessScrollDeltas();

  if (!layer_tree_host_impl_->settings().impl_side_painting) {
    DCHECK_GT(layer_tree_host_impl_->memory_allocation_limit_bytes(), 0u);
  }
  begin_frame_state->memory_allocation_limit_bytes =
      layer_tree_host_impl_->memory_allocation_limit_bytes();
  begin_frame_state->memory_allocation_priority_cutoff =
      layer_tree_host_impl_->memory_allocation_priority_cutoff();
  begin_frame_state->evicted_ui_resources =
      layer_tree_host_impl_->EvictedUIResourcesExist();
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::BeginFrameOnMainThread,
                 main_thread_weak_ptr_,
                 base::Passed(&begin_frame_state)));

  if (begin_frame_sent_to_main_thread_completion_event_on_impl_thread_) {
    begin_frame_sent_to_main_thread_completion_event_on_impl_thread_->Signal();
    begin_frame_sent_to_main_thread_completion_event_on_impl_thread_ = NULL;
  }
  begin_frame_sent_to_main_thread_time_ = base::TimeTicks::HighResNow();
}

void ThreadProxy::BeginFrameOnMainThread(
    scoped_ptr<BeginFrameAndCommitState> begin_frame_state) {
  TRACE_EVENT0("cc", "ThreadProxy::BeginFrameOnMainThread");
  DCHECK(IsMainThread());

  if (!layer_tree_host_)
    return;

  if (defer_commits_) {
    pending_deferred_commit_ = begin_frame_state.Pass();
    layer_tree_host_->DidDeferCommit();
    TRACE_EVENT0("cc", "EarlyOut_DeferCommits");
    return;
  }

  // Do not notify the impl thread of commit requests that occur during
  // the apply/animate/layout part of the BeginFrameAndCommit process since
  // those commit requests will get painted immediately. Once we have done
  // the paint, commit_requested_ will be set to false to allow new commit
  // requests to be scheduled.
  commit_requested_ = true;
  commit_request_sent_to_impl_thread_ = true;

  // On the other hand, the AnimationRequested flag needs to be cleared
  // here so that any animation requests generated by the apply or animate
  // callbacks will trigger another frame.
  animate_requested_ = false;

  if (!in_composite_and_readback_ && !layer_tree_host_->visible()) {
    commit_requested_ = false;
    commit_request_sent_to_impl_thread_ = false;

    TRACE_EVENT0("cc", "EarlyOut_NotVisible");
    bool did_handle = false;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::BeginFrameAbortedByMainThreadOnImplThread,
                   impl_thread_weak_ptr_,
                   did_handle));
    return;
  }

  if (begin_frame_state)
    layer_tree_host_->ApplyScrollAndScale(*begin_frame_state->scroll_info);

  layer_tree_host_->WillBeginFrame();

  if (begin_frame_state) {
    layer_tree_host_->UpdateClientAnimations(
        begin_frame_state->monotonic_frame_begin_time);
    layer_tree_host_->AnimateLayers(
        begin_frame_state->monotonic_frame_begin_time);
  }

  // Unlink any backings that the impl thread has evicted, so that we know to
  // re-paint them in UpdateLayers.
  if (layer_tree_host_->contents_texture_manager()) {
    layer_tree_host_->contents_texture_manager()->
        UnlinkAndClearEvictedBackings();

    if (begin_frame_state) {
      layer_tree_host_->contents_texture_manager()->SetMaxMemoryLimitBytes(
          begin_frame_state->memory_allocation_limit_bytes);
      layer_tree_host_->contents_texture_manager()->SetExternalPriorityCutoff(
          begin_frame_state->memory_allocation_priority_cutoff);
    }
  }

  // Recreate all UI resources if there were evicted UI resources when the impl
  // thread initiated the commit.
  bool evicted_ui_resources =
      begin_frame_state ? begin_frame_state->evicted_ui_resources : false;
  if (evicted_ui_resources)
      layer_tree_host_->RecreateUIResources();

  layer_tree_host_->Layout();

  // Clear the commit flag after updating animations and layout here --- objects
  // that only layout when painted will trigger another SetNeedsCommit inside
  // UpdateLayers.
  commit_requested_ = false;
  commit_request_sent_to_impl_thread_ = false;
  bool can_cancel_this_commit =
      can_cancel_commit_ &&
      !in_composite_and_readback_ &&
      !evicted_ui_resources;
  can_cancel_commit_ = true;

  scoped_ptr<ResourceUpdateQueue> queue =
      make_scoped_ptr(new ResourceUpdateQueue);

  bool updated = layer_tree_host_->UpdateLayers(queue.get());

  // Once single buffered layers are committed, they cannot be modified until
  // they are drawn by the impl thread.
  textures_acquired_ = false;

  layer_tree_host_->WillCommit();

  if (!updated && can_cancel_this_commit) {
    TRACE_EVENT0("cc", "EarlyOut_NoUpdates");
    bool did_handle = true;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::BeginFrameAbortedByMainThreadOnImplThread,
                   impl_thread_weak_ptr_,
                   did_handle));

    // Although the commit is internally aborted, this is because it has been
    // detected to be a no-op.  From the perspective of an embedder, this commit
    // went through, and input should no longer be throttled, etc.
    layer_tree_host_->CommitComplete();
    layer_tree_host_->DidBeginFrame();
    return;
  }

  // Before calling animate, we set animate_requested_ to false. If it is true
  // now, it means SetNeedAnimate was called again, but during a state when
  // commit_request_sent_to_impl_thread_ = true. We need to force that call to
  // happen again now so that the commit request is sent to the impl thread.
  if (animate_requested_) {
    // Forces SetNeedsAnimate to consider posting a commit task.
    animate_requested_ = false;
    SetNeedsAnimate();
  }

  scoped_refptr<cc::ContextProvider> offscreen_context_provider;
  if (renderer_capabilities_main_thread_copy_.using_offscreen_context3d &&
      layer_tree_host_->needs_offscreen_context()) {
    offscreen_context_provider = layer_tree_host_->client()->
        OffscreenContextProviderForCompositorThread();
    if (offscreen_context_provider.get())
      created_offscreen_context_provider_ = true;
  }

  // Notify the impl thread that the main thread is ready to commit. This will
  // begin the commit process, which is blocking from the main thread's
  // point of view, but asynchronously performed on the impl thread,
  // coordinated by the Scheduler.
  {
    TRACE_EVENT0("cc", "ThreadProxy::BeginFrameOnMainThread::commit");

    DebugScopedSetMainThreadBlocked main_thread_blocked(this);

    // This CapturePostTasks should be destroyed before CommitComplete() is
    // called since that goes out to the embedder, and we want the embedder
    // to receive its callbacks before that.
    BlockingTaskRunner::CapturePostTasks blocked;

    RenderingStatsInstrumentation* stats_instrumentation =
        layer_tree_host_->rendering_stats_instrumentation();
    base::TimeTicks start_time = stats_instrumentation->StartRecording();

    CompletionEvent completion;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::StartCommitOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion,
                   queue.release(),
                   offscreen_context_provider));
    completion.Wait();

    base::TimeDelta duration = stats_instrumentation->EndRecording(start_time);
    stats_instrumentation->AddCommit(duration);
    stats_instrumentation->IssueTraceEventForMainThreadStats();
    stats_instrumentation->AccumulateAndClearMainThreadStats();
  }

  layer_tree_host_->CommitComplete();
  layer_tree_host_->DidBeginFrame();
}

void ThreadProxy::StartCommitOnImplThread(
    CompletionEvent* completion,
    ResourceUpdateQueue* raw_queue,
    scoped_refptr<cc::ContextProvider> offscreen_context_provider) {
  scoped_ptr<ResourceUpdateQueue> queue(raw_queue);

  TRACE_EVENT0("cc", "ThreadProxy::StartCommitOnImplThread");
  DCHECK(!commit_completion_event_on_impl_thread_);
  DCHECK(IsImplThread() && IsMainThreadBlocked());
  DCHECK(scheduler_on_impl_thread_);
  DCHECK(scheduler_on_impl_thread_->CommitPending());

  if (!layer_tree_host_impl_) {
    TRACE_EVENT0("cc", "EarlyOut_NoLayerTree");
    completion->Signal();
    return;
  }

  if (offscreen_context_provider.get())
    offscreen_context_provider->BindToCurrentThread();
  layer_tree_host_impl_->SetOffscreenContextProvider(
      offscreen_context_provider);

  if (layer_tree_host_->contents_texture_manager()) {
    if (layer_tree_host_->contents_texture_manager()->
            LinkedEvictedBackingsExist()) {
      // Clear any uploads we were making to textures linked to evicted
      // resources
      queue->ClearUploadsToEvictedResources();
      // Some textures in the layer tree are invalid. Kick off another commit
      // to fill them again.
      SetNeedsCommitOnImplThread();
    }

    layer_tree_host_->contents_texture_manager()->
        PushTexturePrioritiesToBackings();
  }

  commit_completion_event_on_impl_thread_ = completion;
  current_resource_update_controller_on_impl_thread_ =
      ResourceUpdateController::Create(
          this,
          Proxy::ImplThreadTaskRunner(),
          queue.Pass(),
          layer_tree_host_impl_->resource_provider());
  current_resource_update_controller_on_impl_thread_->PerformMoreUpdates(
      scheduler_on_impl_thread_->AnticipatedDrawTime());
}

void ThreadProxy::BeginFrameAbortedByMainThreadOnImplThread(bool did_handle) {
  TRACE_EVENT0("cc", "ThreadProxy::BeginFrameAbortedByMainThreadOnImplThread");
  DCHECK(IsImplThread());
  DCHECK(scheduler_on_impl_thread_);
  DCHECK(scheduler_on_impl_thread_->CommitPending());
  DCHECK(!layer_tree_host_impl_->pending_tree());

  // If the begin frame data was handled, then scroll and scale set was applied
  // by the main thread, so the active tree needs to be updated as if these sent
  // values were applied and committed.
  if (did_handle) {
    layer_tree_host_impl_->active_tree()
        ->ApplySentScrollAndScaleDeltasFromAbortedCommit();
    layer_tree_host_impl_->active_tree()->ResetContentsTexturesPurged();
    SetInputThrottledUntilCommitOnImplThread(false);
  }
  scheduler_on_impl_thread_->BeginFrameAbortedByMainThread(did_handle);
}

void ThreadProxy::ScheduledActionCommit() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionCommit");
  DCHECK(IsImplThread());
  DCHECK(commit_completion_event_on_impl_thread_);
  DCHECK(current_resource_update_controller_on_impl_thread_);

  // Complete all remaining texture updates.
  current_resource_update_controller_on_impl_thread_->Finalize();
  current_resource_update_controller_on_impl_thread_.reset();

  inside_commit_ = true;
  layer_tree_host_impl_->BeginCommit();
  layer_tree_host_->BeginCommitOnImplThread(layer_tree_host_impl_.get());
  layer_tree_host_->FinishCommitOnImplThread(layer_tree_host_impl_.get());
  layer_tree_host_impl_->CommitComplete();
  inside_commit_ = false;

  SetInputThrottledUntilCommitOnImplThread(false);

  UpdateBackgroundAnimateTicking();

  next_frame_is_newly_committed_frame_on_impl_thread_ = true;

  if (layer_tree_host_->settings().impl_side_painting &&
      commit_waits_for_activation_) {
    // For some layer types in impl-side painting, the commit is held until
    // the pending tree is activated.  It's also possible that the
    // pending tree has already activated if there was no work to be done.
    TRACE_EVENT_INSTANT0("cc", "HoldCommit", TRACE_EVENT_SCOPE_THREAD);
    completion_event_for_commit_held_on_tree_activation_ =
        commit_completion_event_on_impl_thread_;
    commit_completion_event_on_impl_thread_ = NULL;
  } else {
    commit_completion_event_on_impl_thread_->Signal();
    commit_completion_event_on_impl_thread_ = NULL;
  }

  commit_waits_for_activation_ = false;

  commit_complete_time_ = base::TimeTicks::HighResNow();
  begin_frame_to_commit_duration_history_.InsertSample(
      commit_complete_time_ - begin_frame_sent_to_main_thread_time_);

  // SetVisible kicks off the next scheduler action, so this must be last.
  scheduler_on_impl_thread_->SetVisible(layer_tree_host_impl_->visible());
}

void ThreadProxy::ScheduledActionUpdateVisibleTiles() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionUpdateVisibleTiles");
  layer_tree_host_impl_->UpdateVisibleTiles();
}

void ThreadProxy::ScheduledActionActivatePendingTree() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionActivatePendingTree");
  layer_tree_host_impl_->ActivatePendingTree();
}

void ThreadProxy::ScheduledActionBeginOutputSurfaceCreation() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionBeginOutputSurfaceCreation");
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::CreateAndInitializeOutputSurface,
                 main_thread_weak_ptr_));
}

DrawSwapReadbackResult ThreadProxy::DrawSwapReadbackInternal(
    bool forced_draw,
    bool swap_requested,
    bool readback_requested) {
  DrawSwapReadbackResult result;
  result.did_draw = false;
  result.did_swap = false;
  result.did_readback = false;
  DCHECK(IsImplThread());
  DCHECK(layer_tree_host_impl_.get());
  if (!layer_tree_host_impl_)
    return result;

  DCHECK(layer_tree_host_impl_->renderer());
  if (!layer_tree_host_impl_->renderer())
    return result;

  base::TimeTicks start_time = base::TimeTicks::HighResNow();
  base::TimeDelta draw_duration_estimate = DrawDurationEstimate();
  base::AutoReset<bool> mark_inside(&inside_draw_, true);

  // Advance our animations.
  base::TimeTicks monotonic_time =
      layer_tree_host_impl_->CurrentFrameTimeTicks();
  base::Time wall_clock_time = layer_tree_host_impl_->CurrentFrameTime();
  layer_tree_host_impl_->Animate(monotonic_time, wall_clock_time);

  // This method is called on a forced draw, regardless of whether we are able
  // to produce a frame, as the calling site on main thread is blocked until its
  // request completes, and we signal completion here. If CanDraw() is false, we
  // will indicate success=false to the caller, but we must still signal
  // completion to avoid deadlock.

  // We guard PrepareToDraw() with CanDraw() because it always returns a valid
  // frame, so can only be used when such a frame is possible. Since
  // DrawLayers() depends on the result of PrepareToDraw(), it is guarded on
  // CanDraw() as well.

  bool drawing_for_readback =
      readback_requested && !!readback_request_on_impl_thread_;
  bool can_do_readback = layer_tree_host_impl_->renderer()->CanReadPixels();

  LayerTreeHostImpl::FrameData frame;
  bool draw_frame = false;

  if (layer_tree_host_impl_->CanDraw() &&
      (!drawing_for_readback || can_do_readback)) {
    // If it is for a readback, make sure we draw the portion being read back.
    gfx::Rect readback_rect;
    if (drawing_for_readback)
      readback_rect = readback_request_on_impl_thread_->rect;

    if (layer_tree_host_impl_->PrepareToDraw(&frame, readback_rect) ||
        forced_draw)
      draw_frame = true;
  }

  if (draw_frame) {
    layer_tree_host_impl_->DrawLayers(
        &frame,
        scheduler_on_impl_thread_->LastBeginFrameOnImplThreadTime());
    result.did_draw = true;
  }
  layer_tree_host_impl_->DidDrawAllLayers(frame);

  bool start_ready_animations = draw_frame;
  layer_tree_host_impl_->UpdateAnimationState(start_ready_animations);

  // Check for a pending CompositeAndReadback.
  if (drawing_for_readback) {
    DCHECK(!swap_requested);
    result.did_readback = false;
    if (draw_frame && !layer_tree_host_impl_->IsContextLost()) {
      layer_tree_host_impl_->Readback(readback_request_on_impl_thread_->pixels,
                                      readback_request_on_impl_thread_->rect);
      result.did_readback = true;
    }
    readback_request_on_impl_thread_->success = result.did_readback;
    readback_request_on_impl_thread_->completion.Signal();
    readback_request_on_impl_thread_ = NULL;
  } else if (draw_frame) {
    DCHECK(swap_requested);
    result.did_swap = layer_tree_host_impl_->SwapBuffers(frame);

    // We don't know if we have incomplete tiles if we didn't actually swap.
    if (result.did_swap) {
      DCHECK(!frame.has_no_damage);
      SetSwapUsedIncompleteTileOnImplThread(frame.contains_incomplete_tile);
    }
  }

  // Tell the main thread that the the newly-commited frame was drawn.
  if (next_frame_is_newly_committed_frame_on_impl_thread_) {
    next_frame_is_newly_committed_frame_on_impl_thread_ = false;
    Proxy::MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::DidCommitAndDrawFrame, main_thread_weak_ptr_));
  }

  if (draw_frame) {
    CheckOutputSurfaceStatusOnImplThread();

    base::TimeDelta draw_duration = base::TimeTicks::HighResNow() - start_time;
    draw_duration_history_.InsertSample(draw_duration);
    base::TimeDelta draw_duration_overestimate;
    base::TimeDelta draw_duration_underestimate;
    if (draw_duration > draw_duration_estimate)
      draw_duration_underestimate = draw_duration - draw_duration_estimate;
    else
      draw_duration_overestimate = draw_duration_estimate - draw_duration;
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDuration",
                               draw_duration,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(100),
                               50);
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDurationUnderestimate",
                               draw_duration_underestimate,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(100),
                               50);
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDurationOverestimate",
                               draw_duration_overestimate,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(100),
                               50);
  }

  return result;
}

void ThreadProxy::AcquireLayerTextures() {
  // Called when the main thread needs to modify a layer texture that is used
  // directly by the compositor.
  // This method will block until the next compositor draw if there is a
  // previously committed frame that is still undrawn. This is necessary to
  // ensure that the main thread does not monopolize access to the textures.
  DCHECK(IsMainThread());

  if (textures_acquired_)
    return;

  TRACE_EVENT0("cc", "ThreadProxy::AcquireLayerTextures");
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  CompletionEvent completion;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::AcquireLayerTexturesForMainThreadOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion));
  // Block until it is safe to write to layer textures from the main thread.
  completion.Wait();

  textures_acquired_ = true;
  can_cancel_commit_ = false;
}

void ThreadProxy::AcquireLayerTexturesForMainThreadOnImplThread(
    CompletionEvent* completion) {
  DCHECK(IsImplThread());
  DCHECK(!texture_acquisition_completion_event_on_impl_thread_);

  texture_acquisition_completion_event_on_impl_thread_ = completion;
  scheduler_on_impl_thread_->SetMainThreadNeedsLayerTextures();
}

void ThreadProxy::ScheduledActionAcquireLayerTexturesForMainThread() {
  DCHECK(texture_acquisition_completion_event_on_impl_thread_);
  texture_acquisition_completion_event_on_impl_thread_->Signal();
  texture_acquisition_completion_event_on_impl_thread_ = NULL;
}

void ThreadProxy::ScheduledActionManageTiles() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionManageTiles");
  DCHECK(layer_tree_host_impl_->settings().impl_side_painting);
  layer_tree_host_impl_->ManageTiles();
}

DrawSwapReadbackResult ThreadProxy::ScheduledActionDrawAndSwapIfPossible() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionDrawAndSwap");
  bool forced_draw = false;
  bool swap_requested = true;
  bool readback_requested = false;
  return DrawSwapReadbackInternal(
      forced_draw, swap_requested, readback_requested);
}

DrawSwapReadbackResult ThreadProxy::ScheduledActionDrawAndSwapForced() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionDrawAndSwapForced");
  bool forced_draw = true;
  bool swap_requested = true;
  bool readback_requested = false;
  return DrawSwapReadbackInternal(
      forced_draw, swap_requested, readback_requested);
}

DrawSwapReadbackResult ThreadProxy::ScheduledActionDrawAndReadback() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionDrawAndReadback");
  bool forced_draw = true;
  bool swap_requested = false;
  bool readback_requested = true;
  return DrawSwapReadbackInternal(
      forced_draw, swap_requested, readback_requested);
}

void ThreadProxy::DidAnticipatedDrawTimeChange(base::TimeTicks time) {
  if (current_resource_update_controller_on_impl_thread_)
    current_resource_update_controller_on_impl_thread_->PerformMoreUpdates(
        time);
}

base::TimeDelta ThreadProxy::DrawDurationEstimate() {
  base::TimeDelta historical_estimate =
      draw_duration_history_.Percentile(kDrawDurationEstimationPercentile);
  base::TimeDelta padding = base::TimeDelta::FromMicroseconds(
      kDrawDurationEstimatePaddingInMicroseconds);
  return historical_estimate + padding;
}

base::TimeDelta ThreadProxy::BeginFrameToCommitDurationEstimate() {
  return begin_frame_to_commit_duration_history_.Percentile(
      kCommitAndActivationDurationEstimationPercentile);
}

base::TimeDelta ThreadProxy::CommitToActivateDurationEstimate() {
  return commit_to_activate_duration_history_.Percentile(
      kCommitAndActivationDurationEstimationPercentile);
}

void ThreadProxy::PostBeginFrameDeadline(const base::Closure& closure,
                                         base::TimeTicks deadline) {
  base::TimeDelta delta = deadline - base::TimeTicks::Now();
  if (delta <= base::TimeDelta())
    delta = base::TimeDelta();
  Proxy::ImplThreadTaskRunner()->PostDelayedTask(FROM_HERE, closure, delta);
}

void ThreadProxy::ReadyToFinalizeTextureUpdates() {
  DCHECK(IsImplThread());
  scheduler_on_impl_thread_->FinishCommit();
}

void ThreadProxy::DidCommitAndDrawFrame() {
  DCHECK(IsMainThread());
  if (!layer_tree_host_)
    return;
  layer_tree_host_->DidCommitAndDrawFrame();
}

void ThreadProxy::DidCompleteSwapBuffers() {
  DCHECK(IsMainThread());
  if (!layer_tree_host_)
    return;
  layer_tree_host_->DidCompleteSwapBuffers();
}

void ThreadProxy::SetAnimationEvents(scoped_ptr<AnimationEventsVector> events,
                                     base::Time wall_clock_time) {
  TRACE_EVENT0("cc", "ThreadProxy::SetAnimationEvents");
  DCHECK(IsMainThread());
  if (!layer_tree_host_)
    return;
  layer_tree_host_->SetAnimationEvents(events.Pass(), wall_clock_time);
}

void ThreadProxy::CreateAndInitializeOutputSurface() {
  TRACE_EVENT0("cc", "ThreadProxy::CreateAndInitializeOutputSurface");
  DCHECK(IsMainThread());

  // Check that output surface has not been recreated by CompositeAndReadback
  // after this task is posted but before it is run.
  bool has_initialized_output_surface_on_impl_thread = true;
  {
    CompletionEvent completion;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::HasInitializedOutputSurfaceOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion,
                   &has_initialized_output_surface_on_impl_thread));
    completion.Wait();
  }
  if (has_initialized_output_surface_on_impl_thread)
    return;

  layer_tree_host_->DidLoseOutputSurface();
  output_surface_creation_callback_.Reset(base::Bind(
      &ThreadProxy::DoCreateAndInitializeOutputSurface,
      base::Unretained(this)));
  output_surface_creation_callback_.callback().Run();
}

void ThreadProxy::HasInitializedOutputSurfaceOnImplThread(
    CompletionEvent* completion,
    bool* has_initialized_output_surface) {
  DCHECK(IsImplThread());
  *has_initialized_output_surface =
      scheduler_on_impl_thread_->HasInitializedOutputSurface();
  completion->Signal();
}

void ThreadProxy::InitializeImplOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::InitializeImplOnImplThread");
  DCHECK(IsImplThread());
  layer_tree_host_impl_ = layer_tree_host_->CreateLayerTreeHostImpl(this);
  const LayerTreeSettings& settings = layer_tree_host_->settings();
  SchedulerSettings scheduler_settings;
  scheduler_settings.deadline_scheduling_enabled =
      settings.deadline_scheduling_enabled;
  scheduler_settings.impl_side_painting = settings.impl_side_painting;
  scheduler_settings.timeout_and_draw_when_animation_checkerboards =
      settings.timeout_and_draw_when_animation_checkerboards;
  scheduler_settings.maximum_number_of_failed_draws_before_draw_is_forced_ =
      settings.maximum_number_of_failed_draws_before_draw_is_forced_;
  scheduler_settings.using_synchronous_renderer_compositor =
      settings.using_synchronous_renderer_compositor;
  scheduler_settings.throttle_frame_production =
      settings.throttle_frame_production;
  scheduler_on_impl_thread_ = Scheduler::Create(this, scheduler_settings);
  scheduler_on_impl_thread_->SetVisible(layer_tree_host_impl_->visible());

  impl_thread_weak_ptr_ = weak_factory_on_impl_thread_.GetWeakPtr();
  completion->Signal();
}

void ThreadProxy::InitializeOutputSurfaceOnImplThread(
    CompletionEvent* completion,
    scoped_ptr<OutputSurface> output_surface,
    scoped_refptr<ContextProvider> offscreen_context_provider,
    bool* success,
    RendererCapabilities* capabilities) {
  TRACE_EVENT0("cc", "ThreadProxy::InitializeOutputSurfaceOnImplThread");
  DCHECK(IsImplThread());
  DCHECK(IsMainThreadBlocked());
  DCHECK(success);
  DCHECK(capabilities);

  layer_tree_host_->DeleteContentsTexturesOnImplThread(
      layer_tree_host_impl_->resource_provider());

  *success = layer_tree_host_impl_->InitializeRenderer(output_surface.Pass());

  if (*success) {
    *capabilities = layer_tree_host_impl_->GetRendererCapabilities();
    scheduler_on_impl_thread_->DidCreateAndInitializeOutputSurface();
  } else if (offscreen_context_provider.get()) {
    if (offscreen_context_provider->BindToCurrentThread())
      offscreen_context_provider->VerifyContexts();
    offscreen_context_provider = NULL;
  }

  layer_tree_host_impl_->SetOffscreenContextProvider(
      offscreen_context_provider);

  completion->Signal();
}

void ThreadProxy::FinishGLOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::FinishGLOnImplThread");
  DCHECK(IsImplThread());
  if (layer_tree_host_impl_->resource_provider())
    layer_tree_host_impl_->resource_provider()->Finish();
  completion->Signal();
}

void ThreadProxy::LayerTreeHostClosedOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::LayerTreeHostClosedOnImplThread");
  DCHECK(IsImplThread());
  layer_tree_host_->DeleteContentsTexturesOnImplThread(
      layer_tree_host_impl_->resource_provider());
  layer_tree_host_impl_->SetNeedsBeginFrame(false);
  scheduler_on_impl_thread_.reset();
  layer_tree_host_impl_.reset();
  weak_factory_on_impl_thread_.InvalidateWeakPtrs();
  completion->Signal();
}

size_t ThreadProxy::MaxPartialTextureUpdates() const {
  return ResourceUpdateController::MaxPartialTextureUpdates();
}

ThreadProxy::BeginFrameAndCommitState::BeginFrameAndCommitState()
    : memory_allocation_limit_bytes(0),
      memory_allocation_priority_cutoff(0),
      evicted_ui_resources(false) {}

ThreadProxy::BeginFrameAndCommitState::~BeginFrameAndCommitState() {}

scoped_ptr<base::Value> ThreadProxy::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());

  CompletionEvent completion;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(
        const_cast<ThreadProxy*>(this));
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::AsValueOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion,
                   state.get()));
    completion.Wait();
  }
  return state.PassAs<base::Value>();
}

void ThreadProxy::AsValueOnImplThread(CompletionEvent* completion,
                                      base::DictionaryValue* state) const {
  state->Set("layer_tree_host_impl",
             layer_tree_host_impl_->AsValue().release());
  completion->Signal();
}

bool ThreadProxy::CommitPendingForTesting() {
  DCHECK(IsMainThread());
  CommitPendingRequest commit_pending_request;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::CommitPendingOnImplThreadForTesting,
                   impl_thread_weak_ptr_,
                   &commit_pending_request));
    commit_pending_request.completion.Wait();
  }
  return commit_pending_request.commit_pending;
}

void ThreadProxy::CommitPendingOnImplThreadForTesting(
    CommitPendingRequest* request) {
  DCHECK(IsImplThread());
  if (layer_tree_host_impl_->output_surface())
    request->commit_pending = scheduler_on_impl_thread_->CommitPending();
  else
    request->commit_pending = false;
  request->completion.Signal();
}

scoped_ptr<base::Value> ThreadProxy::SchedulerStateAsValueForTesting() {
  if (IsImplThread())
    return scheduler_on_impl_thread_->StateAsValue().Pass();

  SchedulerStateRequest scheduler_state_request;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::SchedulerStateAsValueOnImplThreadForTesting,
                   impl_thread_weak_ptr_,
                   &scheduler_state_request));
    scheduler_state_request.completion.Wait();
  }
  return scheduler_state_request.state.Pass();
}

void ThreadProxy::SchedulerStateAsValueOnImplThreadForTesting(
    SchedulerStateRequest* request) {
  DCHECK(IsImplThread());
  request->state = scheduler_on_impl_thread_->StateAsValue();
  request->completion.Signal();
}

void ThreadProxy::RenewTreePriority() {
  DCHECK(IsImplThread());
  bool smoothness_takes_priority =
      layer_tree_host_impl_->pinch_gesture_active() ||
      layer_tree_host_impl_->CurrentlyScrollingLayer() ||
      layer_tree_host_impl_->page_scale_animation_active();

  base::TimeTicks now = layer_tree_host_impl_->CurrentPhysicalTimeTicks();

  // Update expiration time if smoothness currently takes priority.
  if (smoothness_takes_priority) {
    smoothness_takes_priority_expiration_time_ =
        now + base::TimeDelta::FromMilliseconds(
                  kSmoothnessTakesPriorityExpirationDelay * 1000);
  }

  // We use the same priority for both trees by default.
  TreePriority priority = SAME_PRIORITY_FOR_BOTH_TREES;

  // Smoothness takes priority if expiration time is in the future.
  if (smoothness_takes_priority_expiration_time_ > now)
    priority = SMOOTHNESS_TAKES_PRIORITY;

  // New content always takes priority when the active tree has
  // evicted resources or there is an invalid viewport size.
  if (layer_tree_host_impl_->active_tree()->ContentsTexturesPurged() ||
      layer_tree_host_impl_->active_tree()->ViewportSizeInvalid() ||
      layer_tree_host_impl_->EvictedUIResourcesExist() ||
      input_throttled_until_commit_)
    priority = NEW_CONTENT_TAKES_PRIORITY;

  layer_tree_host_impl_->SetTreePriority(priority);

  // Notify the the client of this compositor via the output surface.
  // TODO(epenner): Route this to compositor-thread instead of output-surface
  // after GTFO refactor of compositor-thread (http://crbug/170828).
  if (layer_tree_host_impl_->output_surface()) {
    layer_tree_host_impl_->output_surface()->
        UpdateSmoothnessTakesPriority(priority == SMOOTHNESS_TAKES_PRIORITY);
  }

  base::TimeDelta delay = smoothness_takes_priority_expiration_time_ - now;

  // Need to make sure a delayed task is posted when we have smoothness
  // takes priority expiration time in the future.
  if (delay <= base::TimeDelta())
    return;
  if (renew_tree_priority_on_impl_thread_pending_)
    return;

  Proxy::ImplThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::RenewTreePriorityOnImplThread,
                 weak_factory_on_impl_thread_.GetWeakPtr()),
      delay);

  renew_tree_priority_on_impl_thread_pending_ = true;
}

void ThreadProxy::RenewTreePriorityOnImplThread() {
  DCHECK(renew_tree_priority_on_impl_thread_pending_);
  renew_tree_priority_on_impl_thread_pending_ = false;

  RenewTreePriority();
}

void ThreadProxy::RequestScrollbarAnimationOnImplThread(base::TimeDelta delay) {
  Proxy::ImplThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::StartScrollbarAnimationOnImplThread,
                 impl_thread_weak_ptr_),
      delay);
}

void ThreadProxy::StartScrollbarAnimationOnImplThread() {
  layer_tree_host_impl_->StartScrollbarAnimation();
}

void ThreadProxy::DidActivatePendingTree() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::DidActivatePendingTreeOnImplThread");

  if (completion_event_for_commit_held_on_tree_activation_ &&
      !layer_tree_host_impl_->pending_tree()) {
    TRACE_EVENT_INSTANT0("cc", "ReleaseCommitbyActivation",
                         TRACE_EVENT_SCOPE_THREAD);
    DCHECK(layer_tree_host_impl_->settings().impl_side_painting);
    completion_event_for_commit_held_on_tree_activation_->Signal();
    completion_event_for_commit_held_on_tree_activation_ = NULL;
  }

  UpdateBackgroundAnimateTicking();

  commit_to_activate_duration_history_.InsertSample(
      base::TimeTicks::HighResNow() - commit_complete_time_);
}

}  // namespace cc
