// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/single_thread_proxy.h"

#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/quads/draw_quad.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource_update_controller.h"
#include "cc/trees/blocking_task_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/scoped_abort_remaining_swap_promises.h"
#include "ui/gfx/frame_time.h"

namespace cc {

scoped_ptr<Proxy> SingleThreadProxy::Create(
    LayerTreeHost* layer_tree_host,
    LayerTreeHostSingleThreadClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  return make_scoped_ptr(
             new SingleThreadProxy(layer_tree_host, client, main_task_runner))
      .PassAs<Proxy>();
}

SingleThreadProxy::SingleThreadProxy(
    LayerTreeHost* layer_tree_host,
    LayerTreeHostSingleThreadClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : Proxy(main_task_runner, NULL),
      layer_tree_host_(layer_tree_host),
      client_(client),
      timing_history_(layer_tree_host->rendering_stats_instrumentation()),
      next_frame_is_newly_committed_frame_(false),
      inside_draw_(false),
      defer_commits_(false),
      commit_was_deferred_(false),
      commit_requested_(false),
      weak_factory_(this) {
  TRACE_EVENT0("cc", "SingleThreadProxy::SingleThreadProxy");
  DCHECK(Proxy::IsMainThread());
  DCHECK(layer_tree_host);

  // Impl-side painting not supported without threaded compositing.
  CHECK(!layer_tree_host->settings().impl_side_painting)
      << "Threaded compositing must be enabled to use impl-side painting.";
}

void SingleThreadProxy::Start() {
  DebugScopedSetImplThread impl(this);
  layer_tree_host_impl_ = layer_tree_host_->CreateLayerTreeHostImpl(this);
}

SingleThreadProxy::~SingleThreadProxy() {
  TRACE_EVENT0("cc", "SingleThreadProxy::~SingleThreadProxy");
  DCHECK(Proxy::IsMainThread());
  // Make sure Stop() got called or never Started.
  DCHECK(!layer_tree_host_impl_);
}

void SingleThreadProxy::FinishAllRendering() {
  TRACE_EVENT0("cc", "SingleThreadProxy::FinishAllRendering");
  DCHECK(Proxy::IsMainThread());
  {
    DebugScopedSetImplThread impl(this);
    layer_tree_host_impl_->FinishAllRendering();
  }
}

bool SingleThreadProxy::IsStarted() const {
  DCHECK(Proxy::IsMainThread());
  return layer_tree_host_impl_;
}

void SingleThreadProxy::SetLayerTreeHostClientReady() {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetLayerTreeHostClientReady");
  // Scheduling is controlled by the embedder in the single thread case, so
  // nothing to do.
  DCHECK(Proxy::IsMainThread());
  DebugScopedSetImplThread impl(this);
  if (layer_tree_host_->settings().single_thread_proxy_scheduler &&
      !scheduler_on_impl_thread_) {
    SchedulerSettings scheduler_settings(layer_tree_host_->settings());
    scheduler_on_impl_thread_ = Scheduler::Create(this,
                                                  scheduler_settings,
                                                  layer_tree_host_->id(),
                                                  MainThreadTaskRunner());
    scheduler_on_impl_thread_->SetCanStart();
    scheduler_on_impl_thread_->SetVisible(layer_tree_host_impl_->visible());
  }
}

void SingleThreadProxy::SetVisible(bool visible) {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetVisible");
  DebugScopedSetImplThread impl(this);
  layer_tree_host_impl_->SetVisible(visible);
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetVisible(layer_tree_host_impl_->visible());
  // Changing visibility could change ShouldComposite().
  UpdateBackgroundAnimateTicking();
}

void SingleThreadProxy::CreateAndInitializeOutputSurface() {
  TRACE_EVENT0(
      "cc", "SingleThreadProxy::CreateAndInitializeOutputSurface");
  DCHECK(Proxy::IsMainThread());
  DCHECK(layer_tree_host_->output_surface_lost());

  scoped_ptr<OutputSurface> output_surface =
      layer_tree_host_->CreateOutputSurface();

  renderer_capabilities_for_main_thread_ = RendererCapabilities();

  bool success = !!output_surface;
  if (success) {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    DebugScopedSetImplThread impl(this);
    layer_tree_host_->DeleteContentsTexturesOnImplThread(
        layer_tree_host_impl_->resource_provider());
    success = layer_tree_host_impl_->InitializeRenderer(output_surface.Pass());
  }

  layer_tree_host_->OnCreateAndInitializeOutputSurfaceAttempted(success);

  if (success) {
    if (scheduler_on_impl_thread_)
      scheduler_on_impl_thread_->DidCreateAndInitializeOutputSurface();
  } else if (Proxy::MainThreadTaskRunner()) {
    MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&SingleThreadProxy::CreateAndInitializeOutputSurface,
                   weak_factory_.GetWeakPtr()));
  }
}

const RendererCapabilities& SingleThreadProxy::GetRendererCapabilities() const {
  DCHECK(Proxy::IsMainThread());
  DCHECK(!layer_tree_host_->output_surface_lost());
  return renderer_capabilities_for_main_thread_;
}

void SingleThreadProxy::SetNeedsAnimate() {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetNeedsAnimate");
  DCHECK(Proxy::IsMainThread());
  client_->ScheduleAnimation();
  SetNeedsCommit();
}

void SingleThreadProxy::SetNeedsUpdateLayers() {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetNeedsUpdateLayers");
  DCHECK(Proxy::IsMainThread());
  SetNeedsCommit();
}

void SingleThreadProxy::DoCommit(const BeginFrameArgs& begin_frame_args) {
  TRACE_EVENT0("cc", "SingleThreadProxy::DoCommit");
  DCHECK(Proxy::IsMainThread());
  layer_tree_host_->WillBeginMainFrame();
  layer_tree_host_->BeginMainFrame(begin_frame_args);
  layer_tree_host_->AnimateLayers(begin_frame_args.frame_time);
  layer_tree_host_->Layout();
  commit_requested_ = false;

  if (PrioritizedResourceManager* contents_texture_manager =
          layer_tree_host_->contents_texture_manager()) {
    contents_texture_manager->UnlinkAndClearEvictedBackings();
    contents_texture_manager->SetMaxMemoryLimitBytes(
        layer_tree_host_impl_->memory_allocation_limit_bytes());
    contents_texture_manager->SetExternalPriorityCutoff(
        layer_tree_host_impl_->memory_allocation_priority_cutoff());
  }

  scoped_ptr<ResourceUpdateQueue> queue =
      make_scoped_ptr(new ResourceUpdateQueue);

  layer_tree_host_->UpdateLayers(queue.get());

  layer_tree_host_->WillCommit();

  // Commit immediately.
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    DebugScopedSetImplThread impl(this);

    // This CapturePostTasks should be destroyed before CommitComplete() is
    // called since that goes out to the embedder, and we want the embedder
    // to receive its callbacks before that.
    BlockingTaskRunner::CapturePostTasks blocked;

    layer_tree_host_impl_->BeginCommit();

    if (PrioritizedResourceManager* contents_texture_manager =
        layer_tree_host_->contents_texture_manager()) {
      contents_texture_manager->PushTexturePrioritiesToBackings();
    }
    layer_tree_host_->BeginCommitOnImplThread(layer_tree_host_impl_.get());

    scoped_ptr<ResourceUpdateController> update_controller =
        ResourceUpdateController::Create(
            NULL,
            MainThreadTaskRunner(),
            queue.Pass(),
            layer_tree_host_impl_->resource_provider());
    update_controller->Finalize();

    if (layer_tree_host_impl_->EvictedUIResourcesExist())
      layer_tree_host_->RecreateUIResources();

    layer_tree_host_->FinishCommitOnImplThread(layer_tree_host_impl_.get());

    layer_tree_host_impl_->CommitComplete();

    UpdateBackgroundAnimateTicking();

#if DCHECK_IS_ON
    // In the single-threaded case, the scale and scroll deltas should never be
    // touched on the impl layer tree.
    scoped_ptr<ScrollAndScaleSet> scroll_info =
        layer_tree_host_impl_->ProcessScrollDeltas();
    DCHECK(!scroll_info->scrolls.size());
    DCHECK_EQ(1.f, scroll_info->page_scale_delta);
#endif

    RenderingStatsInstrumentation* stats_instrumentation =
        layer_tree_host_->rendering_stats_instrumentation();
    benchmark_instrumentation::IssueMainThreadRenderingStatsEvent(
        stats_instrumentation->main_thread_rendering_stats());
    stats_instrumentation->AccumulateAndClearMainThreadStats();
  }
  layer_tree_host_->CommitComplete();
  layer_tree_host_->DidBeginMainFrame();
  timing_history_.DidCommit();

  next_frame_is_newly_committed_frame_ = true;
}

void SingleThreadProxy::SetNeedsCommit() {
  DCHECK(Proxy::IsMainThread());
  DebugScopedSetImplThread impl(this);
  client_->ScheduleComposite();
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetNeedsCommit();
  commit_requested_ = true;
}

void SingleThreadProxy::SetNeedsRedraw(const gfx::Rect& damage_rect) {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetNeedsRedraw");
  DCHECK(Proxy::IsMainThread());
  DebugScopedSetImplThread impl(this);
  client_->ScheduleComposite();
  SetNeedsRedrawRectOnImplThread(damage_rect);
}

void SingleThreadProxy::SetNextCommitWaitsForActivation() {
  // There is no activation here other than commit. So do nothing.
  DCHECK(Proxy::IsMainThread());
}

void SingleThreadProxy::SetDeferCommits(bool defer_commits) {
  DCHECK(Proxy::IsMainThread());
  // Deferring commits only makes sense if there's a scheduler.
  if (!scheduler_on_impl_thread_)
    return;
  if (defer_commits_ == defer_commits)
    return;

  if (defer_commits)
    TRACE_EVENT_ASYNC_BEGIN0("cc", "SingleThreadProxy::SetDeferCommits", this);
  else
    TRACE_EVENT_ASYNC_END0("cc", "SingleThreadProxy::SetDeferCommits", this);

  defer_commits_ = defer_commits;
  if (!defer_commits_ && commit_was_deferred_) {
    commit_was_deferred_ = false;
    BeginMainFrame();
  }
}

bool SingleThreadProxy::CommitRequested() const {
  DCHECK(Proxy::IsMainThread());
  return commit_requested_;
}

bool SingleThreadProxy::BeginMainFrameRequested() const {
  DCHECK(Proxy::IsMainThread());
  // If there is no scheduler, then there can be no pending begin frame,
  // as all frames are all manually initiated by the embedder of cc.
  if (!scheduler_on_impl_thread_)
    return false;
  return commit_requested_;
}

size_t SingleThreadProxy::MaxPartialTextureUpdates() const {
  return std::numeric_limits<size_t>::max();
}

void SingleThreadProxy::Stop() {
  TRACE_EVENT0("cc", "SingleThreadProxy::stop");
  DCHECK(Proxy::IsMainThread());
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    DebugScopedSetImplThread impl(this);

    BlockingTaskRunner::CapturePostTasks blocked;
    layer_tree_host_->DeleteContentsTexturesOnImplThread(
        layer_tree_host_impl_->resource_provider());
    scheduler_on_impl_thread_.reset();
    layer_tree_host_impl_.reset();
  }
  layer_tree_host_ = NULL;
}

void SingleThreadProxy::OnCanDrawStateChanged(bool can_draw) {
  TRACE_EVENT1(
      "cc", "SingleThreadProxy::OnCanDrawStateChanged", "can_draw", can_draw);
  DCHECK(Proxy::IsImplThread());
  UpdateBackgroundAnimateTicking();
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetCanDraw(can_draw);
}

void SingleThreadProxy::NotifyReadyToActivate() {
  // Impl-side painting only.
  NOTREACHED();
}

void SingleThreadProxy::SetNeedsRedrawOnImplThread() {
  client_->ScheduleComposite();
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetNeedsRedraw();
}

void SingleThreadProxy::SetNeedsAnimateOnImplThread() {
  SetNeedsRedrawOnImplThread();
}

void SingleThreadProxy::SetNeedsManageTilesOnImplThread() {
  // Impl-side painting only.
  NOTREACHED();
}

void SingleThreadProxy::SetNeedsRedrawRectOnImplThread(
    const gfx::Rect& damage_rect) {
  layer_tree_host_impl_->SetViewportDamage(damage_rect);
  SetNeedsRedrawOnImplThread();
}

void SingleThreadProxy::DidInitializeVisibleTileOnImplThread() {
  // Impl-side painting only.
  NOTREACHED();
}

void SingleThreadProxy::SetNeedsCommitOnImplThread() {
  client_->ScheduleComposite();
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->SetNeedsCommit();
}

void SingleThreadProxy::PostAnimationEventsToMainThreadOnImplThread(
    scoped_ptr<AnimationEventsVector> events) {
  TRACE_EVENT0(
      "cc", "SingleThreadProxy::PostAnimationEventsToMainThreadOnImplThread");
  DCHECK(Proxy::IsImplThread());
  DebugScopedSetMainThread main(this);
  layer_tree_host_->SetAnimationEvents(events.Pass());
}

bool SingleThreadProxy::ReduceContentsTextureMemoryOnImplThread(
    size_t limit_bytes,
    int priority_cutoff) {
  DCHECK(IsImplThread());
  PrioritizedResourceManager* contents_texture_manager =
      layer_tree_host_->contents_texture_manager();

  ResourceProvider* resource_provider =
      layer_tree_host_impl_->resource_provider();

  if (!contents_texture_manager || !resource_provider)
    return false;

  return contents_texture_manager->ReduceMemoryOnImplThread(
      limit_bytes, priority_cutoff, resource_provider);
}

bool SingleThreadProxy::IsInsideDraw() { return inside_draw_; }

void SingleThreadProxy::UpdateRendererCapabilitiesOnImplThread() {
  DCHECK(IsImplThread());
  renderer_capabilities_for_main_thread_ =
      layer_tree_host_impl_->GetRendererCapabilities().MainThreadCapabilities();
}

void SingleThreadProxy::DidManageTiles() {
  // Impl-side painting only.
  NOTREACHED();
}

void SingleThreadProxy::DidLoseOutputSurfaceOnImplThread() {
  TRACE_EVENT0("cc", "SingleThreadProxy::DidLoseOutputSurfaceOnImplThread");
  {
    DebugScopedSetMainThread main(this);
    // This must happen before we notify the scheduler as it may try to recreate
    // the output surface if already in BEGIN_IMPL_FRAME_STATE_IDLE.
    layer_tree_host_->DidLoseOutputSurface();
  }
  client_->DidAbortSwapBuffers();
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->DidLoseOutputSurface();
}

void SingleThreadProxy::DidSwapBuffersOnImplThread() {
  TRACE_EVENT0("cc", "SingleThreadProxy::DidSwapBuffersOnImplThread");
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->DidSwapBuffers();
  client_->DidPostSwapBuffers();
}

void SingleThreadProxy::DidSwapBuffersCompleteOnImplThread() {
  TRACE_EVENT0("cc", "SingleThreadProxy::DidSwapBuffersCompleteOnImplThread");
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->DidSwapBuffersComplete();
  layer_tree_host_->DidCompleteSwapBuffers();
}

void SingleThreadProxy::BeginFrame(const BeginFrameArgs& args) {
  TRACE_EVENT0("cc", "SingleThreadProxy::BeginFrame");
  if (scheduler_on_impl_thread_)
    scheduler_on_impl_thread_->BeginImplFrame(args);
}

void SingleThreadProxy::CompositeImmediately(base::TimeTicks frame_begin_time) {
  TRACE_EVENT0("cc", "SingleThreadProxy::CompositeImmediately");
  DCHECK(Proxy::IsMainThread());
  DCHECK(!layer_tree_host_->output_surface_lost());

  BeginFrameArgs begin_frame_args = BeginFrameArgs::Create(
      frame_begin_time, base::TimeTicks(), BeginFrameArgs::DefaultInterval());
  DoCommit(begin_frame_args);

  LayerTreeHostImpl::FrameData frame;
  DoComposite(frame_begin_time, &frame);
}

void SingleThreadProxy::AsValueInto(base::debug::TracedValue* state) const {
  // The following line casts away const modifiers because it is just
  // setting debug state. We still want the AsValue() function and its
  // call chain to be const throughout.
  DebugScopedSetImplThread impl(const_cast<SingleThreadProxy*>(this));

  state->BeginDictionary("layer_tree_host_impl");
  layer_tree_host_impl_->AsValueInto(state);
  state->EndDictionary();
}

void SingleThreadProxy::ForceSerializeOnSwapBuffers() {
  {
    DebugScopedSetImplThread impl(this);
    if (layer_tree_host_impl_->renderer()) {
      DCHECK(!layer_tree_host_->output_surface_lost());
      layer_tree_host_impl_->renderer()->DoNoOp();
    }
  }
}

bool SingleThreadProxy::SupportsImplScrolling() const {
  return false;
}

bool SingleThreadProxy::ShouldComposite() const {
  DCHECK(Proxy::IsImplThread());
  return layer_tree_host_impl_->visible() &&
         layer_tree_host_impl_->CanDraw();
}

void SingleThreadProxy::UpdateBackgroundAnimateTicking() {
  DCHECK(Proxy::IsImplThread());
  layer_tree_host_impl_->UpdateBackgroundAnimateTicking(
      !ShouldComposite() && layer_tree_host_impl_->active_tree()->root_layer());
}

DrawResult SingleThreadProxy::DoComposite(base::TimeTicks frame_begin_time,
                                          LayerTreeHostImpl::FrameData* frame) {
  TRACE_EVENT0("cc", "SingleThreadProxy::DoComposite");
  DCHECK(!layer_tree_host_->output_surface_lost());

  {
    DebugScopedSetImplThread impl(this);
    base::AutoReset<bool> mark_inside(&inside_draw_, true);

    // We guard PrepareToDraw() with CanDraw() because it always returns a valid
    // frame, so can only be used when such a frame is possible. Since
    // DrawLayers() depends on the result of PrepareToDraw(), it is guarded on
    // CanDraw() as well.
    if (!ShouldComposite()) {
      UpdateBackgroundAnimateTicking();
      return DRAW_ABORTED_CANT_DRAW;
    }

    timing_history_.DidStartDrawing();

    layer_tree_host_impl_->Animate(
        layer_tree_host_impl_->CurrentBeginFrameArgs().frame_time);
    UpdateBackgroundAnimateTicking();

    if (!layer_tree_host_impl_->IsContextLost()) {
      layer_tree_host_impl_->PrepareToDraw(frame);
      layer_tree_host_impl_->DrawLayers(frame, frame_begin_time);
      layer_tree_host_impl_->DidDrawAllLayers(*frame);
    }

    bool start_ready_animations = true;
    layer_tree_host_impl_->UpdateAnimationState(start_ready_animations);

    layer_tree_host_impl_->ResetCurrentBeginFrameArgsForNextFrame();

    timing_history_.DidFinishDrawing();
  }

  {
    DebugScopedSetImplThread impl(this);

    if (layer_tree_host_impl_->IsContextLost()) {
      DidLoseOutputSurfaceOnImplThread();
    } else {
      // This CapturePostTasks should be destroyed before
      // DidCommitAndDrawFrame() is called since that goes out to the
      // embedder,
      // and we want the embedder to receive its callbacks before that.
      // NOTE: This maintains consistent ordering with the ThreadProxy since
      // the DidCommitAndDrawFrame() must be post-tasked from the impl thread
      // there as the main thread is not blocked, so any posted tasks inside
      // the swap buffers will execute first.
      DebugScopedSetMainThreadBlocked main_thread_blocked(this);

      BlockingTaskRunner::CapturePostTasks blocked;
      layer_tree_host_impl_->SwapBuffers(*frame);
    }
  }
  DidCommitAndDrawFrame();

  return DRAW_SUCCESS;
}

void SingleThreadProxy::DidCommitAndDrawFrame() {
  if (next_frame_is_newly_committed_frame_) {
    DebugScopedSetMainThread main(this);
    next_frame_is_newly_committed_frame_ = false;
    layer_tree_host_->DidCommitAndDrawFrame();
  }
}

bool SingleThreadProxy::MainFrameWillHappenForTesting() {
  return false;
}

void SingleThreadProxy::SetNeedsBeginFrame(bool enable) {
  layer_tree_host_impl_->SetNeedsBeginFrame(enable);
}

void SingleThreadProxy::WillBeginImplFrame(const BeginFrameArgs& args) {
  layer_tree_host_impl_->WillBeginImplFrame(args);
}

void SingleThreadProxy::ScheduledActionSendBeginMainFrame() {
  TRACE_EVENT0("cc", "SingleThreadProxy::ScheduledActionSendBeginMainFrame");
  // Although this proxy is single-threaded, it's problematic to synchronously
  // have BeginMainFrame happen after ScheduledActionSendBeginMainFrame.  This
  // could cause a commit to occur in between a series of SetNeedsCommit calls
  // (i.e. property modifications) causing some to fall on one frame and some to
  // fall on the next.  Doing it asynchronously instead matches the semantics of
  // ThreadProxy::SetNeedsCommit where SetNeedsCommit will not cause a
  // synchronous commit.
  MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&SingleThreadProxy::BeginMainFrame,
                 weak_factory_.GetWeakPtr()));
}

void SingleThreadProxy::BeginMainFrame() {
  if (defer_commits_) {
    DCHECK(!commit_was_deferred_);
    commit_was_deferred_ = true;
    layer_tree_host_->DidDeferCommit();
    return;
  }

  // This checker assumes NotifyReadyToCommit below causes a synchronous commit.
  ScopedAbortRemainingSwapPromises swap_promise_checker(layer_tree_host_);

  if (!layer_tree_host_->visible()) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NotVisible", TRACE_EVENT_SCOPE_THREAD);
    BeginMainFrameAbortedOnImplThread();
    return;
  }

  if (layer_tree_host_->output_surface_lost()) {
    TRACE_EVENT_INSTANT0(
        "cc", "EarlyOut_OutputSurfaceLost", TRACE_EVENT_SCOPE_THREAD);
    BeginMainFrameAbortedOnImplThread();
    return;
  }

  timing_history_.DidBeginMainFrame();

  DCHECK(scheduler_on_impl_thread_);
  scheduler_on_impl_thread_->NotifyBeginMainFrameStarted();
  scheduler_on_impl_thread_->NotifyReadyToCommit();
}

void SingleThreadProxy::BeginMainFrameAbortedOnImplThread() {
  DebugScopedSetImplThread impl(this);
  DCHECK(scheduler_on_impl_thread_->CommitPending());
  DCHECK(!layer_tree_host_impl_->pending_tree());

  // TODO(enne): SingleThreadProxy does not support cancelling commits yet so
  // did_handle is always false.
  bool did_handle = false;
  layer_tree_host_impl_->BeginMainFrameAborted(did_handle);
  scheduler_on_impl_thread_->BeginMainFrameAborted(did_handle);
}

DrawResult SingleThreadProxy::ScheduledActionDrawAndSwapIfPossible() {
  DebugScopedSetImplThread impl(this);
  if (layer_tree_host_impl_->IsContextLost()) {
    DidCommitAndDrawFrame();
    return DRAW_SUCCESS;
  }

  LayerTreeHostImpl::FrameData frame;
  return DoComposite(layer_tree_host_impl_->CurrentBeginFrameArgs().frame_time,
                     &frame);
}

DrawResult SingleThreadProxy::ScheduledActionDrawAndSwapForced() {
  NOTREACHED();
  return INVALID_RESULT;
}

void SingleThreadProxy::ScheduledActionCommit() {
  DebugScopedSetMainThread main(this);
  DoCommit(layer_tree_host_impl_->CurrentBeginFrameArgs());
}

void SingleThreadProxy::ScheduledActionAnimate() {
  TRACE_EVENT0("cc", "ScheduledActionAnimate");
  layer_tree_host_impl_->Animate(
      layer_tree_host_impl_->CurrentBeginFrameArgs().frame_time);
}

void SingleThreadProxy::ScheduledActionUpdateVisibleTiles() {
  // Impl-side painting only.
  NOTREACHED();
}

void SingleThreadProxy::ScheduledActionActivateSyncTree() {
}

void SingleThreadProxy::ScheduledActionBeginOutputSurfaceCreation() {
  DebugScopedSetMainThread main(this);
  DCHECK(scheduler_on_impl_thread_);
  // If possible, create the output surface in a post task.  Synchronously
  // creating the output surface makes tests more awkward since this differs
  // from the ThreadProxy behavior.  However, sometimes there is no
  // task runner.
  if (Proxy::MainThreadTaskRunner()) {
    MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&SingleThreadProxy::CreateAndInitializeOutputSurface,
                   weak_factory_.GetWeakPtr()));
  } else {
    CreateAndInitializeOutputSurface();
  }
}

void SingleThreadProxy::ScheduledActionManageTiles() {
  // Impl-side painting only.
  NOTREACHED();
}

void SingleThreadProxy::DidAnticipatedDrawTimeChange(base::TimeTicks time) {
}

base::TimeDelta SingleThreadProxy::DrawDurationEstimate() {
  return timing_history_.DrawDurationEstimate();
}

base::TimeDelta SingleThreadProxy::BeginMainFrameToCommitDurationEstimate() {
  return timing_history_.BeginMainFrameToCommitDurationEstimate();
}

base::TimeDelta SingleThreadProxy::CommitToActivateDurationEstimate() {
  return timing_history_.CommitToActivateDurationEstimate();
}

void SingleThreadProxy::DidBeginImplFrameDeadline() {
  layer_tree_host_impl_->ResetCurrentBeginFrameArgsForNextFrame();
}

}  // namespace cc
