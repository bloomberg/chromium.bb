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
#include "ui/gfx/frame_time.h"

namespace cc {

scoped_ptr<Proxy> SingleThreadProxy::Create(
    LayerTreeHost* layer_tree_host,
    LayerTreeHostSingleThreadClient* client) {
  return make_scoped_ptr(
      new SingleThreadProxy(layer_tree_host, client)).PassAs<Proxy>();
}

SingleThreadProxy::SingleThreadProxy(LayerTreeHost* layer_tree_host,
                                     LayerTreeHostSingleThreadClient* client)
    : Proxy(NULL),
      layer_tree_host_(layer_tree_host),
      client_(client),
      created_offscreen_context_provider_(false),
      next_frame_is_newly_committed_frame_(false),
      inside_draw_(false) {
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

bool SingleThreadProxy::CompositeAndReadback(void* pixels,
                                             const gfx::Rect& rect) {
  TRACE_EVENT0("cc", "SingleThreadProxy::CompositeAndReadback");
  DCHECK(Proxy::IsMainThread());

  gfx::Rect device_viewport_damage_rect = rect;

  LayerTreeHostImpl::FrameData frame;
  if (!CommitAndComposite(gfx::FrameTime::Now(),
                          device_viewport_damage_rect,
                          true,  // for_readback
                          &frame))
    return false;

  {
    DebugScopedSetImplThread impl(this);
    layer_tree_host_impl_->Readback(pixels, rect);

    if (layer_tree_host_impl_->IsContextLost())
      return false;
  }

  return true;
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
}

void SingleThreadProxy::SetVisible(bool visible) {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetVisible");
  DebugScopedSetImplThread impl(this);
  layer_tree_host_impl_->SetVisible(visible);

  // Changing visibility could change ShouldComposite().
  UpdateBackgroundAnimateTicking();
}

void SingleThreadProxy::CreateAndInitializeOutputSurface() {
  TRACE_EVENT0(
      "cc", "SingleThreadProxy::CreateAndInitializeOutputSurface");
  DCHECK(Proxy::IsMainThread());

  scoped_ptr<OutputSurface> output_surface =
      layer_tree_host_->CreateOutputSurface();
  if (!output_surface) {
    OnOutputSurfaceInitializeAttempted(false);
    return;
  }

  scoped_refptr<ContextProvider> offscreen_context_provider;
  if (created_offscreen_context_provider_) {
    offscreen_context_provider =
        layer_tree_host_->client()->OffscreenContextProvider();
    if (!offscreen_context_provider.get() ||
        !offscreen_context_provider->BindToCurrentThread()) {
      OnOutputSurfaceInitializeAttempted(false);
      return;
    }
  }

  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    DebugScopedSetImplThread impl(this);
    layer_tree_host_->DeleteContentsTexturesOnImplThread(
        layer_tree_host_impl_->resource_provider());
  }

  bool initialized;
  {
    DebugScopedSetImplThread impl(this);

    DCHECK(output_surface);
    initialized = layer_tree_host_impl_->InitializeRenderer(
        output_surface.Pass());
    if (!initialized && offscreen_context_provider.get()) {
      offscreen_context_provider->VerifyContexts();
      offscreen_context_provider = NULL;
    }

    layer_tree_host_impl_->SetOffscreenContextProvider(
        offscreen_context_provider);
  }

  OnOutputSurfaceInitializeAttempted(initialized);
}

void SingleThreadProxy::OnOutputSurfaceInitializeAttempted(bool success) {
  LayerTreeHost::CreateResult result =
      layer_tree_host_->OnCreateAndInitializeOutputSurfaceAttempted(success);
  if (result == LayerTreeHost::CreateFailedButTryAgain) {
    // Force another recreation attempt to happen by requesting another commit.
    SetNeedsCommit();
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
}

void SingleThreadProxy::SetNeedsUpdateLayers() {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetNeedsUpdateLayers");
  DCHECK(Proxy::IsMainThread());
  client_->ScheduleComposite();
}

void SingleThreadProxy::DoCommit(scoped_ptr<ResourceUpdateQueue> queue) {
  TRACE_EVENT0("cc", "SingleThreadProxy::DoCommit");
  DCHECK(Proxy::IsMainThread());
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
            Proxy::MainThreadTaskRunner(),
            queue.Pass(),
            layer_tree_host_impl_->resource_provider());
    update_controller->Finalize();

    if (layer_tree_host_impl_->EvictedUIResourcesExist())
      layer_tree_host_->RecreateUIResources();

    layer_tree_host_->FinishCommitOnImplThread(layer_tree_host_impl_.get());

    layer_tree_host_impl_->CommitComplete();

#ifndef NDEBUG
    // In the single-threaded case, the scale and scroll deltas should never be
    // touched on the impl layer tree.
    scoped_ptr<ScrollAndScaleSet> scroll_info =
        layer_tree_host_impl_->ProcessScrollDeltas();
    DCHECK(!scroll_info->scrolls.size());
    DCHECK_EQ(1.f, scroll_info->page_scale_delta);
#endif

    RenderingStatsInstrumentation* stats_instrumentation =
        layer_tree_host_->rendering_stats_instrumentation();
    BenchmarkInstrumentation::IssueMainThreadRenderingStatsEvent(
        stats_instrumentation->main_thread_rendering_stats());
    stats_instrumentation->AccumulateAndClearMainThreadStats();
  }
  layer_tree_host_->CommitComplete();
  next_frame_is_newly_committed_frame_ = true;
}

void SingleThreadProxy::SetNeedsCommit() {
  DCHECK(Proxy::IsMainThread());
  client_->ScheduleComposite();
}

void SingleThreadProxy::SetNeedsRedraw(const gfx::Rect& damage_rect) {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetNeedsRedraw");
  SetNeedsRedrawRectOnImplThread(damage_rect);
  client_->ScheduleComposite();
}

void SingleThreadProxy::SetNextCommitWaitsForActivation() {
  // There is no activation here other than commit. So do nothing.
}

void SingleThreadProxy::SetDeferCommits(bool defer_commits) {
  // Thread-only feature.
  NOTREACHED();
}

bool SingleThreadProxy::CommitRequested() const { return false; }

bool SingleThreadProxy::BeginMainFrameRequested() const { return false; }

size_t SingleThreadProxy::MaxPartialTextureUpdates() const {
  return std::numeric_limits<size_t>::max();
}

void SingleThreadProxy::Stop() {
  TRACE_EVENT0("cc", "SingleThreadProxy::stop");
  DCHECK(Proxy::IsMainThread());
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    DebugScopedSetImplThread impl(this);

    layer_tree_host_->DeleteContentsTexturesOnImplThread(
        layer_tree_host_impl_->resource_provider());
    layer_tree_host_impl_.reset();
  }
  layer_tree_host_ = NULL;
}

void SingleThreadProxy::OnCanDrawStateChanged(bool can_draw) {
  TRACE_EVENT1(
      "cc", "SingleThreadProxy::OnCanDrawStateChanged", "can_draw", can_draw);
  DCHECK(Proxy::IsImplThread());
  UpdateBackgroundAnimateTicking();
}

void SingleThreadProxy::NotifyReadyToActivate() {
  // Thread-only feature.
  NOTREACHED();
}

void SingleThreadProxy::SetNeedsRedrawOnImplThread() {
  client_->ScheduleComposite();
}

void SingleThreadProxy::SetNeedsManageTilesOnImplThread() {
  // Thread-only/Impl-side-painting-only feature.
  NOTREACHED();
}

void SingleThreadProxy::SetNeedsRedrawRectOnImplThread(
    const gfx::Rect& damage_rect) {
  // TODO(brianderson): Once we move render_widget scheduling into this class,
  // we can treat redraw requests more efficiently than CommitAndRedraw
  // requests.
  layer_tree_host_impl_->SetViewportDamage(damage_rect);
  SetNeedsCommit();
}

void SingleThreadProxy::DidInitializeVisibleTileOnImplThread() {
  // Impl-side painting only.
  NOTREACHED();
}

void SingleThreadProxy::SetNeedsCommitOnImplThread() {
  client_->ScheduleComposite();
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

void SingleThreadProxy::SendManagedMemoryStats() {
  DCHECK(Proxy::IsImplThread());
  if (!layer_tree_host_impl_)
    return;
  PrioritizedResourceManager* contents_texture_manager =
      layer_tree_host_->contents_texture_manager();
  if (!contents_texture_manager)
    return;

  layer_tree_host_impl_->SendManagedMemoryStats(
      contents_texture_manager->MemoryVisibleBytes(),
      contents_texture_manager->MemoryVisibleAndNearbyBytes(),
      contents_texture_manager->MemoryUseBytes());
}

bool SingleThreadProxy::IsInsideDraw() { return inside_draw_; }

void SingleThreadProxy::UpdateRendererCapabilitiesOnImplThread() {
  DCHECK(IsImplThread());
  renderer_capabilities_for_main_thread_ =
      layer_tree_host_impl_->GetRendererCapabilities().MainThreadCapabilities();
}

void SingleThreadProxy::DidLoseOutputSurfaceOnImplThread() {
  TRACE_EVENT0("cc", "SingleThreadProxy::DidLoseOutputSurfaceOnImplThread");
  // Cause a commit so we can notice the lost context.
  SetNeedsCommitOnImplThread();
  client_->DidAbortSwapBuffers();
}

void SingleThreadProxy::DidSwapBuffersOnImplThread() {
  client_->DidPostSwapBuffers();
}

void SingleThreadProxy::OnSwapBuffersCompleteOnImplThread() {
  TRACE_EVENT0("cc", "SingleThreadProxy::OnSwapBuffersCompleteOnImplThread");
  client_->DidCompleteSwapBuffers();
}

// Called by the legacy scheduling path (e.g. where render_widget does the
// scheduling)
void SingleThreadProxy::CompositeImmediately(base::TimeTicks frame_begin_time) {
  TRACE_EVENT0("cc", "SingleThreadProxy::CompositeImmediately");
  gfx::Rect device_viewport_damage_rect;

  LayerTreeHostImpl::FrameData frame;
  if (CommitAndComposite(frame_begin_time,
                         device_viewport_damage_rect,
                         false,  // for_readback
                         &frame)) {
    {
      DebugScopedSetMainThreadBlocked main_thread_blocked(this);
      DebugScopedSetImplThread impl(this);

      // This CapturePostTasks should be destroyed before
      // DidCommitAndDrawFrame() is called since that goes out to the embedder,
      // and we want the embedder to receive its callbacks before that.
      // NOTE: This maintains consistent ordering with the ThreadProxy since
      // the DidCommitAndDrawFrame() must be post-tasked from the impl thread
      // there as the main thread is not blocked, so any posted tasks inside
      // the swap buffers will execute first.
      BlockingTaskRunner::CapturePostTasks blocked;

      layer_tree_host_impl_->SwapBuffers(frame);
    }
    DidSwapFrame();
  }
}

scoped_ptr<base::Value> SingleThreadProxy::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  {
    // The following line casts away const modifiers because it is just
    // setting debug state. We still want the AsValue() function and its
    // call chain to be const throughout.
    DebugScopedSetImplThread impl(const_cast<SingleThreadProxy*>(this));

    state->Set("layer_tree_host_impl",
               layer_tree_host_impl_->AsValue().release());
  }
  return state.PassAs<base::Value>();
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

bool SingleThreadProxy::CommitAndComposite(
    base::TimeTicks frame_begin_time,
    const gfx::Rect& device_viewport_damage_rect,
    bool for_readback,
    LayerTreeHostImpl::FrameData* frame) {
  TRACE_EVENT0("cc", "SingleThreadProxy::CommitAndComposite");
  DCHECK(Proxy::IsMainThread());

  if (!layer_tree_host_->InitializeOutputSurfaceIfNeeded())
    return false;

  layer_tree_host_->AnimateLayers(frame_begin_time);

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

  scoped_refptr<ContextProvider> offscreen_context_provider;
  if (renderer_capabilities_for_main_thread_.using_offscreen_context3d &&
      layer_tree_host_->needs_offscreen_context()) {
    offscreen_context_provider =
        layer_tree_host_->client()->OffscreenContextProvider();
    if (offscreen_context_provider.get() &&
        !offscreen_context_provider->BindToCurrentThread())
      offscreen_context_provider = NULL;

    if (offscreen_context_provider.get())
      created_offscreen_context_provider_ = true;
  }

  DoCommit(queue.Pass());
  bool result = DoComposite(offscreen_context_provider,
                            frame_begin_time,
                            device_viewport_damage_rect,
                            for_readback,
                            frame);
  layer_tree_host_->DidBeginMainFrame();
  return result;
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

bool SingleThreadProxy::DoComposite(
    scoped_refptr<ContextProvider> offscreen_context_provider,
    base::TimeTicks frame_begin_time,
    const gfx::Rect& device_viewport_damage_rect,
    bool for_readback,
    LayerTreeHostImpl::FrameData* frame) {
  TRACE_EVENT0("cc", "SingleThreadProxy::DoComposite");
  DCHECK(!layer_tree_host_->output_surface_lost());

  bool lost_output_surface = false;
  {
    DebugScopedSetImplThread impl(this);
    base::AutoReset<bool> mark_inside(&inside_draw_, true);

    layer_tree_host_impl_->SetOffscreenContextProvider(
        offscreen_context_provider);

    bool can_do_readback = layer_tree_host_impl_->renderer()->CanReadPixels();

    // We guard PrepareToDraw() with CanDraw() because it always returns a valid
    // frame, so can only be used when such a frame is possible. Since
    // DrawLayers() depends on the result of PrepareToDraw(), it is guarded on
    // CanDraw() as well.
    if (!ShouldComposite() || (for_readback && !can_do_readback)) {
      UpdateBackgroundAnimateTicking();
      return false;
    }

    layer_tree_host_impl_->Animate(
        layer_tree_host_impl_->CurrentFrameTimeTicks(),
        layer_tree_host_impl_->CurrentFrameTime());
    UpdateBackgroundAnimateTicking();

    if (!layer_tree_host_impl_->IsContextLost()) {
      layer_tree_host_impl_->PrepareToDraw(frame, device_viewport_damage_rect);
      layer_tree_host_impl_->DrawLayers(frame, frame_begin_time);
      layer_tree_host_impl_->DidDrawAllLayers(*frame);
    }
    lost_output_surface = layer_tree_host_impl_->IsContextLost();

    bool start_ready_animations = true;
    layer_tree_host_impl_->UpdateAnimationState(start_ready_animations);

    layer_tree_host_impl_->ResetCurrentFrameTimeForNextFrame();
  }

  if (lost_output_surface) {
    ContextProvider* offscreen_contexts =
        layer_tree_host_impl_->offscreen_context_provider();
    if (offscreen_contexts)
      offscreen_contexts->VerifyContexts();
    layer_tree_host_->DidLoseOutputSurface();
    return false;
  }

  return true;
}

void SingleThreadProxy::DidSwapFrame() {
  if (next_frame_is_newly_committed_frame_) {
    next_frame_is_newly_committed_frame_ = false;
    layer_tree_host_->DidCommitAndDrawFrame();
  }
}

bool SingleThreadProxy::CommitPendingForTesting() { return false; }

}  // namespace cc
