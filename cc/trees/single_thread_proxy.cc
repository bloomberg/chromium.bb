// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/single_thread_proxy.h"

#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "cc/base/thread.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/quads/draw_quad.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource_update_controller.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

scoped_ptr<Proxy> SingleThreadProxy::Create(LayerTreeHost* layer_tree_host) {
  return make_scoped_ptr(
      new SingleThreadProxy(layer_tree_host)).PassAs<Proxy>();
}

SingleThreadProxy::SingleThreadProxy(LayerTreeHost* layer_tree_host)
    : Proxy(scoped_ptr<Thread>(NULL)),
      layer_tree_host_(layer_tree_host),
      output_surface_lost_(false),
      created_offscreen_context_provider_(false),
      renderer_initialized_(false),
      next_frame_is_newly_committed_frame_(false),
      inside_draw_(false) {
  TRACE_EVENT0("cc", "SingleThreadProxy::SingleThreadProxy");
  DCHECK(Proxy::IsMainThread());
  DCHECK(layer_tree_host);

  // Impl-side painting not supported without threaded compositing.
  CHECK(!layer_tree_host->settings().impl_side_painting);
}

void SingleThreadProxy::Start() {
  DebugScopedSetImplThread impl(this);
  layer_tree_host_impl_ = layer_tree_host_->CreateLayerTreeHostImpl(this);
}

SingleThreadProxy::~SingleThreadProxy() {
  TRACE_EVENT0("cc", "SingleThreadProxy::~SingleThreadProxy");
  DCHECK(Proxy::IsMainThread());
  DCHECK(!layer_tree_host_impl_.get() &&
         !layer_tree_host_);  // make sure Stop() got called.
}

bool SingleThreadProxy::CompositeAndReadback(void* pixels, gfx::Rect rect) {
  TRACE_EVENT0("cc", "SingleThreadProxy::CompositeAndReadback");
  DCHECK(Proxy::IsMainThread());

  if (!CommitAndComposite(base::TimeTicks::Now()))
    return false;

  {
    DebugScopedSetImplThread impl(this);
    layer_tree_host_impl_->Readback(pixels, rect);

    if (layer_tree_host_impl_->IsContextLost())
      return false;

    layer_tree_host_impl_->SwapBuffers();
  }
  DidSwapFrame();

  return true;
}

void SingleThreadProxy::FinishAllRendering() {
  DCHECK(Proxy::IsMainThread());
  {
    DebugScopedSetImplThread impl(this);
    layer_tree_host_impl_->FinishAllRendering();
  }
}

bool SingleThreadProxy::IsStarted() const {
  DCHECK(Proxy::IsMainThread());
  return layer_tree_host_impl_.get();
}

bool SingleThreadProxy::InitializeOutputSurface() {
  DCHECK(Proxy::IsMainThread());
  scoped_ptr<OutputSurface> output_surface =
      layer_tree_host_->CreateOutputSurface();
  if (!output_surface.get())
    return false;
  output_surface_before_initialization_ = output_surface.Pass();
  return true;
}

void SingleThreadProxy::SetSurfaceReady() {
  // Scheduling is controlled by the embedder in the single thread case, so
  // nothing to do.
}

void SingleThreadProxy::SetVisible(bool visible) {
  DebugScopedSetImplThread impl(this);
  layer_tree_host_impl_->SetVisible(visible);
}

bool SingleThreadProxy::InitializeRenderer() {
  DCHECK(Proxy::IsMainThread());
  DCHECK(output_surface_before_initialization_.get());
  {
    DebugScopedSetImplThread impl(this);
    bool ok = layer_tree_host_impl_->InitializeRenderer(
        output_surface_before_initialization_.Pass());
    if (ok) {
      renderer_initialized_ = true;
      renderer_capabilities_for_main_thread_ =
          layer_tree_host_impl_->GetRendererCapabilities();
    }

    return ok;
  }
}

bool SingleThreadProxy::RecreateOutputSurface() {
  TRACE_EVENT0("cc", "SingleThreadProxy::RecreateContext");
  DCHECK(Proxy::IsMainThread());
  DCHECK(output_surface_lost_);

  scoped_ptr<OutputSurface> output_surface =
      layer_tree_host_->CreateOutputSurface();
  if (!output_surface.get())
    return false;
  scoped_refptr<cc::ContextProvider> offscreen_context_provider;
  if (created_offscreen_context_provider_) {
    offscreen_context_provider =
        layer_tree_host_->client()->OffscreenContextProviderForMainThread();
    if (!offscreen_context_provider)
      return false;
  }

  bool initialized;
  {
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    DebugScopedSetImplThread impl(this);
    layer_tree_host_->DeleteContentsTexturesOnImplThread(
        layer_tree_host_impl_->resource_provider());
    initialized =
        layer_tree_host_impl_->InitializeRenderer(output_surface.Pass());
    if (initialized) {
      renderer_capabilities_for_main_thread_ =
          layer_tree_host_impl_->GetRendererCapabilities();
      layer_tree_host_impl_->resource_provider()->
          set_offscreen_context_provider(offscreen_context_provider);
    } else if (offscreen_context_provider) {
      offscreen_context_provider->VerifyContexts();
    }
  }

  if (initialized)
    output_surface_lost_ = false;

  return initialized;
}

const RendererCapabilities& SingleThreadProxy::GetRendererCapabilities() const {
  DCHECK(renderer_initialized_);
  // Note: this gets called during the commit by the "impl" thread.
  return renderer_capabilities_for_main_thread_;
}

void SingleThreadProxy::SetNeedsAnimate() {
  // Thread-only feature.
  NOTREACHED();
}

void SingleThreadProxy::DoCommit(scoped_ptr<ResourceUpdateQueue> queue) {
  DCHECK(Proxy::IsMainThread());
  // Commit immediately.
  {
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    DebugScopedSetImplThread impl(this);

    RenderingStatsInstrumentation* stats_instrumentation =
        layer_tree_host_->rendering_stats_instrumentation();
    base::TimeTicks start_time = stats_instrumentation->StartRecording();

    layer_tree_host_impl_->BeginCommit();

    layer_tree_host_->contents_texture_manager()->
        PushTexturePrioritiesToBackings();
    layer_tree_host_->BeginCommitOnImplThread(layer_tree_host_impl_.get());

    scoped_ptr<ResourceUpdateController> update_controller =
        ResourceUpdateController::Create(
            NULL,
            Proxy::MainThread(),
            queue.Pass(),
            layer_tree_host_impl_->resource_provider());
    update_controller->Finalize();

    layer_tree_host_->FinishCommitOnImplThread(layer_tree_host_impl_.get());

    layer_tree_host_impl_->CommitComplete();

#ifndef NDEBUG
    // In the single-threaded case, the scroll deltas should never be
    // touched on the impl layer tree.
    scoped_ptr<ScrollAndScaleSet> scroll_info =
        layer_tree_host_impl_->ProcessScrollDeltas();
    DCHECK(!scroll_info->scrolls.size());
#endif

    base::TimeDelta duration = stats_instrumentation->EndRecording(start_time);
    stats_instrumentation->AddCommit(duration);
  }
  layer_tree_host_->CommitComplete();
  next_frame_is_newly_committed_frame_ = true;
}

void SingleThreadProxy::SetNeedsCommit() {
  DCHECK(Proxy::IsMainThread());
  layer_tree_host_->ScheduleComposite();
}

void SingleThreadProxy::SetNeedsRedraw() {
  // FIXME: Once we move render_widget scheduling into this class, we can
  // treat redraw requests more efficiently than CommitAndRedraw requests.
  layer_tree_host_impl_->SetFullRootLayerDamage();
  SetNeedsCommit();
}

void SingleThreadProxy::OnHasPendingTreeStateChanged(bool have_pending_tree) {
  // Thread-only feature.
  NOTREACHED();
}

void SingleThreadProxy::SetDeferCommits(bool defer_commits) {
  // Thread-only feature.
  NOTREACHED();
}

bool SingleThreadProxy::CommitRequested() const { return false; }

size_t SingleThreadProxy::MaxPartialTextureUpdates() const {
  return std::numeric_limits<size_t>::max();
}

void SingleThreadProxy::Stop() {
  TRACE_EVENT0("cc", "SingleThreadProxy::stop");
  DCHECK(Proxy::IsMainThread());
  {
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    DebugScopedSetImplThread impl(this);

    layer_tree_host_->DeleteContentsTexturesOnImplThread(
        layer_tree_host_impl_->resource_provider());
    layer_tree_host_impl_.reset();
  }
  layer_tree_host_ = NULL;
}

void SingleThreadProxy::SetNeedsRedrawOnImplThread() {
  layer_tree_host_->ScheduleComposite();
}

void SingleThreadProxy::DidInitializeVisibleTileOnImplThread() {
  // Impl-side painting only.
  NOTREACHED();
}

void SingleThreadProxy::SetNeedsCommitOnImplThread() {
  layer_tree_host_->ScheduleComposite();
}

void SingleThreadProxy::SetNeedsManageTilesOnImplThread() {
  layer_tree_host_->ScheduleComposite();
}

void SingleThreadProxy::PostAnimationEventsToMainThreadOnImplThread(
    scoped_ptr<AnimationEventsVector> events,
    base::Time wall_clock_time) {
  DCHECK(Proxy::IsImplThread());
  DebugScopedSetMainThread main(this);
  layer_tree_host_->SetAnimationEvents(events.Pass(), wall_clock_time);
}

bool SingleThreadProxy::ReduceContentsTextureMemoryOnImplThread(
    size_t limit_bytes,
    int priority_cutoff) {
  DCHECK(IsImplThread());
  if (!layer_tree_host_->contents_texture_manager())
    return false;

  return layer_tree_host_->contents_texture_manager()->ReduceMemoryOnImplThread(
      limit_bytes, priority_cutoff, layer_tree_host_impl_->resource_provider());
}

void SingleThreadProxy::ReduceWastedContentsTextureMemoryOnImplThread() {
  // Impl-side painting only.
  NOTREACHED();
}

void SingleThreadProxy::SendManagedMemoryStats() {
  DCHECK(Proxy::IsImplThread());
  if (!layer_tree_host_impl_.get())
    return;
  if (!layer_tree_host_->contents_texture_manager())
    return;

  PrioritizedResourceManager* contents_texture_manager =
      layer_tree_host_->contents_texture_manager();
  layer_tree_host_impl_->SendManagedMemoryStats(
      contents_texture_manager->MemoryVisibleBytes(),
      contents_texture_manager->MemoryVisibleAndNearbyBytes(),
      contents_texture_manager->MemoryUseBytes());
}

bool SingleThreadProxy::IsInsideDraw() { return inside_draw_; }

void SingleThreadProxy::DidLoseOutputSurfaceOnImplThread() {
  // Cause a commit so we can notice the lost context.
  SetNeedsCommitOnImplThread();
}

// Called by the legacy scheduling path (e.g. where render_widget does the
// scheduling)
void SingleThreadProxy::CompositeImmediately(base::TimeTicks frame_begin_time) {
  if (CommitAndComposite(frame_begin_time)) {
    layer_tree_host_impl_->SwapBuffers();
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
    if (renderer_initialized_)
      layer_tree_host_impl_->renderer()->DoNoOp();
  }
}

void SingleThreadProxy::OnSwapBuffersCompleteOnImplThread() { NOTREACHED(); }

bool SingleThreadProxy::CommitAndComposite(base::TimeTicks frame_begin_time) {
  DCHECK(Proxy::IsMainThread());

  if (!layer_tree_host_->InitializeRendererIfNeeded())
    return false;

  scoped_refptr<cc::ContextProvider> offscreen_context_provider;
  if (renderer_capabilities_for_main_thread_.using_offscreen_context3d &&
      layer_tree_host_->needs_offscreen_context()) {
    offscreen_context_provider =
        layer_tree_host_->client()->OffscreenContextProviderForMainThread();
    if (offscreen_context_provider)
      created_offscreen_context_provider_ = true;
  }

  layer_tree_host_->contents_texture_manager()->UnlinkAndClearEvictedBackings();

  scoped_ptr<ResourceUpdateQueue> queue =
      make_scoped_ptr(new ResourceUpdateQueue);
  layer_tree_host_->UpdateLayers(
      queue.get(), layer_tree_host_impl_->memory_allocation_limit_bytes());

  layer_tree_host_->WillCommit();
  DoCommit(queue.Pass());
  bool result = DoComposite(offscreen_context_provider, frame_begin_time);
  layer_tree_host_->DidBeginFrame();
  return result;
}

bool SingleThreadProxy::DoComposite(
    scoped_refptr<cc::ContextProvider> offscreen_context_provider,
    base::TimeTicks frame_begin_time) {
  DCHECK(!output_surface_lost_);
  {
    DebugScopedSetImplThread impl(this);
    base::AutoReset<bool> mark_inside(&inside_draw_, true);

    layer_tree_host_impl_->resource_provider()->
        set_offscreen_context_provider(offscreen_context_provider);

    if (!layer_tree_host_impl_->visible())
      return false;

    layer_tree_host_impl_->Animate(base::TimeTicks::Now(), base::Time::Now());

    // We guard prepareToDraw() with canDraw() because it always returns a valid
    // frame, so can only be used when such a frame is possible. Since
    // drawLayers() depends on the result of prepareToDraw(), it is guarded on
    // canDraw() as well.
    if (!layer_tree_host_impl_->CanDraw())
      return false;

    LayerTreeHostImpl::FrameData frame;
    layer_tree_host_impl_->PrepareToDraw(&frame);
    layer_tree_host_impl_->DrawLayers(&frame, frame_begin_time);
    layer_tree_host_impl_->DidDrawAllLayers(frame);
    output_surface_lost_ = layer_tree_host_impl_->IsContextLost();

    layer_tree_host_impl_->BeginNextFrame();
  }

  if (output_surface_lost_) {
    cc::ContextProvider* offscreen_contexts = layer_tree_host_impl_->
        resource_provider()->offscreen_context_provider();
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

skia::RefPtr<SkPicture> SingleThreadProxy::CapturePicture() {
  // Impl-side painting only.
  NOTREACHED();
  return skia::RefPtr<SkPicture>();
}

}  // namespace cc
