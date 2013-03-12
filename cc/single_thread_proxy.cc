// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/single_thread_proxy.h"

#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "cc/context_provider.h"
#include "cc/draw_quad.h"
#include "cc/layer_tree_host.h"
#include "cc/layer_tree_impl.h"
#include "cc/output_surface.h"
#include "cc/prioritized_resource_manager.h"
#include "cc/resource_update_controller.h"
#include "cc/thread.h"

namespace cc {

scoped_ptr<Proxy> SingleThreadProxy::Create(LayerTreeHost* layer_tree_host) {
  return make_scoped_ptr(
      new SingleThreadProxy(layer_tree_host)).PassAs<Proxy>();
}

SingleThreadProxy::SingleThreadProxy(LayerTreeHost* layer_tree_host)
    : Proxy(scoped_ptr<Thread>(NULL)),
      layer_tree_host_(layer_tree_host),
      output_surface_lost_(false),
      created_offscreen_context_provider(false),
      renderer_initialized_(false),
      next_frame_is_newly_committed_frame_(false),
      inside_draw_(false),
      total_commit_count_(0) {
  TRACE_EVENT0("cc", "SingleThreadProxy::SingleThreadProxy");
  DCHECK(Proxy::IsMainThread());
  DCHECK(layer_tree_host);

  // Impl-side painting not supported without threaded compositing
  DCHECK(!layer_tree_host->settings().implSidePainting);
}

void SingleThreadProxy::Start() {
  DebugScopedSetImplThread impl(this);
  layer_tree_host_impl_ = layer_tree_host_->createLayerTreeHostImpl(this);
}

SingleThreadProxy::~SingleThreadProxy() {
  TRACE_EVENT0("cc", "SingleThreadProxy::~SingleThreadProxy");
  DCHECK(Proxy::IsMainThread());
  DCHECK(!layer_tree_host_impl_.get() &&
         !layer_tree_host_);  // make sure Stop() got called.
}

bool SingleThreadProxy::CompositeAndReadback(void* pixels, gfx::Rect rect) {
  TRACE_EVENT0("cc", "SingleThreadProxy::compositeAndReadback");
  DCHECK(Proxy::IsMainThread());

  if (!CommitAndComposite())
    return false;

  {
    DebugScopedSetImplThread impl(this);
    layer_tree_host_impl_->readback(pixels, rect);

    if (layer_tree_host_impl_->isContextLost())
      return false;

    layer_tree_host_impl_->swapBuffers();
  }
  DidSwapFrame();

  return true;
}

void SingleThreadProxy::StartPageScaleAnimation(gfx::Vector2d target_offset,
                                                bool use_anchor,
                                                float scale,
                                                base::TimeDelta duration) {
  layer_tree_host_impl_->startPageScaleAnimation(
      target_offset, use_anchor, scale, base::TimeTicks::Now(), duration);
}

void SingleThreadProxy::FinishAllRendering() {
  DCHECK(Proxy::IsMainThread());
  {
    DebugScopedSetImplThread impl(this);
    layer_tree_host_impl_->finishAllRendering();
  }
}

bool SingleThreadProxy::IsStarted() const {
  DCHECK(Proxy::IsMainThread());
  return layer_tree_host_impl_.get();
}

bool SingleThreadProxy::InitializeOutputSurface() {
  DCHECK(Proxy::IsMainThread());
  scoped_ptr<OutputSurface> outputSurface =
      layer_tree_host_->createOutputSurface();
  if (!outputSurface.get())
    return false;
  output_surface_before_initialization_ = outputSurface.Pass();
  return true;
}

void SingleThreadProxy::SetSurfaceReady() {
  // Scheduling is controlled by the embedder in the single thread case, so
  // nothing to do.
}

void SingleThreadProxy::SetVisible(bool visible) {
  DebugScopedSetImplThread impl(this);
  layer_tree_host_impl_->setVisible(visible);
}

bool SingleThreadProxy::InitializeRenderer() {
  DCHECK(Proxy::IsMainThread());
  DCHECK(output_surface_before_initialization_.get());
  {
    DebugScopedSetImplThread impl(this);
    bool ok = layer_tree_host_impl_->initializeRenderer(
        output_surface_before_initialization_.Pass());
    if (ok) {
      renderer_initialized_ = true;
      renderer_capabilities_for_main_thread_ =
          layer_tree_host_impl_->rendererCapabilities();
    }

    return ok;
  }
}

bool SingleThreadProxy::RecreateOutputSurface() {
  TRACE_EVENT0("cc", "SingleThreadProxy::recreateContext");
  DCHECK(Proxy::IsMainThread());
  DCHECK(output_surface_lost_);

  scoped_ptr<OutputSurface> outputSurface =
      layer_tree_host_->createOutputSurface();
  if (!outputSurface.get())
    return false;
  scoped_refptr<cc::ContextProvider> offscreen_context_provider;
  if (created_offscreen_context_provider) {
    offscreen_context_provider =
        layer_tree_host_->client()->OffscreenContextProviderForMainThread();
    if (!offscreen_context_provider->InitializeOnMainThread())
      return false;
  }

  bool initialized;
  {
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    DebugScopedSetImplThread impl(this);
    layer_tree_host_->deleteContentsTexturesOnImplThread(
        layer_tree_host_impl_->resourceProvider());
    initialized =
        layer_tree_host_impl_->initializeRenderer(outputSurface.Pass());
    if (initialized) {
      renderer_capabilities_for_main_thread_ =
          layer_tree_host_impl_->rendererCapabilities();
      layer_tree_host_impl_->resourceProvider()
          ->SetOffscreenContextProvider(offscreen_context_provider);
    } else if (offscreen_context_provider) {
      offscreen_context_provider->VerifyContexts();
    }
  }

  if (initialized)
    output_surface_lost_ = false;

  return initialized;
}

void SingleThreadProxy::GetRenderingStats(RenderingStats* stats) {
  stats->totalCommitTime = total_commit_time_;
  stats->totalCommitCount = total_commit_count_;
  layer_tree_host_impl_->renderingStats(stats);
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

    base::TimeTicks startTime = base::TimeTicks::HighResNow();
    layer_tree_host_impl_->beginCommit();

    layer_tree_host_->contentsTextureManager()->
        pushTexturePrioritiesToBackings();
    layer_tree_host_->beginCommitOnImplThread(layer_tree_host_impl_.get());

    scoped_ptr<ResourceUpdateController> updateController =
        ResourceUpdateController::Create(
            NULL,
            Proxy::MainThread(),
            queue.Pass(),
            layer_tree_host_impl_->resourceProvider());
    updateController->Finalize();

    layer_tree_host_->finishCommitOnImplThread(layer_tree_host_impl_.get());

    layer_tree_host_impl_->commitComplete();

#ifndef NDEBUG
    // In the single-threaded case, the scroll deltas should never be
    // touched on the impl layer tree.
    scoped_ptr<ScrollAndScaleSet> scrollInfo =
        layer_tree_host_impl_->processScrollDeltas();
    DCHECK(!scrollInfo->scrolls.size());
#endif

    base::TimeTicks endTime = base::TimeTicks::HighResNow();
    total_commit_time_ += endTime - startTime;
    total_commit_count_++;
  }
  layer_tree_host_->commitComplete();
  next_frame_is_newly_committed_frame_ = true;
}

void SingleThreadProxy::SetNeedsCommit() {
  DCHECK(Proxy::IsMainThread());
  layer_tree_host_->scheduleComposite();
}

void SingleThreadProxy::SetNeedsRedraw() {
  // FIXME: Once we move render_widget scheduling into this class, we can
  // treat redraw requests more efficiently than commitAndRedraw requests.
  layer_tree_host_impl_->SetFullRootLayerDamage();
  SetNeedsCommit();
}

void SingleThreadProxy::onHasPendingTreeStateChanged(bool have_pending_tree) {
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

    layer_tree_host_->deleteContentsTexturesOnImplThread(
        layer_tree_host_impl_->resourceProvider());
    layer_tree_host_impl_.reset();
  }
  layer_tree_host_ = NULL;
}

void SingleThreadProxy::setNeedsRedrawOnImplThread() {
  layer_tree_host_->scheduleComposite();
}

void SingleThreadProxy::didUploadVisibleHighResolutionTileOnImplThread() {
  // Impl-side painting only.
  NOTREACHED();
}

void SingleThreadProxy::setNeedsCommitOnImplThread() {
  layer_tree_host_->scheduleComposite();
}

void SingleThreadProxy::setNeedsManageTilesOnImplThread() {
  layer_tree_host_->scheduleComposite();
}

void SingleThreadProxy::postAnimationEventsToMainThreadOnImplThread(
    scoped_ptr<AnimationEventsVector> events,
    base::Time wall_clock_time) {
  DCHECK(Proxy::IsImplThread());
  DebugScopedSetMainThread main(this);
  layer_tree_host_->setAnimationEvents(events.Pass(), wall_clock_time);
}

bool SingleThreadProxy::reduceContentsTextureMemoryOnImplThread(
    size_t limit_bytes,
    int priority_cutoff) {
  DCHECK(IsImplThread());
  if (!layer_tree_host_->contentsTextureManager())
    return false;

  return layer_tree_host_->contentsTextureManager()->reduceMemoryOnImplThread(
      limit_bytes, priority_cutoff, layer_tree_host_impl_->resourceProvider());
}

void SingleThreadProxy::reduceWastedContentsTextureMemoryOnImplThread() {
  // Impl-side painting only.
  NOTREACHED();
}

void SingleThreadProxy::sendManagedMemoryStats() {
  DCHECK(Proxy::IsImplThread());
  if (!layer_tree_host_impl_.get())
    return;
  if (!layer_tree_host_->contentsTextureManager())
    return;

  layer_tree_host_impl_->sendManagedMemoryStats(
      layer_tree_host_->contentsTextureManager()->memoryVisibleBytes(),
      layer_tree_host_->contentsTextureManager()->memoryVisibleAndNearbyBytes(),
      layer_tree_host_->contentsTextureManager()->memoryUseBytes());
}

bool SingleThreadProxy::isInsideDraw() { return inside_draw_; }

void SingleThreadProxy::didLoseOutputSurfaceOnImplThread() {
  // Cause a commit so we can notice the lost context.
  setNeedsCommitOnImplThread();
}

// Called by the legacy scheduling path (e.g. where render_widget does the
// scheduling)
void SingleThreadProxy::CompositeImmediately() {
  if (CommitAndComposite()) {
    layer_tree_host_impl_->swapBuffers();
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
               layer_tree_host_impl_->asValue().release());
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

void SingleThreadProxy::onSwapBuffersCompleteOnImplThread() { NOTREACHED(); }

bool SingleThreadProxy::CommitAndComposite() {
  DCHECK(Proxy::IsMainThread());

  if (!layer_tree_host_->initializeRendererIfNeeded())
    return false;

  scoped_refptr<cc::ContextProvider> offscreen_context_provider;
  if (renderer_capabilities_for_main_thread_.usingOffscreenContext3d &&
      layer_tree_host_->needsOffscreenContext()) {
    offscreen_context_provider =
        layer_tree_host_->client()->OffscreenContextProviderForMainThread();
    if (offscreen_context_provider->InitializeOnMainThread())
      created_offscreen_context_provider = true;
    else
      offscreen_context_provider = NULL;
  }

  layer_tree_host_->contentsTextureManager()->unlinkAndClearEvictedBackings();

  scoped_ptr<ResourceUpdateQueue> queue =
      make_scoped_ptr(new ResourceUpdateQueue);
  layer_tree_host_->updateLayers(
      *(queue.get()), layer_tree_host_impl_->memoryAllocationLimitBytes());

  layer_tree_host_->willCommit();
  DoCommit(queue.Pass());
  bool result = DoComposite(offscreen_context_provider);
  layer_tree_host_->didBeginFrame();
  return result;
}

bool SingleThreadProxy::DoComposite(
    scoped_refptr<cc::ContextProvider> offscreen_context_provider) {
  DCHECK(!output_surface_lost_);
  {
    DebugScopedSetImplThread impl(this);
    base::AutoReset<bool> mark_inside(&inside_draw_, true);

    layer_tree_host_impl_->resourceProvider()->
        SetOffscreenContextProvider(offscreen_context_provider);

    if (!layer_tree_host_impl_->visible())
      return false;

    layer_tree_host_impl_->animate(base::TimeTicks::Now(), base::Time::Now());

    // We guard prepareToDraw() with canDraw() because it always returns a valid
    // frame, so can only be used when such a frame is possible. Since
    // drawLayers() depends on the result of prepareToDraw(), it is guarded on
    // canDraw() as well.
    if (!layer_tree_host_impl_->canDraw())
      return false;

    LayerTreeHostImpl::FrameData frame;
    layer_tree_host_impl_->prepareToDraw(frame);
    layer_tree_host_impl_->drawLayers(frame);
    layer_tree_host_impl_->didDrawAllLayers(frame);
    output_surface_lost_ = layer_tree_host_impl_->isContextLost();

    layer_tree_host_impl_->beginNextFrame();
  }

  if (output_surface_lost_) {
    cc::ContextProvider* offscreen_contexts = layer_tree_host_impl_->
        resourceProvider()->offscreen_context_provider();
    if (offscreen_contexts)
      offscreen_contexts->VerifyContexts();
    layer_tree_host_->didLoseOutputSurface();
    return false;
  }

  return true;
}

void SingleThreadProxy::DidSwapFrame() {
  if (next_frame_is_newly_committed_frame_) {
    next_frame_is_newly_committed_frame_ = false;
    layer_tree_host_->didCommitAndDrawFrame();
  }
}

bool SingleThreadProxy::CommitPendingForTesting() { return false; }

skia::RefPtr<SkPicture> SingleThreadProxy::CapturePicture() {
  // Impl-side painting only.
  NOTREACHED();
  return skia::RefPtr<SkPicture>();
}

}  // namespace cc
