// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/thread_proxy.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "cc/base/thread.h"
#include "cc/input/input_handler.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/quads/draw_quad.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/frame_rate_controller.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"

namespace {

// Measured in seconds.
const double kContextRecreationTickRate = 0.03;

// Measured in seconds.
const double kSmoothnessTakesPriorityExpirationDelay = 0.25;

}  // namespace

namespace cc {

scoped_ptr<Proxy> ThreadProxy::Create(LayerTreeHost* layer_tree_host,
                                      scoped_ptr<Thread> implThread) {
  return make_scoped_ptr(
      new ThreadProxy(layer_tree_host, implThread.Pass())).PassAs<Proxy>();
}

ThreadProxy::ThreadProxy(LayerTreeHost* layer_tree_host,
                         scoped_ptr<Thread> implThread)
    : Proxy(implThread.Pass()),
      animate_requested_(false),
      commit_requested_(false),
      commit_request_sent_to_impl_thread_(false),
      created_offscreen_context_provider_(false),
      layer_tree_host_(layer_tree_host),
      renderer_initialized_(false),
      started_(false),
      textures_acquired_(true),
      in_composite_and_readback_(false),
      manage_tiles_pending_(false),
      weak_factory_on_impl_thread_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      begin_frame_completion_event_on_impl_thread_(NULL),
      readback_request_on_impl_thread_(NULL),
      commit_completion_event_on_impl_thread_(NULL),
      completion_event_for_commit_held_on_tree_activation_(NULL),
      texture_acquisition_completion_event_on_impl_thread_(NULL),
      next_frame_is_newly_committed_frame_on_impl_thread_(false),
      render_vsync_enabled_(layer_tree_host->settings().renderVSyncEnabled),
      inside_draw_(false),
      defer_commits_(false),
      renew_tree_priority_on_impl_thread_pending_(false) {
  TRACE_EVENT0("cc", "ThreadProxy::ThreadProxy");
  DCHECK(IsMainThread());
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
  DCHECK(!defer_commits_);

  if (!layer_tree_host_->InitializeRendererIfNeeded()) {
    TRACE_EVENT0("cc", "CompositeAndReadback_EarlyOut_LR_Uninitialized");
    return false;
  }

  // Perform a synchronous commit.
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    CompletionEvent begin_frame_completion;
    Proxy::ImplThread()->PostTask(
        base::Bind(&ThreadProxy::ForceBeginFrameOnImplThread,
                   impl_thread_weak_ptr_,
                   &begin_frame_completion));
    begin_frame_completion.Wait();
  }
  in_composite_and_readback_ = true;
  BeginFrame(scoped_ptr<BeginFrameAndCommitState>());
  in_composite_and_readback_ = false;

  // Perform a synchronous readback.
  ReadbackRequest request;
  request.rect = rect;
  request.pixels = pixels;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    Proxy::ImplThread()->PostTask(
        base::Bind(&ThreadProxy::RequestReadbackOnImplThread,
                   impl_thread_weak_ptr_,
                   &request));
    request.completion.Wait();
  }
  return request.success;
}

void ThreadProxy::RequestReadbackOnImplThread(ReadbackRequest* request) {
  DCHECK(Proxy::IsImplThread());
  DCHECK(!readback_request_on_impl_thread_);
  if (!layer_tree_host_impl_) {
    request->success = false;
    request->completion.Signal();
    return;
  }

  readback_request_on_impl_thread_ = request;
  scheduler_on_impl_thread_->SetNeedsRedraw();
  scheduler_on_impl_thread_->SetNeedsForcedRedraw();
}

void ThreadProxy::StartPageScaleAnimation(gfx::Vector2d target_offset,
                                          bool use_anchor,
                                          float scale,
                                          base::TimeDelta duration) {
  DCHECK(Proxy::IsMainThread());
  Proxy::ImplThread()->PostTask(
      base::Bind(&ThreadProxy::RequestStartPageScaleAnimationOnImplThread,
                 impl_thread_weak_ptr_,
                 target_offset,
                 use_anchor,
                 scale,
                 duration));
}

void ThreadProxy::RequestStartPageScaleAnimationOnImplThread(
    gfx::Vector2d target_offset,
    bool use_anchor,
    float scale,
    base::TimeDelta duration) {
  DCHECK(Proxy::IsImplThread());
  if (layer_tree_host_impl_) {
    layer_tree_host_impl_->StartPageScaleAnimation(
        target_offset, use_anchor, scale, base::TimeTicks::Now(), duration);
  }
}

void ThreadProxy::FinishAllRendering() {
  DCHECK(Proxy::IsMainThread());
  DCHECK(!defer_commits_);

  // Make sure all GL drawing is finished on the impl thread.
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  CompletionEvent completion;
  Proxy::ImplThread()->PostTask(
      base::Bind(&ThreadProxy::FinishAllRenderingOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion));
  completion.Wait();
}

bool ThreadProxy::IsStarted() const {
  DCHECK(Proxy::IsMainThread());
  return started_;
}

bool ThreadProxy::InitializeOutputSurface() {
  TRACE_EVENT0("cc", "ThreadProxy::InitializeOutputSurface");
  scoped_ptr<OutputSurface> context = layer_tree_host_->CreateOutputSurface();
  if (!context)
    return false;

  Proxy::ImplThread()->PostTask(
      base::Bind(&ThreadProxy::InitializeOutputSurfaceOnImplThread,
                 impl_thread_weak_ptr_,
                 base::Passed(&context)));
  return true;
}

void ThreadProxy::SetSurfaceReady() {
  TRACE_EVENT0("cc", "ThreadProxy::SetSurfaceReady");
  Proxy::ImplThread()->PostTask(base::Bind(
      &ThreadProxy::SetSurfaceReadyOnImplThread, impl_thread_weak_ptr_));
}

void ThreadProxy::SetSurfaceReadyOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::SetSurfaceReadyOnImplThread");
  scheduler_on_impl_thread_->SetCanBeginFrame(true);
}

void ThreadProxy::SetVisible(bool visible) {
  TRACE_EVENT0("cc", "ThreadProxy::SetVisible");
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  CompletionEvent completion;
  Proxy::ImplThread()->PostTask(base::Bind(&ThreadProxy::SetVisibleOnImplThread,
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
  completion->Signal();
}

bool ThreadProxy::InitializeRenderer() {
  TRACE_EVENT0("cc", "ThreadProxy::InitializeRenderer");
  // Make a blocking call to initializeRendererOnImplThread. The results of that
  // call are pushed into the initialize_succeeded and capabilities local
  // variables.
  CompletionEvent completion;
  bool initialize_succeeded = false;
  RendererCapabilities capabilities;
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  Proxy::ImplThread()->PostTask(
      base::Bind(&ThreadProxy::InitializeRendererOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion,
                 &initialize_succeeded,
                 &capabilities));
  completion.Wait();

  if (initialize_succeeded) {
    renderer_initialized_ = true;
    renderer_capabilities_main_thread_copy_ = capabilities;
  }
  return initialize_succeeded;
}

bool ThreadProxy::RecreateOutputSurface() {
  TRACE_EVENT0("cc", "ThreadProxy::RecreateOutputSurface");
  DCHECK(IsMainThread());

  // Try to create the surface.
  scoped_ptr<OutputSurface> output_surface =
      layer_tree_host_->CreateOutputSurface();
  if (!output_surface)
    return false;
  scoped_refptr<cc::ContextProvider> offscreen_context_provider;
  if (created_offscreen_context_provider_) {
    offscreen_context_provider = layer_tree_host_->client()->
        OffscreenContextProviderForCompositorThread();
    if (!offscreen_context_provider)
      return false;
  }

  // Make a blocking call to recreateOutputSurfaceOnImplThread. The results of
  // that call are pushed into the recreate_succeeded and capabilities local
  // variables.
  CompletionEvent completion;
  bool recreate_succeeded = false;
  RendererCapabilities capabilities;
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  Proxy::ImplThread()->PostTask(
      base::Bind(&ThreadProxy::RecreateOutputSurfaceOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion,
                 base::Passed(&output_surface),
                 offscreen_context_provider,
                 &recreate_succeeded,
                 &capabilities));
  completion.Wait();

  if (recreate_succeeded)
    renderer_capabilities_main_thread_copy_ = capabilities;
  return recreate_succeeded;
}

const RendererCapabilities& ThreadProxy::GetRendererCapabilities() const {
  DCHECK(renderer_initialized_);
  return renderer_capabilities_main_thread_copy_;
}

void ThreadProxy::SetNeedsAnimate() {
  DCHECK(IsMainThread());
  if (animate_requested_)
    return;

  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsAnimate");
  animate_requested_ = true;

  if (commit_request_sent_to_impl_thread_)
    return;
  commit_request_sent_to_impl_thread_ = true;
  Proxy::ImplThread()->PostTask(base::Bind(
      &ThreadProxy::SetNeedsCommitOnImplThread, impl_thread_weak_ptr_));
}

void ThreadProxy::SetNeedsCommit() {
  DCHECK(IsMainThread());
  if (commit_requested_)
    return;
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsCommit");
  commit_requested_ = true;

  if (commit_request_sent_to_impl_thread_)
    return;
  commit_request_sent_to_impl_thread_ = true;
  Proxy::ImplThread()->PostTask(base::Bind(
      &ThreadProxy::SetNeedsCommitOnImplThread, impl_thread_weak_ptr_));
}

void ThreadProxy::DidLoseOutputSurfaceOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::DidLoseOutputSurfaceOnImplThread");
  Proxy::ImplThread()->PostTask(
      base::Bind(&ThreadProxy::CheckOutputSurfaceStatusOnImplThread,
                 impl_thread_weak_ptr_));
}

void ThreadProxy::CheckOutputSurfaceStatusOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::CheckOutputSurfaceStatusOnImplThread");
  if (!layer_tree_host_impl_->IsContextLost())
    return;
  if (cc::ContextProvider* offscreen_contexts = layer_tree_host_impl_->
          resource_provider()->offscreen_context_provider())
    offscreen_contexts->VerifyContexts();
  scheduler_on_impl_thread_->DidLoseOutputSurface();
}

void ThreadProxy::OnSwapBuffersCompleteOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::OnSwapBuffersCompleteOnImplThread");
  scheduler_on_impl_thread_->DidSwapBuffersComplete();
  Proxy::MainThread()->PostTask(
      base::Bind(&ThreadProxy::DidCompleteSwapBuffers, main_thread_weak_ptr_));
}

void ThreadProxy::OnVSyncParametersChanged(base::TimeTicks timebase,
                                           base::TimeDelta interval) {
  DCHECK(IsImplThread());
  TRACE_EVENT2("cc",
               "ThreadProxy::OnVSyncParametersChanged",
               "timebase",
               (timebase - base::TimeTicks()).InMilliseconds(),
               "interval",
               interval.InMilliseconds());
  scheduler_on_impl_thread_->SetTimebaseAndInterval(timebase, interval);
}

void ThreadProxy::OnCanDrawStateChanged(bool can_draw) {
  DCHECK(IsImplThread());
  TRACE_EVENT1(
      "cc", "ThreadProxy::OnCanDrawStateChanged", "can_draw", can_draw);
  scheduler_on_impl_thread_->SetCanDraw(can_draw);
}

void ThreadProxy::OnHasPendingTreeStateChanged(bool has_pending_tree) {
  DCHECK(IsImplThread());
  TRACE_EVENT1("cc",
               "ThreadProxy::OnHasPendingTreeStateChanged",
               "has_pending_tree",
               has_pending_tree);
  scheduler_on_impl_thread_->SetHasPendingTree(has_pending_tree);
}

void ThreadProxy::SetNeedsCommitOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsCommitOnImplThread");
  scheduler_on_impl_thread_->SetNeedsCommit();
}

void ThreadProxy::SetNeedsManageTilesOnImplThread() {
  if (manage_tiles_pending_)
    return;
  Proxy::ImplThread()->PostTask(
      base::Bind(&ThreadProxy::ManageTilesOnImplThread, impl_thread_weak_ptr_));
  manage_tiles_pending_ = true;
}

void ThreadProxy::ManageTilesOnImplThread() {
  // TODO(nduca): If needed, move this into CCSchedulerStateMachine.
  manage_tiles_pending_ = false;
  if (layer_tree_host_impl_)
    layer_tree_host_impl_->ManageTiles();
}

void ThreadProxy::SetNeedsForcedCommitOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsForcedCommitOnImplThread");
  scheduler_on_impl_thread_->SetNeedsForcedCommit();
}

void ThreadProxy::PostAnimationEventsToMainThreadOnImplThread(
    scoped_ptr<AnimationEventsVector> events,
    base::Time wall_clock_time) {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc",
               "ThreadProxy::PostAnimationEventsToMainThreadOnImplThread");
  Proxy::MainThread()->PostTask(base::Bind(&ThreadProxy::SetAnimationEvents,
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

  // If we are using impl-side painting, then sendManagedMemoryStats is called
  // directly after the tile manager's manage function, and doesn't need to
  // interact with main thread's layer tree.
  if (layer_tree_host_->settings().implSidePainting)
    return;

  layer_tree_host_impl_->SendManagedMemoryStats(
      layer_tree_host_->contents_texture_manager()->MemoryVisibleBytes(),
      layer_tree_host_->contents_texture_manager()->
          MemoryVisibleAndNearbyBytes(),
      layer_tree_host_->contents_texture_manager()->MemoryUseBytes());
}

bool ThreadProxy::IsInsideDraw() { return inside_draw_; }

void ThreadProxy::SetNeedsRedraw() {
  DCHECK(IsMainThread());
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsRedraw");
  Proxy::ImplThread()->PostTask(base::Bind(
      &ThreadProxy::SetFullRootLayerDamageOnImplThread, impl_thread_weak_ptr_));
  Proxy::ImplThread()->PostTask(base::Bind(
      &ThreadProxy::SetNeedsRedrawOnImplThread, impl_thread_weak_ptr_));
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
    Proxy::MainThread()->PostTask(
        base::Bind(&ThreadProxy::BeginFrame,
                   main_thread_weak_ptr_,
                   base::Passed(&pending_deferred_commit_)));
}

bool ThreadProxy::CommitRequested() const {
  DCHECK(IsMainThread());
  return commit_requested_;
}

void ThreadProxy::SetNeedsRedrawOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::setNeedsRedrawOnImplThread");
  scheduler_on_impl_thread_->SetNeedsRedraw();
}

void ThreadProxy::DidSwapUseIncompleteTileOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::DidSwapUseIncompleteTileOnImplThread");
  scheduler_on_impl_thread_->DidSwapUseIncompleteTile();
}

void ThreadProxy::DidInitializeVisibleTileOnImplThread() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::DidInitializeVisibleTileOnImplThread");
  scheduler_on_impl_thread_->SetNeedsRedraw();
}

void ThreadProxy::MainThreadHasStoppedFlinging() {
  if (input_handler_on_impl_thread_)
    input_handler_on_impl_thread_->MainThreadHasStoppedFlinging();
}

void ThreadProxy::Start() {
  DCHECK(IsMainThread());
  DCHECK(Proxy::ImplThread());
  // Create LayerTreeHostImpl.
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  CompletionEvent completion;
  scoped_ptr<InputHandler> handler = layer_tree_host_->CreateInputHandler();
  Proxy::ImplThread()->PostTask(
      base::Bind(&ThreadProxy::InitializeImplOnImplThread,
                 base::Unretained(this),
                 &completion,
                 handler.release()));
  completion.Wait();

  main_thread_weak_ptr_ = weak_factory_.GetWeakPtr();

  started_ = true;
}

void ThreadProxy::Stop() {
  TRACE_EVENT0("cc", "ThreadProxy::Stop");
  DCHECK(IsMainThread());
  DCHECK(started_);

  // Synchronously deletes the impl.
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);

    CompletionEvent completion;
    Proxy::ImplThread()->PostTask(
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
  Proxy::ImplThread()->PostTask(
      base::Bind(&ThreadProxy::ForceSerializeOnSwapBuffersOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion));
  completion.Wait();
}

void ThreadProxy::ForceSerializeOnSwapBuffersOnImplThread(
    CompletionEvent* completion) {
  if (renderer_initialized_)
    layer_tree_host_impl_->renderer()->DoNoOp();
  completion->Signal();
}

void ThreadProxy::FinishAllRenderingOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::FinishAllRenderingOnImplThread");
  DCHECK(IsImplThread());
  layer_tree_host_impl_->FinishAllRendering();
  completion->Signal();
}

void ThreadProxy::ForceBeginFrameOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::ForceBeginFrameOnImplThread");
  DCHECK(!begin_frame_completion_event_on_impl_thread_);

  SetNeedsForcedCommitOnImplThread();
  if (scheduler_on_impl_thread_->CommitPending()) {
    completion->Signal();
    return;
  }

  begin_frame_completion_event_on_impl_thread_ = completion;
}

void ThreadProxy::ScheduledActionBeginFrame() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionBeginFrame");
  scoped_ptr<BeginFrameAndCommitState> begin_frame_state(
      new BeginFrameAndCommitState);
  begin_frame_state->monotonic_frame_begin_time = base::TimeTicks::Now();
  begin_frame_state->scroll_info = layer_tree_host_impl_->ProcessScrollDeltas();
  begin_frame_state->impl_transform =
      layer_tree_host_impl_->active_tree()->ImplTransform();
  DCHECK_GT(layer_tree_host_impl_->memory_allocation_limit_bytes(), 0u);
  begin_frame_state->memory_allocation_limit_bytes =
      layer_tree_host_impl_->memory_allocation_limit_bytes();
  Proxy::MainThread()->PostTask(base::Bind(&ThreadProxy::BeginFrame,
                                           main_thread_weak_ptr_,
                                           base::Passed(&begin_frame_state)));

  if (begin_frame_completion_event_on_impl_thread_) {
    begin_frame_completion_event_on_impl_thread_->Signal();
    begin_frame_completion_event_on_impl_thread_ = NULL;
  }
}

void ThreadProxy::BeginFrame(
    scoped_ptr<BeginFrameAndCommitState> begin_frame_state) {
  TRACE_EVENT0("cc", "ThreadProxy::BeginFrame");
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
  // the apply/animate/layout part of the beginFrameAndCommit process since
  // those commit requests will get painted immediately. Once we have done
  // the paint, commit_requested_ will be set to false to allow new commit
  // requests to be scheduled.
  commit_requested_ = true;
  commit_request_sent_to_impl_thread_ = true;

  // On the other hand, the animationRequested flag needs to be cleared
  // here so that any animation requests generated by the apply or animate
  // callbacks will trigger another frame.
  animate_requested_ = false;

  if (begin_frame_state) {
    layer_tree_host_->ApplyScrollAndScale(*begin_frame_state->scroll_info);
    layer_tree_host_->SetImplTransform(begin_frame_state->impl_transform);
  }

  if (!in_composite_and_readback_ && !layer_tree_host_->visible()) {
    commit_requested_ = false;
    commit_request_sent_to_impl_thread_ = false;

    TRACE_EVENT0("cc", "EarlyOut_NotVisible");
    Proxy::ImplThread()->PostTask(base::Bind(
        &ThreadProxy::BeginFrameAbortedOnImplThread, impl_thread_weak_ptr_));
    return;
  }

  layer_tree_host_->WillBeginFrame();

  if (begin_frame_state) {
    layer_tree_host_->UpdateAnimations(
        begin_frame_state->monotonic_frame_begin_time);
  }

  // Unlink any backings that the impl thread has evicted, so that we know to
  // re-paint them in updateLayers.
  if (layer_tree_host_->contents_texture_manager()) {
    layer_tree_host_->contents_texture_manager()->
        UnlinkAndClearEvictedBackings();
  }

  layer_tree_host_->Layout();

  // Clear the commit flag after updating animations and layout here --- objects
  // that only layout when painted will trigger another setNeedsCommit inside
  // updateLayers.
  commit_requested_ = false;
  commit_request_sent_to_impl_thread_ = false;

  if (!layer_tree_host_->InitializeRendererIfNeeded()) {
    TRACE_EVENT0("cc", "EarlyOut_InitializeFailed");
    return;
  }

  scoped_ptr<ResourceUpdateQueue> queue =
      make_scoped_ptr(new ResourceUpdateQueue);
  layer_tree_host_->UpdateLayers(
      queue.get(),
      begin_frame_state ? begin_frame_state->memory_allocation_limit_bytes
                        : 0u);

  // Once single buffered layers are committed, they cannot be modified until
  // they are drawn by the impl thread.
  textures_acquired_ = false;

  layer_tree_host_->WillCommit();
  // Before applying scrolls and calling animate, we set animate_requested_ to
  // false. If it is true now, it means setNeedAnimate was called again, but
  // during a state when commit_request_sent_to_impl_thread_ = true. We need to
  // force that call to happen again now so that the commit request is sent to
  // the impl thread.
  if (animate_requested_) {
    // Forces setNeedsAnimate to consider posting a commit task.
    animate_requested_ = false;
    SetNeedsAnimate();
  }

  scoped_refptr<cc::ContextProvider> offscreen_context_provider;
  if (renderer_capabilities_main_thread_copy_.using_offscreen_context3d &&
      layer_tree_host_->needs_offscreen_context()) {
    offscreen_context_provider = layer_tree_host_->client()->
        OffscreenContextProviderForCompositorThread();
    if (offscreen_context_provider)
      created_offscreen_context_provider_ = true;
  }

  // Notify the impl thread that the beginFrame has completed. This will
  // begin the commit process, which is blocking from the main thread's
  // point of view, but asynchronously performed on the impl thread,
  // coordinated by the Scheduler.
  {
    TRACE_EVENT0("cc", "commit");

    DebugScopedSetMainThreadBlocked main_thread_blocked(this);

    RenderingStatsInstrumentation* stats_instrumentation =
        layer_tree_host_->rendering_stats_instrumentation();
    base::TimeTicks start_time = stats_instrumentation->StartRecording();

    CompletionEvent completion;
    Proxy::ImplThread()->PostTask(
        base::Bind(&ThreadProxy::BeginFrameCompleteOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion,
                   queue.release(),
                   offscreen_context_provider));
    completion.Wait();

    base::TimeDelta duration = stats_instrumentation->EndRecording(start_time);
    stats_instrumentation->AddCommit(duration);
  }

  layer_tree_host_->CommitComplete();
  layer_tree_host_->DidBeginFrame();
}

void ThreadProxy::BeginFrameCompleteOnImplThread(
    CompletionEvent* completion,
    ResourceUpdateQueue* raw_queue,
    scoped_refptr<cc::ContextProvider> offscreen_context_provider) {
  scoped_ptr<ResourceUpdateQueue> queue(raw_queue);

  TRACE_EVENT0("cc", "ThreadProxy::BeginFrameCompleteOnImplThread");
  DCHECK(!commit_completion_event_on_impl_thread_);
  DCHECK(IsImplThread() && IsMainThreadBlocked());
  DCHECK(scheduler_on_impl_thread_);
  DCHECK(scheduler_on_impl_thread_->CommitPending());

  if (!layer_tree_host_impl_) {
    TRACE_EVENT0("cc", "EarlyOut_NoLayerTree");
    completion->Signal();
    return;
  }

  if (offscreen_context_provider)
    offscreen_context_provider->BindToCurrentThread();
  layer_tree_host_impl_->resource_provider()->
      set_offscreen_context_provider(offscreen_context_provider);

  if (layer_tree_host_->contents_texture_manager()->
          LinkedEvictedBackingsExist()) {
    // Clear any uploads we were making to textures linked to evicted
    // resources
    queue->clearUploadsToEvictedResources();
    // Some textures in the layer tree are invalid. Kick off another commit
    // to fill them again.
    SetNeedsCommitOnImplThread();
  }

  layer_tree_host_->contents_texture_manager()->
      PushTexturePrioritiesToBackings();

  current_resource_update_controller_on_impl_thread_ =
      ResourceUpdateController::Create(
          this,
          Proxy::ImplThread(),
          queue.Pass(),
          layer_tree_host_impl_->resource_provider());
  current_resource_update_controller_on_impl_thread_->PerformMoreUpdates(
      scheduler_on_impl_thread_->AnticipatedDrawTime());

  commit_completion_event_on_impl_thread_ = completion;
}

void ThreadProxy::BeginFrameAbortedOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::BeginFrameAbortedOnImplThread");
  DCHECK(IsImplThread());
  DCHECK(scheduler_on_impl_thread_);
  DCHECK(scheduler_on_impl_thread_->CommitPending());

  scheduler_on_impl_thread_->BeginFrameAborted();
}

void ThreadProxy::ScheduledActionCommit() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionCommit");
  DCHECK(IsImplThread());
  DCHECK(commit_completion_event_on_impl_thread_);
  DCHECK(current_resource_update_controller_on_impl_thread_);

  // Complete all remaining texture updates.
  current_resource_update_controller_on_impl_thread_->Finalize();
  current_resource_update_controller_on_impl_thread_.reset();

  layer_tree_host_impl_->BeginCommit();
  layer_tree_host_->BeginCommitOnImplThread(layer_tree_host_impl_.get());
  layer_tree_host_->FinishCommitOnImplThread(layer_tree_host_impl_.get());
  layer_tree_host_impl_->CommitComplete();

  next_frame_is_newly_committed_frame_on_impl_thread_ = true;

  if (layer_tree_host_->settings().implSidePainting &&
      layer_tree_host_->BlocksPendingCommit()) {
    // For some layer types in impl-side painting, the commit is held until
    // the pending tree is activated.
    TRACE_EVENT_INSTANT0("cc", "HoldCommit");
    completion_event_for_commit_held_on_tree_activation_ =
        commit_completion_event_on_impl_thread_;
    commit_completion_event_on_impl_thread_ = NULL;
  } else {
    commit_completion_event_on_impl_thread_->Signal();
    commit_completion_event_on_impl_thread_ = NULL;
  }

  // SetVisible kicks off the next scheduler action, so this must be last.
  scheduler_on_impl_thread_->SetVisible(layer_tree_host_impl_->visible());
}

void ThreadProxy::ScheduledActionCheckForCompletedTileUploads() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc",
               "ThreadProxy::ScheduledActionCheckForCompletedTileUploads");
  layer_tree_host_impl_->CheckForCompletedTileUploads();
}

void ThreadProxy::ScheduledActionActivatePendingTreeIfNeeded() {
  DCHECK(IsImplThread());
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionActivatePendingTreeIfNeeded");
  layer_tree_host_impl_->ActivatePendingTreeIfNeeded();
}

void ThreadProxy::ScheduledActionBeginContextRecreation() {
  DCHECK(IsImplThread());
  Proxy::MainThread()->PostTask(
      base::Bind(&ThreadProxy::BeginContextRecreation, main_thread_weak_ptr_));
}

ScheduledActionDrawAndSwapResult
ThreadProxy::ScheduledActionDrawAndSwapInternal(bool forced_draw) {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionDrawAndSwap");

  base::AutoReset<bool> mark_inside(&inside_draw_, true);

  ScheduledActionDrawAndSwapResult result;
  result.did_draw = false;
  result.did_swap = false;
  DCHECK(IsImplThread());
  DCHECK(layer_tree_host_impl_.get());
  if (!layer_tree_host_impl_)
    return result;

  DCHECK(layer_tree_host_impl_->renderer());
  if (!layer_tree_host_impl_->renderer())
    return result;

  // FIXME: compute the frame display time more intelligently
  base::TimeTicks monotonic_time = base::TimeTicks::Now();
  base::Time wall_clock_time = base::Time::Now();

  if (input_handler_on_impl_thread_)
    input_handler_on_impl_thread_->Animate(monotonic_time);

  layer_tree_host_impl_->ActivatePendingTreeIfNeeded();
  layer_tree_host_impl_->Animate(monotonic_time, wall_clock_time);

  // This method is called on a forced draw, regardless of whether we are able
  // to produce a frame, as the calling site on main thread is blocked until its
  // request completes, and we signal completion here. If canDraw() is false, we
  // will indicate success=false to the caller, but we must still signal
  // completion to avoid deadlock.

  // We guard prepareToDraw() with canDraw() because it always returns a valid
  // frame, so can only be used when such a frame is possible. Since
  // drawLayers() depends on the result of prepareToDraw(), it is guarded on
  // canDraw() as well.

  LayerTreeHostImpl::FrameData frame;
  bool draw_frame =
      layer_tree_host_impl_->CanDraw() &&
      (layer_tree_host_impl_->PrepareToDraw(&frame) || forced_draw);
  if (draw_frame) {
    layer_tree_host_impl_->DrawLayers(
        &frame,
        scheduler_on_impl_thread_->LastVSyncTime());
    result.did_draw= true;
  }
  layer_tree_host_impl_->DidDrawAllLayers(frame);

  // Check for tree activation.
  if (completion_event_for_commit_held_on_tree_activation_ &&
      !layer_tree_host_impl_->pending_tree()) {
    TRACE_EVENT_INSTANT0("cc", "ReleaseCommitbyActivation");
    DCHECK(layer_tree_host_impl_->settings().implSidePainting);
    completion_event_for_commit_held_on_tree_activation_->Signal();
    completion_event_for_commit_held_on_tree_activation_ = NULL;
  }

  // Check for a pending compositeAndReadback.
  if (readback_request_on_impl_thread_) {
    readback_request_on_impl_thread_->success = false;
    if (draw_frame) {
      layer_tree_host_impl_->Readback(readback_request_on_impl_thread_->pixels,
                                      readback_request_on_impl_thread_->rect);
      readback_request_on_impl_thread_->success =
          !layer_tree_host_impl_->IsContextLost();
    }
    readback_request_on_impl_thread_->completion.Signal();
    readback_request_on_impl_thread_ = NULL;
  } else if (draw_frame) {
    result.did_swap = layer_tree_host_impl_->SwapBuffers();

    if (frame.contains_incomplete_tile)
      DidSwapUseIncompleteTileOnImplThread();
  }

  // Tell the main thread that the the newly-commited frame was drawn.
  if (next_frame_is_newly_committed_frame_on_impl_thread_) {
    next_frame_is_newly_committed_frame_on_impl_thread_ = false;
    Proxy::MainThread()->PostTask(
        base::Bind(&ThreadProxy::DidCommitAndDrawFrame, main_thread_weak_ptr_));
  }

  if (draw_frame)
    CheckOutputSurfaceStatusOnImplThread();

  layer_tree_host_impl_->BeginNextFrame();

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
  Proxy::ImplThread()->PostTask(
      base::Bind(&ThreadProxy::AcquireLayerTexturesForMainThreadOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion));
  // Block until it is safe to write to layer textures from the main thread.
  completion.Wait();

  textures_acquired_ = true;
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

ScheduledActionDrawAndSwapResult
ThreadProxy::ScheduledActionDrawAndSwapIfPossible() {
  return ScheduledActionDrawAndSwapInternal(false);
}

ScheduledActionDrawAndSwapResult
ThreadProxy::ScheduledActionDrawAndSwapForced() {
  return ScheduledActionDrawAndSwapInternal(true);
}

void ThreadProxy::DidAnticipatedDrawTimeChange(base::TimeTicks time) {
  if (layer_tree_host_impl_)
    layer_tree_host_impl_->SetAnticipatedDrawTime(time);

  if (current_resource_update_controller_on_impl_thread_)
    current_resource_update_controller_on_impl_thread_->PerformMoreUpdates(time);
}

void ThreadProxy::ReadyToFinalizeTextureUpdates() {
  DCHECK(IsImplThread());
  scheduler_on_impl_thread_->BeginFrameComplete();
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

void ThreadProxy::BeginContextRecreation() {
  TRACE_EVENT0("cc", "ThreadProxy::BeginContextRecreation");
  DCHECK(IsMainThread());
  layer_tree_host_->DidLoseOutputSurface();
  output_surface_recreation_callback_.Reset(base::Bind(
      &ThreadProxy::TryToRecreateOutputSurface, base::Unretained(this)));
  Proxy::MainThread()->PostTask(output_surface_recreation_callback_.callback());
}

void ThreadProxy::TryToRecreateOutputSurface() {
  DCHECK(IsMainThread());
  DCHECK(layer_tree_host_);
  LayerTreeHost::RecreateResult result =
      layer_tree_host_->RecreateOutputSurface();
  if (result == LayerTreeHost::RecreateFailedButTryAgain)
    Proxy::MainThread()->PostTask(
        output_surface_recreation_callback_.callback());
  else if (result == LayerTreeHost::RecreateSucceeded)
    output_surface_recreation_callback_.Cancel();
}

void ThreadProxy::InitializeImplOnImplThread(CompletionEvent* completion,
                                             InputHandler* handler) {
  TRACE_EVENT0("cc", "ThreadProxy::InitializeImplOnImplThread");
  DCHECK(IsImplThread());
  layer_tree_host_impl_ = layer_tree_host_->CreateLayerTreeHostImpl(this);
  const base::TimeDelta displayRefreshInterval =
      base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond /
                                        60);
  scoped_ptr<FrameRateController> frame_rate_controller;
  if (render_vsync_enabled_) {
    frame_rate_controller.reset(
        new FrameRateController(DelayBasedTimeSource::Create(
            displayRefreshInterval, Proxy::ImplThread())));
  } else {
    frame_rate_controller.reset(new FrameRateController(Proxy::ImplThread()));
  }
  SchedulerSettings scheduler_settings;
  scheduler_settings.impl_side_painting =
      layer_tree_host_->settings().implSidePainting;
  scheduler_on_impl_thread_ = Scheduler::Create(this,
                                                frame_rate_controller.Pass(),
                                                scheduler_settings);
  scheduler_on_impl_thread_->SetVisible(layer_tree_host_impl_->visible());

  input_handler_on_impl_thread_ = scoped_ptr<InputHandler>(handler);
  if (input_handler_on_impl_thread_)
    input_handler_on_impl_thread_->BindToClient(layer_tree_host_impl_.get());

  impl_thread_weak_ptr_ = weak_factory_on_impl_thread_.GetWeakPtr();
  completion->Signal();
}

void ThreadProxy::InitializeOutputSurfaceOnImplThread(
    scoped_ptr<OutputSurface> output_surface) {
  TRACE_EVENT0("cc", "ThreadProxy::InitializeContextOnImplThread");
  DCHECK(IsImplThread());
  output_surface_before_initialization_on_impl_thread_ = output_surface.Pass();
}

void ThreadProxy::InitializeRendererOnImplThread(
    CompletionEvent* completion,
    bool* initialize_succeeded,
    RendererCapabilities* capabilities) {
  TRACE_EVENT0("cc", "ThreadProxy::InitializeRendererOnImplThread");
  DCHECK(IsImplThread());
  DCHECK(output_surface_before_initialization_on_impl_thread_.get());
  *initialize_succeeded = layer_tree_host_impl_->InitializeRenderer(
      output_surface_before_initialization_on_impl_thread_.Pass());
  if (*initialize_succeeded) {
    *capabilities = layer_tree_host_impl_->GetRendererCapabilities();
    scheduler_on_impl_thread_->SetSwapBuffersCompleteSupported(
        capabilities->using_swap_complete_callback);

    int maxFramesPending = layer_tree_host_impl_->output_surface()->
        capabilities().max_frames_pending;
    if (maxFramesPending <= 0)
      maxFramesPending = FrameRateController::DEFAULT_MAX_FRAMES_PENDING;
    if (layer_tree_host_impl_->output_surface()->capabilities().
            has_parent_compositor)
      maxFramesPending = 1;
    scheduler_on_impl_thread_->SetMaxFramesPending(maxFramesPending);
  }

  completion->Signal();
}

void ThreadProxy::LayerTreeHostClosedOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::LayerTreeHostClosedOnImplThread");
  DCHECK(IsImplThread());
  layer_tree_host_->DeleteContentsTexturesOnImplThread(
      layer_tree_host_impl_->resource_provider());
  input_handler_on_impl_thread_.reset();
  layer_tree_host_impl_.reset();
  scheduler_on_impl_thread_.reset();
  weak_factory_on_impl_thread_.InvalidateWeakPtrs();
  completion->Signal();
}

void ThreadProxy::SetFullRootLayerDamageOnImplThread() {
  DCHECK(IsImplThread());
  layer_tree_host_impl_->SetFullRootLayerDamage();
}

size_t ThreadProxy::MaxPartialTextureUpdates() const {
  return ResourceUpdateController::MaxPartialTextureUpdates();
}

void ThreadProxy::RecreateOutputSurfaceOnImplThread(
    CompletionEvent* completion,
    scoped_ptr<OutputSurface> output_surface,
    scoped_refptr<cc::ContextProvider> offscreen_context_provider,
    bool* recreate_succeeded,
    RendererCapabilities* capabilities) {
  TRACE_EVENT0("cc", "ThreadProxy::RecreateOutputSurfaceOnImplThread");
  DCHECK(IsImplThread());

  layer_tree_host_->DeleteContentsTexturesOnImplThread(
      layer_tree_host_impl_->resource_provider());
  *recreate_succeeded =
      layer_tree_host_impl_->InitializeRenderer(output_surface.Pass());
  if (offscreen_context_provider)
      offscreen_context_provider->BindToCurrentThread();

  if (*recreate_succeeded) {
    *capabilities = layer_tree_host_impl_->GetRendererCapabilities();
    layer_tree_host_impl_->resource_provider()->
        set_offscreen_context_provider(offscreen_context_provider);
    scheduler_on_impl_thread_->DidRecreateOutputSurface();
  } else if (offscreen_context_provider) {
    offscreen_context_provider->VerifyContexts();
  }
  completion->Signal();
}

ThreadProxy::BeginFrameAndCommitState::BeginFrameAndCommitState()
    : memory_allocation_limit_bytes(0) {}

ThreadProxy::BeginFrameAndCommitState::~BeginFrameAndCommitState() {}

scoped_ptr<base::Value> ThreadProxy::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());

  CompletionEvent completion;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(
        const_cast<ThreadProxy*>(this));
    Proxy::ImplThread()->PostTask(base::Bind(&ThreadProxy::AsValueOnImplThread,
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
    Proxy::ImplThread()->PostTask(
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

skia::RefPtr<SkPicture> ThreadProxy::CapturePicture() {
  DCHECK(IsMainThread());
  CompletionEvent completion;
  skia::RefPtr<SkPicture> picture;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    Proxy::ImplThread()->PostTask(
        base::Bind(&ThreadProxy::CapturePictureOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion,
                   &picture));
    completion.Wait();
  }
  return picture;
}

void ThreadProxy::CapturePictureOnImplThread(CompletionEvent* completion,
                                             skia::RefPtr<SkPicture>* picture) {
  DCHECK(IsImplThread());
  *picture = layer_tree_host_impl_->CapturePicture();
  completion->Signal();
}

void ThreadProxy::RenewTreePriority() {
  bool smoothness_takes_priority =
      layer_tree_host_impl_->pinch_gesture_active() ||
      layer_tree_host_impl_->CurrentlyScrollingLayer() ||
      layer_tree_host_impl_->page_scale_animation_active();

  // Update expiration time if smoothness currently takes priority.
  if (smoothness_takes_priority) {
    smoothness_takes_priority_expiration_time_ =
        base::TimeTicks::Now() +
        base::TimeDelta::FromMilliseconds(
            kSmoothnessTakesPriorityExpirationDelay * 1000);
  }

  // We use the same priority for both trees by default.
  TreePriority priority = SAME_PRIORITY_FOR_BOTH_TREES;

  // Smoothness takes priority if expiration time is in the future.
  if (smoothness_takes_priority_expiration_time_ > base::TimeTicks::Now())
    priority = SMOOTHNESS_TAKES_PRIORITY;

  // New content always takes priority when the active tree has
  // evicted resources or there is an invalid viewport size.
  if (layer_tree_host_impl_->active_tree()->ContentsTexturesPurged() ||
      layer_tree_host_impl_->active_tree()->ViewportSizeInvalid())
    priority = NEW_CONTENT_TAKES_PRIORITY;

  layer_tree_host_impl_->SetTreePriority(priority);

  // Notify the the client of this compositor via the output surface.
  // TODO(epenner): Route this to compositor-thread instead of output-surface
  // after GTFO refactor of compositor-thread (http://crbug/170828).
  if (layer_tree_host_impl_->output_surface()) {
    layer_tree_host_impl_->output_surface()->
        UpdateSmoothnessTakesPriority(priority == SMOOTHNESS_TAKES_PRIORITY);
  }

  base::TimeDelta delay =
      smoothness_takes_priority_expiration_time_ - base::TimeTicks::Now();

  // Need to make sure a delayed task is posted when we have smoothness
  // takes priority expiration time in the future.
  if (delay <= base::TimeDelta())
    return;
  if (renew_tree_priority_on_impl_thread_pending_)
    return;

  Proxy::ImplThread()->PostDelayedTask(
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
  Proxy::ImplThread()->PostDelayedTask(
      base::Bind(&ThreadProxy::StartScrollbarAnimationOnImplThread,
                 impl_thread_weak_ptr_),
      delay);
}

void ThreadProxy::StartScrollbarAnimationOnImplThread() {
  layer_tree_host_impl_->StartScrollbarAnimation(base::TimeTicks::Now());
}

}  // namespace cc
