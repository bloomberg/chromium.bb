// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host_impl.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "cc/append_quads_data.h"
#include "cc/compositor_frame_metadata.h"
#include "cc/damage_tracker.h"
#include "cc/debug_rect_history.h"
#include "cc/delay_based_time_source.h"
#include "cc/delegating_renderer.h"
#include "cc/frame_rate_counter.h"
#include "cc/gl_renderer.h"
#include "cc/heads_up_display_layer_impl.h"
#include "cc/layer_iterator.h"
#include "cc/layer_tree_host.h"
#include "cc/layer_tree_host_common.h"
#include "cc/layer_tree_impl.h"
#include "cc/math_util.h"
#include "cc/overdraw_metrics.h"
#include "cc/page_scale_animation.h"
#include "cc/paint_time_counter.h"
#include "cc/prioritized_resource_manager.h"
#include "cc/quad_culler.h"
#include "cc/render_pass_draw_quad.h"
#include "cc/rendering_stats.h"
#include "cc/scrollbar_animation_controller.h"
#include "cc/scrollbar_layer_impl.h"
#include "cc/shared_quad_state.h"
#include "cc/single_thread_proxy.h"
#include "cc/software_renderer.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/texture_uploader.h"
#include "cc/top_controls_manager.h"
#include "cc/util.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

namespace {

void didVisibilityChange(cc::LayerTreeHostImpl* id, bool visible)
{
    if (visible) {
        TRACE_EVENT_ASYNC_BEGIN1("webkit", "LayerTreeHostImpl::setVisible", id, "LayerTreeHostImpl", id);
        return;
    }

    TRACE_EVENT_ASYNC_END0("webkit", "LayerTreeHostImpl::setVisible", id);
}

} // namespace

namespace cc {

class LayerTreeHostImplTimeSourceAdapter : public TimeSourceClient {
public:
    static scoped_ptr<LayerTreeHostImplTimeSourceAdapter> create(LayerTreeHostImpl* layerTreeHostImpl, scoped_refptr<DelayBasedTimeSource> timeSource)
    {
        return make_scoped_ptr(new LayerTreeHostImplTimeSourceAdapter(layerTreeHostImpl, timeSource));
    }
    virtual ~LayerTreeHostImplTimeSourceAdapter()
    {
        m_timeSource->setClient(0);
        m_timeSource->setActive(false);
    }

    virtual void onTimerTick() OVERRIDE
    {
        // In single threaded mode we attempt to simulate changing the current
        // thread by maintaining a fake thread id. When we switch from one
        // thread to another, we construct DebugScopedSetXXXThread objects that
        // update the thread id. This lets DCHECKS that ensure we're on the
        // right thread to work correctly in single threaded mode. The problem
        // here is that the timer tasks are run via the message loop, and when
        // they run, we've had no chance to construct a DebugScopedSetXXXThread
        // object. The result is that we report that we're running on the main
        // thread. In multi-threaded mode, this timer is run on the compositor
        // thread, so to keep this consistent in single-threaded mode, we'll
        // construct a DebugScopedSetImplThread object. There is no need to do
        // this in multi-threaded mode since the real thread id's will be
        // correct. In fact, setting fake thread id's interferes with the real
        // thread id's and causes breakage.
        scoped_ptr<DebugScopedSetImplThread> setImplThread;
        if (!m_layerTreeHostImpl->proxy()->hasImplThread())
            setImplThread.reset(new DebugScopedSetImplThread(m_layerTreeHostImpl->proxy()));

        m_layerTreeHostImpl->activatePendingTreeIfNeeded();
        m_layerTreeHostImpl->animate(base::TimeTicks::Now(), base::Time::Now());
    }

    void setActive(bool active)
    {
        if (active != m_timeSource->active())
            m_timeSource->setActive(active);
    }

private:
    LayerTreeHostImplTimeSourceAdapter(LayerTreeHostImpl* layerTreeHostImpl, scoped_refptr<DelayBasedTimeSource> timeSource)
        : m_layerTreeHostImpl(layerTreeHostImpl)
        , m_timeSource(timeSource)
    {
        m_timeSource->setClient(this);
    }

    LayerTreeHostImpl* m_layerTreeHostImpl;
    scoped_refptr<DelayBasedTimeSource> m_timeSource;

    DISALLOW_COPY_AND_ASSIGN(LayerTreeHostImplTimeSourceAdapter);
};

LayerTreeHostImpl::FrameData::FrameData()
{
}

LayerTreeHostImpl::FrameData::~FrameData()
{
}

scoped_ptr<LayerTreeHostImpl> LayerTreeHostImpl::create(const LayerTreeSettings& settings, LayerTreeHostImplClient* client, Proxy* proxy)
{
    return make_scoped_ptr(new LayerTreeHostImpl(settings, client, proxy));
}

LayerTreeHostImpl::LayerTreeHostImpl(const LayerTreeSettings& settings, LayerTreeHostImplClient* client, Proxy* proxy)
    : m_client(client)
    , m_proxy(proxy)
    , m_scrollDeltaIsInViewportSpace(false)
    , m_settings(settings)
    , m_debugState(settings.initialDebugState)
    , m_deviceScaleFactor(1)
    , m_visible(true)
    , m_managedMemoryPolicy(PrioritizedResourceManager::defaultMemoryAllocationLimit(),
                            ManagedMemoryPolicy::CUTOFF_ALLOW_EVERYTHING,
                            0,
                            ManagedMemoryPolicy::CUTOFF_ALLOW_NOTHING)
    , m_needsUpdateDrawProperties(false)
    , m_pinchGestureActive(false)
    , m_fpsCounter(FrameRateCounter::create(m_proxy->hasImplThread()))
    , m_paintTimeCounter(PaintTimeCounter::create())
    , m_debugRectHistory(DebugRectHistory::create())
    , m_numImplThreadScrolls(0)
    , m_numMainThreadScrolls(0)
    , m_cumulativeNumLayersDrawn(0)
    , m_cumulativeNumMissingTiles(0)
    , m_lastSentMemoryVisibleBytes(0)
    , m_lastSentMemoryVisibleAndNearbyBytes(0)
    , m_lastSentMemoryUseBytes(0)
    , m_animationRegistrar(AnimationRegistrar::create())
{
    DCHECK(m_proxy->isImplThread());
    didVisibilityChange(this, m_visible);

    if (settings.calculateTopControlsPosition)
        m_topControlsManager = TopControlsManager::Create(this, settings.topControlsHeightPx);

    // LTHI always has an active tree.
    m_activeTree = LayerTreeImpl::create(this);
}

LayerTreeHostImpl::~LayerTreeHostImpl()
{
    DCHECK(m_proxy->isImplThread());
    TRACE_EVENT0("cc", "LayerTreeHostImpl::~LayerTreeHostImpl()");

    if (rootLayer()) {
        clearRenderSurfaces();
        // The layer trees must be destroyed before the layer tree host. We've
        // made a contract with our animation controllers that the registrar
        // will outlive them, and we must make good.
        m_activeTree.reset();
        m_pendingTree.reset();
    }
}

void LayerTreeHostImpl::beginCommit()
{
}

void LayerTreeHostImpl::commitComplete()
{
    TRACE_EVENT0("cc", "LayerTreeHostImpl::commitComplete");

    // Impl-side painting needs an update immediately post-commit to have the
    // opportunity to create tilings.  Other paths can call updateDrawProperties
    // more lazily when needed prior to drawing.
    setNeedsUpdateDrawProperties();
    if (m_settings.implSidePainting)
        updateDrawProperties();

    // Recompute max scroll position; must be after layer content bounds are
    // updated.
    updateMaxScrollOffset();
    m_client->sendManagedMemoryStats();
}

bool LayerTreeHostImpl::canDraw()
{
    // Note: If you are changing this function or any other function that might
    // affect the result of canDraw, make sure to call m_client->onCanDrawStateChanged
    // in the proper places and update the notifyIfCanDrawChanged test.

    if (!rootLayer()) {
        TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::canDraw no root layer");
        return false;
    }
    if (deviceViewportSize().IsEmpty()) {
        TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::canDraw empty viewport");
        return false;
    }
    if (!m_renderer) {
        TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::canDraw no renderer");
        return false;
    }
    if (m_activeTree->ContentsTexturesPurged()) {
        TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::canDraw contents textures purged");
        return false;
    }
    return true;
}

OutputSurface* LayerTreeHostImpl::outputSurface() const
{
    return m_outputSurface.get();
}

void LayerTreeHostImpl::animate(base::TimeTicks monotonicTime, base::Time wallClockTime)
{
    animatePageScale(monotonicTime);
    animateLayers(monotonicTime, wallClockTime);
    animateScrollbars(monotonicTime);
    if (m_topControlsManager)
        m_topControlsManager->Animate(monotonicTime);
}

void LayerTreeHostImpl::manageTiles()
{
    DCHECK(m_tileManager);
    m_tileManager->ManageTiles();

    size_t memoryRequiredBytes;
    size_t memoryNiceToHaveBytes;
    size_t memoryUsedBytes;
    m_tileManager->GetMemoryStats(&memoryRequiredBytes,
                                  &memoryNiceToHaveBytes,
                                  &memoryUsedBytes);
    sendManagedMemoryStats(memoryRequiredBytes,
                           memoryNiceToHaveBytes,
                           memoryUsedBytes);
}

void LayerTreeHostImpl::startPageScaleAnimation(gfx::Vector2d targetOffset, bool anchorPoint, float pageScale, base::TimeTicks startTime, base::TimeDelta duration)
{
    if (!rootScrollLayer())
        return;

    gfx::Vector2dF scrollTotal = rootScrollLayer()->scrollOffset() + rootScrollLayer()->scrollDelta();
    gfx::SizeF scaledScrollableSize = activeTree()->ScrollableSize();
    if (!m_settings.pageScalePinchZoomEnabled) {
        scrollTotal.Scale(1 / m_pinchZoomViewport.page_scale_factor());
        scaledScrollableSize.Scale(1 / m_pinchZoomViewport.page_scale_factor());
    }
    gfx::SizeF viewportSize = gfx::ScaleSize(m_deviceViewportSize, 1 / m_deviceScaleFactor);

    double startTimeSeconds = (startTime - base::TimeTicks()).InSecondsF();
    m_pageScaleAnimation = PageScaleAnimation::create(scrollTotal, m_pinchZoomViewport.total_page_scale_factor(), viewportSize, scaledScrollableSize, startTimeSeconds);

    if (anchorPoint) {
        gfx::Vector2dF anchor(targetOffset);
        if (!m_settings.pageScalePinchZoomEnabled)
            anchor.Scale(1 / pageScale);
        m_pageScaleAnimation->zoomWithAnchor(anchor, pageScale, duration.InSecondsF());
    } else {
        gfx::Vector2dF scaledTargetOffset = targetOffset;
        if (!m_settings.pageScalePinchZoomEnabled)
            scaledTargetOffset.Scale(1 / pageScale);
        m_pageScaleAnimation->zoomTo(scaledTargetOffset, pageScale, duration.InSecondsF());
    }

    setNeedsUpdateDrawProperties();
    m_client->setNeedsRedrawOnImplThread();
    m_client->setNeedsCommitOnImplThread();
}

void LayerTreeHostImpl::scheduleAnimation()
{
    m_client->setNeedsRedrawOnImplThread();
}

bool LayerTreeHostImpl::haveTouchEventHandlersAt(const gfx::Point& viewportPoint)
{
    if (!ensureRenderSurfaceLayerList())
      return false;

    gfx::PointF deviceViewportPoint = gfx::ScalePoint(viewportPoint, m_deviceScaleFactor);

    // First find out which layer was hit from the saved list of visible layers
    // in the most recent frame.
    LayerImpl* layerImpl = LayerTreeHostCommon::findLayerThatIsHitByPoint(deviceViewportPoint, activeTree()->RenderSurfaceLayerList());

    // Walk up the hierarchy and look for a layer with a touch event handler region that the given point hits.
    for (; layerImpl; layerImpl = layerImpl->parent()) {
      if (LayerTreeHostCommon::layerHasTouchEventHandlersAt(deviceViewportPoint,layerImpl))
          return true;
    }

    return false;
}

void LayerTreeHostImpl::trackDamageForAllSurfaces(LayerImpl* rootDrawLayer, const LayerList& renderSurfaceLayerList)
{
    // For now, we use damage tracking to compute a global scissor. To do this, we must
    // compute all damage tracking before drawing anything, so that we know the root
    // damage rect. The root damage rect is then used to scissor each surface.

    for (int surfaceIndex = renderSurfaceLayerList.size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        LayerImpl* renderSurfaceLayer = renderSurfaceLayerList[surfaceIndex];
        RenderSurfaceImpl* renderSurface = renderSurfaceLayer->renderSurface();
        DCHECK(renderSurface);
        renderSurface->damageTracker()->updateDamageTrackingState(renderSurface->layerList(), renderSurfaceLayer->id(), renderSurface->surfacePropertyChangedOnlyFromDescendant(), renderSurface->contentRect(), renderSurfaceLayer->maskLayer(), renderSurfaceLayer->filters(), renderSurfaceLayer->filter().get());
    }
}

void LayerTreeHostImpl::updateDrawProperties()
{
    if (!needsUpdateDrawProperties())
        return;

    if (!m_renderer) { // For maxTextureSize.
      // This will get set if renderer gets initialized.
      m_needsUpdateDrawProperties = false;
      return;
    }

    if (m_topControlsManager)
      m_topControlsManager->UpdateDrawPositions();
    activeTree()->UpdateDrawProperties();
    if (pendingTree())
      pendingTree()->UpdateDrawProperties();

    m_needsUpdateDrawProperties = false;
}

void LayerTreeHostImpl::FrameData::appendRenderPass(scoped_ptr<RenderPass> renderPass)
{
    renderPassesById[renderPass->id] = renderPass.get();
    renderPasses.push_back(renderPass.Pass());
}

static void appendQuadsForLayer(RenderPass* targetRenderPass, LayerImpl* layer, OcclusionTrackerImpl& occlusionTracker, AppendQuadsData& appendQuadsData)
{
    bool forSurface = false;
    QuadCuller quadCuller(targetRenderPass->quad_list,
                          targetRenderPass->shared_quad_state_list,
                          layer,
                          occlusionTracker,
                          layer->showDebugBorders(),
                          forSurface);
    layer->appendQuads(quadCuller, appendQuadsData);
}

static void appendQuadsForRenderSurfaceLayer(RenderPass* targetRenderPass, LayerImpl* layer, const RenderPass* contributingRenderPass, OcclusionTrackerImpl& occlusionTracker, AppendQuadsData& appendQuadsData)
{
    bool forSurface = true;
    QuadCuller quadCuller(targetRenderPass->quad_list,
                          targetRenderPass->shared_quad_state_list,
                          layer,
                          occlusionTracker,
                          layer->showDebugBorders(),
                          forSurface);

    bool isReplica = false;
    layer->renderSurface()->appendQuads(quadCuller,
                                        appendQuadsData,
                                        isReplica,
                                        contributingRenderPass->id);

    // Add replica after the surface so that it appears below the surface.
    if (layer->hasReplica()) {
        isReplica = true;
        layer->renderSurface()->appendQuads(quadCuller,
                                            appendQuadsData,
                                            isReplica,
                                            contributingRenderPass->id);
    }
}

static void appendQuadsToFillScreen(RenderPass* targetRenderPass, LayerImpl* rootLayer, SkColor screenBackgroundColor, const OcclusionTrackerImpl& occlusionTracker)
{
    if (!rootLayer || !SkColorGetA(screenBackgroundColor))
        return;

    Region fillRegion = occlusionTracker.computeVisibleRegionInScreen();
    if (fillRegion.IsEmpty())
        return;

    bool forSurface = false;
    QuadCuller quadCuller(targetRenderPass->quad_list,
                          targetRenderPass->shared_quad_state_list,
                          rootLayer,
                          occlusionTracker,
                          rootLayer->showDebugBorders(),
                          forSurface);

    // Manually create the quad state for the gutter quads, as the root layer
    // doesn't have any bounds and so can't generate this itself.
    // FIXME: Make the gutter quads generated by the solid color layer (make it smarter about generating quads to fill unoccluded areas).

    gfx::Rect rootTargetRect = rootLayer->renderSurface()->contentRect();
    float opacity = 1;
    SharedQuadState* sharedQuadState = quadCuller.useSharedQuadState(SharedQuadState::Create());
    sharedQuadState->SetAll(rootLayer->drawTransform(),
                            rootTargetRect,
                            rootTargetRect,
                            false,
                            opacity);

    AppendQuadsData appendQuadsData;

    gfx::Transform transformToLayerSpace(gfx::Transform::kSkipInitialization);
    bool didInvert = rootLayer->screenSpaceTransform().GetInverse(&transformToLayerSpace);
    DCHECK(didInvert);
    for (Region::Iterator fillRects(fillRegion); fillRects.has_rect(); fillRects.next()) {
        // The root layer transform is composed of translations and scales only,
        // no perspective, so mapping is sufficient (as opposed to projecting).
        gfx::Rect layerRect = MathUtil::mapClippedRect(transformToLayerSpace, fillRects.rect());
        // Skip the quad culler and just append the quads directly to avoid
        // occlusion checks.
        scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
        quad->SetNew(sharedQuadState, layerRect, screenBackgroundColor);
        quadCuller.append(quad.PassAs<DrawQuad>(), appendQuadsData);
    }
}

bool LayerTreeHostImpl::calculateRenderPasses(FrameData& frame)
{
    DCHECK(frame.renderPasses.empty());

    if (!canDraw() || !rootLayer())
      return false;

    trackDamageForAllSurfaces(rootLayer(), *frame.renderSurfaceLayerList);

    TRACE_EVENT1("cc", "LayerTreeHostImpl::calculateRenderPasses", "renderSurfaceLayerList.size()", static_cast<long long unsigned>(frame.renderSurfaceLayerList->size()));

    // Create the render passes in dependency order.
    for (int surfaceIndex = frame.renderSurfaceLayerList->size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        LayerImpl* renderSurfaceLayer = (*frame.renderSurfaceLayerList)[surfaceIndex];
        renderSurfaceLayer->renderSurface()->appendRenderPasses(frame);
    }

    bool recordMetricsForFrame = m_settings.showOverdrawInTracing && base::debug::TraceLog::GetInstance() && base::debug::TraceLog::GetInstance()->IsEnabled();
    OcclusionTrackerImpl occlusionTracker(rootLayer()->renderSurface()->contentRect(), recordMetricsForFrame);
    occlusionTracker.setMinimumTrackingSize(m_settings.minimumOcclusionTrackingSize);

    if (m_debugState.showOccludingRects)
        occlusionTracker.setOccludingScreenSpaceRectsContainer(&frame.occludingScreenSpaceRects);
    if (m_debugState.showNonOccludingRects)
        occlusionTracker.setNonOccludingScreenSpaceRectsContainer(&frame.nonOccludingScreenSpaceRects);

    // Add quads to the Render passes in FrontToBack order to allow for testing occlusion and performing culling during the tree walk.
    typedef LayerIterator<LayerImpl, std::vector<LayerImpl*>, RenderSurfaceImpl, LayerIteratorActions::FrontToBack> LayerIteratorType;

    // Typically when we are missing a texture and use a checkerboard quad, we still draw the frame. However when the layer being
    // checkerboarded is moving due to an impl-animation, we drop the frame to avoid flashing due to the texture suddenly appearing
    // in the future.
    bool drawFrame = true;

    LayerIteratorType end = LayerIteratorType::end(frame.renderSurfaceLayerList);
    for (LayerIteratorType it = LayerIteratorType::begin(frame.renderSurfaceLayerList); it != end; ++it) {
        RenderPass::Id targetRenderPassId = it.targetRenderSurfaceLayer()->renderSurface()->renderPassId();
        RenderPass* targetRenderPass = frame.renderPassesById[targetRenderPassId];

        occlusionTracker.enterLayer(it);

        AppendQuadsData appendQuadsData(targetRenderPass->id);

        if (it.representsContributingRenderSurface()) {
            RenderPass::Id contributingRenderPassId = it->renderSurface()->renderPassId();
            RenderPass* contributingRenderPass = frame.renderPassesById[contributingRenderPassId];
            appendQuadsForRenderSurfaceLayer(targetRenderPass, *it, contributingRenderPass, occlusionTracker, appendQuadsData);
        } else if (it.representsItself() && !it->visibleContentRect().IsEmpty()) {
            bool hasOcclusionFromOutsideTargetSurface;
            bool implDrawTransformIsUnknown = false;
            if (occlusionTracker.occluded(it->renderTarget(), it->visibleContentRect(), it->drawTransform(), implDrawTransformIsUnknown, it->isClipped(), it->clipRect(), &hasOcclusionFromOutsideTargetSurface))
                appendQuadsData.hadOcclusionFromOutsideTargetSurface |= hasOcclusionFromOutsideTargetSurface;
            else {
                DCHECK_EQ(activeTree(), it->layerTreeImpl());
                it->willDraw(m_resourceProvider.get());
                frame.willDrawLayers.push_back(*it);

                if (it->hasContributingDelegatedRenderPasses()) {
                    RenderPass::Id contributingRenderPassId = it->firstContributingRenderPassId();
                    while (frame.renderPassesById.find(contributingRenderPassId) != frame.renderPassesById.end()) {
                        RenderPass* renderPass = frame.renderPassesById[contributingRenderPassId];
  
                        AppendQuadsData appendQuadsData(renderPass->id);
                        appendQuadsForLayer(renderPass, *it, occlusionTracker, appendQuadsData);

                        contributingRenderPassId = it->nextContributingRenderPassId(contributingRenderPassId);
                    }
                }

                appendQuadsForLayer(targetRenderPass, *it, occlusionTracker, appendQuadsData);
            }

            ++m_cumulativeNumLayersDrawn;
        }

        if (appendQuadsData.hadOcclusionFromOutsideTargetSurface)
          targetRenderPass->has_occlusion_from_outside_target_surface = true;

        if (appendQuadsData.numMissingTiles) {
            m_cumulativeNumMissingTiles += appendQuadsData.numMissingTiles;
            bool layerHasAnimatingTransform = it->screenSpaceTransformIsAnimating() || it->drawTransformIsAnimating();
            if (layerHasAnimatingTransform)
                drawFrame = false;
        }

        occlusionTracker.leaveLayer(it);
    }

#ifndef NDEBUG
    for (size_t i = 0; i < frame.renderPasses.size(); ++i) {
        for (size_t j = 0; j < frame.renderPasses[i]->quad_list.size(); ++j)
            DCHECK(frame.renderPasses[i]->quad_list[j]->shared_quad_state);
        DCHECK(frame.renderPassesById.find(frame.renderPasses[i]->id)
               != frame.renderPassesById.end());
    }
#endif
    DCHECK(frame.renderPasses.back()->output_rect.origin().IsOrigin());

    if (!activeTree()->has_transparent_background()) {
        frame.renderPasses.back()->has_transparent_background = false;
        appendQuadsToFillScreen(frame.renderPasses.back(), rootLayer(), activeTree()->background_color(), occlusionTracker);
    }

    if (drawFrame)
        occlusionTracker.overdrawMetrics().recordMetrics(this);

    removeRenderPasses(CullRenderPassesWithNoQuads(), frame);
    m_renderer->decideRenderPassAllocationsForFrame(frame.renderPasses);
    removeRenderPasses(CullRenderPassesWithCachedTextures(*m_renderer), frame);

    return drawFrame;
}

void LayerTreeHostImpl::setBackgroundTickingEnabled(bool enabled)
{
    // Lazily create the timeSource adapter so that we can vary the interval for testing.
    if (!m_timeSourceClientAdapter)
        m_timeSourceClientAdapter = LayerTreeHostImplTimeSourceAdapter::create(this, DelayBasedTimeSource::create(lowFrequencyAnimationInterval(), m_proxy->currentThread()));

    m_timeSourceClientAdapter->setActive(enabled);
}

static inline RenderPass* findRenderPassById(RenderPass::Id renderPassId, const LayerTreeHostImpl::FrameData& frame)
{
    RenderPassIdHashMap::const_iterator it = frame.renderPassesById.find(renderPassId);
    return it != frame.renderPassesById.end() ? it->second : NULL;
}

static void removeRenderPassesRecursive(RenderPass::Id removeRenderPassId, LayerTreeHostImpl::FrameData& frame)
{
    RenderPass* removeRenderPass = findRenderPassById(removeRenderPassId, frame);
    // The pass was already removed by another quad - probably the original, and we are the replica.
    if (!removeRenderPass)
        return;
    RenderPassList& renderPasses = frame.renderPasses;
    RenderPassList::iterator toRemove = std::find(renderPasses.begin(), renderPasses.end(), removeRenderPass);

    DCHECK(toRemove != renderPasses.end());

    scoped_ptr<RenderPass> removedPass = renderPasses.take(toRemove);
    frame.renderPasses.erase(toRemove);
    frame.renderPassesById.erase(removeRenderPassId);

    // Now follow up for all RenderPass quads and remove their RenderPasses recursively.
    const QuadList& quadList = removedPass->quad_list;
    QuadList::constBackToFrontIterator quadListIterator = quadList.backToFrontBegin();
    for (; quadListIterator != quadList.backToFrontEnd(); ++quadListIterator) {
        DrawQuad* currentQuad = (*quadListIterator);
        if (currentQuad->material != DrawQuad::RENDER_PASS)
            continue;

        RenderPass::Id nextRemoveRenderPassId = RenderPassDrawQuad::MaterialCast(currentQuad)->render_pass_id;
        removeRenderPassesRecursive(nextRemoveRenderPassId, frame);
    }
}

bool LayerTreeHostImpl::CullRenderPassesWithCachedTextures::shouldRemoveRenderPass(const RenderPassDrawQuad& quad, const FrameData&) const
{
    if (!quad.contents_changed_since_last_frame.IsEmpty()) {
        TRACE_EVENT0("cc", "CullRenderPassesWithCachedTextures have damage");
        return false;
    } else if (!m_renderer.haveCachedResourcesForRenderPassId(quad.render_pass_id)) {
        TRACE_EVENT0("cc", "CullRenderPassesWithCachedTextures have no texture");
        return false;
    }
    TRACE_EVENT0("cc", "CullRenderPassesWithCachedTextures dropped!");
    return true;
}

bool LayerTreeHostImpl::CullRenderPassesWithNoQuads::shouldRemoveRenderPass(const RenderPassDrawQuad& quad, const FrameData& frame) const
{
    const RenderPass* renderPass = findRenderPassById(quad.render_pass_id, frame);
    if (!renderPass)
        return false;

    // If any quad or RenderPass draws into this RenderPass, then keep it.
    const QuadList& quadList = renderPass->quad_list;
    for (QuadList::constBackToFrontIterator quadListIterator = quadList.backToFrontBegin(); quadListIterator != quadList.backToFrontEnd(); ++quadListIterator) {
        DrawQuad* currentQuad = *quadListIterator;

        if (currentQuad->material != DrawQuad::RENDER_PASS)
            return false;

        const RenderPass* contributingPass = findRenderPassById(RenderPassDrawQuad::MaterialCast(currentQuad)->render_pass_id, frame);
        if (contributingPass)
            return false;
    }
    return true;
}

// Defined for linking tests.
template CC_EXPORT void LayerTreeHostImpl::removeRenderPasses<LayerTreeHostImpl::CullRenderPassesWithCachedTextures>(CullRenderPassesWithCachedTextures, FrameData&);
template CC_EXPORT void LayerTreeHostImpl::removeRenderPasses<LayerTreeHostImpl::CullRenderPassesWithNoQuads>(CullRenderPassesWithNoQuads, FrameData&);

// static
template<typename RenderPassCuller>
void LayerTreeHostImpl::removeRenderPasses(RenderPassCuller culler, FrameData& frame)
{
    for (size_t it = culler.renderPassListBegin(frame.renderPasses); it != culler.renderPassListEnd(frame.renderPasses); it = culler.renderPassListNext(it)) {
        const RenderPass* currentPass = frame.renderPasses[it];
        const QuadList& quadList = currentPass->quad_list;
        QuadList::constBackToFrontIterator quadListIterator = quadList.backToFrontBegin();

        for (; quadListIterator != quadList.backToFrontEnd(); ++quadListIterator) {
            DrawQuad* currentQuad = *quadListIterator;

            if (currentQuad->material != DrawQuad::RENDER_PASS)
                continue;

            RenderPassDrawQuad* renderPassQuad = static_cast<RenderPassDrawQuad*>(currentQuad);
            if (!culler.shouldRemoveRenderPass(*renderPassQuad, frame))
                continue;

            // We are changing the vector in the middle of iteration. Because we
            // delete render passes that draw into the current pass, we are
            // guaranteed that any data from the iterator to the end will not
            // change. So, capture the iterator position from the end of the
            // list, and restore it after the change.
            int positionFromEnd = frame.renderPasses.size() - it;
            removeRenderPassesRecursive(renderPassQuad->render_pass_id, frame);
            it = frame.renderPasses.size() - positionFromEnd;
            DCHECK(it >= 0);
        }
    }
}

bool LayerTreeHostImpl::prepareToDraw(FrameData& frame)
{
    TRACE_EVENT0("cc", "LayerTreeHostImpl::prepareToDraw");

    updateDrawProperties();

    frame.renderSurfaceLayerList = &activeTree()->RenderSurfaceLayerList();
    frame.renderPasses.clear();
    frame.renderPassesById.clear();
    frame.willDrawLayers.clear();

    if (!calculateRenderPasses(frame))
        return false;

    // If we return true, then we expect drawLayers() to be called before this function is called again.
    return true;
}

void LayerTreeHostImpl::enforceManagedMemoryPolicy(const ManagedMemoryPolicy& policy)
{
    bool evictedResources = m_client->reduceContentsTextureMemoryOnImplThread(
        m_visible ? policy.bytesLimitWhenVisible : policy.bytesLimitWhenNotVisible,
        ManagedMemoryPolicy::priorityCutoffToValue(
            m_visible ? policy.priorityCutoffWhenVisible : policy.priorityCutoffWhenNotVisible));
    if (evictedResources) {
        activeTree()->SetContentsTexturesPurged();
        if (pendingTree())
            pendingTree()->SetContentsTexturesPurged();
        m_client->setNeedsCommitOnImplThread();
        m_client->onCanDrawStateChanged(canDraw());
        m_client->renewTreePriority();
    }
    m_client->sendManagedMemoryStats();

    if (m_tileManager) {
      GlobalStateThatImpactsTilePriority new_state(m_tileManager->GlobalState());
      new_state.memory_limit_in_bytes = m_visible ? policy.bytesLimitWhenVisible : policy.bytesLimitWhenNotVisible;
      new_state.memory_limit_policy = ManagedMemoryPolicy::priorityCutoffToTileMemoryLimitPolicy(
          m_visible ? policy.priorityCutoffWhenVisible : policy.priorityCutoffWhenNotVisible);
      m_tileManager->SetGlobalState(new_state);
    }
}

bool LayerTreeHostImpl::hasImplThread() const
{
    return m_proxy->hasImplThread();
}

void LayerTreeHostImpl::ScheduleManageTiles()
{
    if (m_client)
      m_client->setNeedsManageTilesOnImplThread();
}

void LayerTreeHostImpl::DidUploadVisibleHighResolutionTile()
{
    if (m_client)
        m_client->didUploadVisibleHighResolutionTileOnImplThread();
}

bool LayerTreeHostImpl::shouldClearRootRenderPass() const
{
    return m_settings.shouldClearRootRenderPass;
}

void LayerTreeHostImpl::setManagedMemoryPolicy(const ManagedMemoryPolicy& policy)
{
    if (m_managedMemoryPolicy == policy)
        return;

    m_managedMemoryPolicy = policy;
    if (!m_proxy->hasImplThread()) {
        // FIXME: In single-thread mode, this can be called on the main thread
        // by GLRenderer::onMemoryAllocationChanged.
        DebugScopedSetImplThread implThread(m_proxy);
        enforceManagedMemoryPolicy(m_managedMemoryPolicy);
    } else {
        DCHECK(m_proxy->isImplThread());
        enforceManagedMemoryPolicy(m_managedMemoryPolicy);
    }
    // We always need to commit after changing the memory policy because the new
    // limit can result in more or less content having texture allocated for it.
    m_client->setNeedsCommitOnImplThread();
}

void LayerTreeHostImpl::OnVSyncParametersChanged(base::TimeTicks timebase, base::TimeDelta interval)
{
    m_client->onVSyncParametersChanged(timebase, interval);
}

void LayerTreeHostImpl::OnSendFrameToParentCompositorAck(const CompositorFrameAck& ack)
{
    if (!m_renderer)
        return;

    // TODO(piman): We may need to do some validation on this ack before processing it.
    m_renderer->receiveCompositorFrameAck(ack);
}

void LayerTreeHostImpl::OnCanDrawStateChangedForTree(LayerTreeImpl*)
{
    m_client->onCanDrawStateChanged(canDraw());
}

CompositorFrameMetadata LayerTreeHostImpl::makeCompositorFrameMetadata() const
{
    CompositorFrameMetadata metadata;
    metadata.page_scale_factor = m_pinchZoomViewport.total_page_scale_factor();
    metadata.viewport_size = m_pinchZoomViewport.ZoomedViewport().size();
    metadata.root_layer_size = activeTree()->ScrollableSize();
    metadata.min_page_scale_factor = m_pinchZoomViewport.min_page_scale_factor();
    metadata.max_page_scale_factor = m_pinchZoomViewport.max_page_scale_factor();
    if (m_topControlsManager) {
        metadata.location_bar_offset = gfx::Vector2dF(0.f, m_topControlsManager->controls_top_offset());
        metadata.location_bar_content_translation = gfx::Vector2dF(0.f, m_topControlsManager->content_top_offset());
    }

    if (!rootScrollLayer())
      return metadata;

    metadata.root_scroll_offset = rootScrollLayer()->scrollOffset() + rootScrollLayer()->scrollDelta();
    if (!m_settings.pageScalePinchZoomEnabled)
        metadata.root_scroll_offset.Scale(1 / m_pinchZoomViewport.page_scale_factor());

    return metadata;
}

void LayerTreeHostImpl::drawLayers(FrameData& frame)
{
    TRACE_EVENT0("cc", "LayerTreeHostImpl::drawLayers");
    DCHECK(canDraw());
    DCHECK(!frame.renderPasses.empty());

    // FIXME: use the frame begin time from the overall compositor scheduler.
    // This value is currently inaccessible because it is up in Chromium's
    // RenderWidget.
    m_fpsCounter->saveTimeStamp(base::TimeTicks::Now());

    if (m_debugState.showHudRects())
        m_debugRectHistory->saveDebugRectsForCurrentFrame(rootLayer(), *frame.renderSurfaceLayerList, frame.occludingScreenSpaceRects, frame.nonOccludingScreenSpaceRects, m_debugState);

    // Because the contents of the HUD depend on everything else in the frame, the contents
    // of its texture are updated as the last thing before the frame is drawn.
    if (m_activeTree->hud_layer())
        m_activeTree->hud_layer()->updateHudTexture(m_resourceProvider.get());

    m_renderer->drawFrame(frame.renderPasses);
    // The render passes should be consumed by the renderer.
    DCHECK(frame.renderPasses.empty());
    frame.renderPassesById.clear();

    // The next frame should start by assuming nothing has changed, and changes are noted as they occur.
    for (unsigned int i = 0; i < frame.renderSurfaceLayerList->size(); i++)
        (*frame.renderSurfaceLayerList)[i]->renderSurface()->damageTracker()->didDrawDamagedArea();
    rootLayer()->resetAllChangeTrackingForSubtree();
}

void LayerTreeHostImpl::didDrawAllLayers(const FrameData& frame)
{
    for (size_t i = 0; i < frame.willDrawLayers.size(); ++i)
        frame.willDrawLayers[i]->didDraw(m_resourceProvider.get());

    // Once all layers have been drawn, pending texture uploads should no
    // longer block future uploads.
    m_resourceProvider->markPendingUploadsAsNonBlocking();
}

void LayerTreeHostImpl::finishAllRendering()
{
    if (m_renderer)
        m_renderer->finish();
}

bool LayerTreeHostImpl::isContextLost()
{
    DCHECK(m_proxy->isImplThread());
    return m_renderer && m_renderer->isContextLost();
}

const RendererCapabilities& LayerTreeHostImpl::rendererCapabilities() const
{
    return m_renderer->capabilities();
}

bool LayerTreeHostImpl::swapBuffers()
{
    DCHECK(m_renderer);
    bool result = m_renderer->swapBuffers();

    if (m_settings.implSidePainting &&
        !activeTree()->AreVisibleResourcesReady()) {
        m_client->didSwapUseIncompleteTileOnImplThread();
    }

    return result;
}

const gfx::Size& LayerTreeHostImpl::deviceViewportSize() const
{
    return m_deviceViewportSize;
}

const LayerTreeSettings& LayerTreeHostImpl::settings() const
{
    return m_settings;
}

void LayerTreeHostImpl::didLoseOutputSurface()
{
    m_client->didLoseOutputSurfaceOnImplThread();
}

void LayerTreeHostImpl::onSwapBuffersComplete()
{
    m_client->onSwapBuffersCompleteOnImplThread();
}

void LayerTreeHostImpl::readback(void* pixels, const gfx::Rect& rect)
{
    DCHECK(m_renderer);
    m_renderer->getFramebufferPixels(pixels, rect);
}

bool LayerTreeHostImpl::haveRootScrollLayer() const {
  return rootScrollLayer();
}

float LayerTreeHostImpl::rootScrollLayerTotalScrollY() const {
  if (LayerImpl* layer = rootScrollLayer())
    return layer->scrollOffset().y() + layer->scrollDelta().y();
  return 0.0f;
}

LayerImpl* LayerTreeHostImpl::rootLayer() const
{
  return m_activeTree->RootLayer();
}

LayerImpl* LayerTreeHostImpl::rootScrollLayer() const
{
  return m_activeTree->RootScrollLayer();
}

LayerImpl* LayerTreeHostImpl::currentlyScrollingLayer() const
{
  return m_activeTree->CurrentlyScrollingLayer();
}

// Content layers can be either directly scrollable or contained in an outer
// scrolling layer which applies the scroll transform. Given a content layer,
// this function returns the associated scroll layer if any.
static LayerImpl* findScrollLayerForContentLayer(LayerImpl* layerImpl)
{
    if (!layerImpl)
        return 0;

    if (layerImpl->scrollable())
        return layerImpl;

    if (layerImpl->drawsContent() && layerImpl->parent() && layerImpl->parent()->scrollable())
        return layerImpl->parent();

    return 0;
}

void LayerTreeHostImpl::createPendingTree()
{
    CHECK(!m_pendingTree);
    m_pendingTree = LayerTreeImpl::create(this);
    m_client->onCanDrawStateChanged(canDraw());
    m_client->onHasPendingTreeStateChanged(pendingTree());
}

void LayerTreeHostImpl::checkForCompletedTileUploads()
{
    DCHECK(!m_client->isInsideDraw()) << "Checking for completed uploads within a draw may trigger spurious redraws.";
    if (m_tileManager)
        m_tileManager->CheckForCompletedTileUploads();
}

void LayerTreeHostImpl::activatePendingTreeIfNeeded()
{
    if (!pendingTree())
        return;

    // It's always fine to activate to an empty tree.  Otherwise, only
    // activate once all visible resources in pending tree are ready.
    if (activeTree()->RootLayer() &&
        !pendingTree()->AreVisibleResourcesReady())
      return;

    activatePendingTree();
}

void LayerTreeHostImpl::activatePendingTree()
{
    CHECK(m_pendingTree);

    m_activeTree->PushPersistedState(m_pendingTree.get());
    m_activeTree.swap(m_pendingTree);
    // TODO(enne): consider recycling this tree to prevent layer churn
    m_pendingTree.reset();
    m_activeTree->DidBecomeActive();

    m_client->onCanDrawStateChanged(canDraw());
    m_client->onHasPendingTreeStateChanged(pendingTree());
    m_client->setNeedsRedrawOnImplThread();
    m_client->renewTreePriority();
}

void LayerTreeHostImpl::setVisible(bool visible)
{
    DCHECK(m_proxy->isImplThread());

    if (m_visible == visible)
        return;
    m_visible = visible;
    didVisibilityChange(this, m_visible);
    enforceManagedMemoryPolicy(m_managedMemoryPolicy);

    if (!m_renderer)
        return;

    m_renderer->setVisible(visible);

    setBackgroundTickingEnabled(!m_visible && !m_animationRegistrar->active_animation_controllers().empty());
}

bool LayerTreeHostImpl::initializeRenderer(scoped_ptr<OutputSurface> outputSurface)
{
    // Since we will create a new resource provider, we cannot continue to use
    // the old resources (i.e. renderSurfaces and texture IDs). Clear them
    // before we destroy the old resource provider.
    if (rootLayer())
        clearRenderSurfaces();
    if (activeTree()->RootLayer())
        sendDidLoseOutputSurfaceRecursive(activeTree()->RootLayer());
    if (pendingTree() && pendingTree()->RootLayer())
        sendDidLoseOutputSurfaceRecursive(pendingTree()->RootLayer());

    // Note: order is important here.
    m_renderer.reset();
    m_tileManager.reset();
    m_resourceProvider.reset();
    m_outputSurface.reset();

    if (!outputSurface->BindToClient(this))
        return false;

    scoped_ptr<ResourceProvider> resourceProvider = ResourceProvider::create(outputSurface.get());
    if (!resourceProvider)
        return false;

    if (m_settings.implSidePainting)
      m_tileManager.reset(new TileManager(this, resourceProvider.get(), m_settings.numRasterThreads));

    if (outputSurface->Capabilities().has_parent_compositor)
        m_renderer = DelegatingRenderer::Create(this, outputSurface.get(), resourceProvider.get());
    else if (outputSurface->Context3D())
        m_renderer = GLRenderer::create(this, outputSurface.get(), resourceProvider.get());
    else if (outputSurface->SoftwareDevice())
        m_renderer = SoftwareRenderer::create(this, resourceProvider.get(), outputSurface->SoftwareDevice());
    if (!m_renderer)
        return false;

    setNeedsUpdateDrawProperties();
    m_resourceProvider = resourceProvider.Pass();
    m_outputSurface = outputSurface.Pass();

    if (!m_visible)
        m_renderer->setVisible(m_visible);

    m_client->onCanDrawStateChanged(canDraw());

    return true;
}

void LayerTreeHostImpl::setViewportSize(const gfx::Size& layoutViewportSize, const gfx::Size& deviceViewportSize)
{
    if (layoutViewportSize == m_layoutViewportSize && deviceViewportSize == m_deviceViewportSize)
        return;

    m_layoutViewportSize = layoutViewportSize;
    m_deviceViewportSize = deviceViewportSize;

    m_pinchZoomViewport.set_layout_viewport_size(layoutViewportSize);
    m_pinchZoomViewport.set_device_viewport_size(deviceViewportSize);

    updateMaxScrollOffset();

    if (m_renderer)
        m_renderer->viewportChanged();

    m_client->onCanDrawStateChanged(canDraw());
}

static void adjustScrollsForPageScaleChange(LayerImpl* layerImpl, float pageScaleChange)
{
    if (!layerImpl)
        return;

    if (layerImpl->scrollable()) {
        // We need to convert impl-side scroll deltas to pageScale space.
        gfx::Vector2dF scrollDelta = layerImpl->scrollDelta();
        scrollDelta.Scale(pageScaleChange);
        layerImpl->setScrollDelta(scrollDelta);
    }

    for (size_t i = 0; i < layerImpl->children().size(); ++i)
        adjustScrollsForPageScaleChange(layerImpl->children()[i], pageScaleChange);
}

void LayerTreeHostImpl::setDeviceScaleFactor(float deviceScaleFactor)
{
    if (deviceScaleFactor == m_deviceScaleFactor)
        return;
    m_deviceScaleFactor = deviceScaleFactor;
    m_pinchZoomViewport.set_device_scale_factor(m_deviceScaleFactor);

    updateMaxScrollOffset();
}

float LayerTreeHostImpl::pageScaleFactor() const
{
    return m_pinchZoomViewport.page_scale_factor();
}

void LayerTreeHostImpl::setPageScaleFactorAndLimits(float pageScaleFactor, float minPageScaleFactor, float maxPageScaleFactor)
{
    if (!pageScaleFactor)
      return;

    float pageScaleChange = pageScaleFactor / m_pinchZoomViewport.page_scale_factor();
    m_pinchZoomViewport.SetPageScaleFactorAndLimits(pageScaleFactor, minPageScaleFactor, maxPageScaleFactor);

    if (!m_settings.pageScalePinchZoomEnabled && pageScaleChange != 1)
        adjustScrollsForPageScaleChange(rootScrollLayer(), pageScaleChange);

    // Clamp delta to limits and refresh display matrix.
    setPageScaleDelta(m_pinchZoomViewport.page_scale_delta() / m_pinchZoomViewport.sent_page_scale_delta());
    m_pinchZoomViewport.set_sent_page_scale_delta(1);
}

void LayerTreeHostImpl::setPageScaleDelta(float delta)
{
    m_pinchZoomViewport.set_page_scale_delta(delta);

    updateMaxScrollOffset();
}

void LayerTreeHostImpl::updateMaxScrollOffset()
{
    activeTree()->UpdateMaxScrollOffset();
    if (pendingTree())
        pendingTree()->UpdateMaxScrollOffset();
}

void LayerTreeHostImpl::setNeedsUpdateDrawProperties()
{
    m_needsUpdateDrawProperties = true;
}

void LayerTreeHostImpl::setNeedsRedraw()
{
    m_client->setNeedsRedrawOnImplThread();
}

bool LayerTreeHostImpl::ensureRenderSurfaceLayerList()
{
    updateDrawProperties();
    return activeTree()->RenderSurfaceLayerList().size();
}

InputHandlerClient::ScrollStatus LayerTreeHostImpl::scrollBegin(gfx::Point viewportPoint, InputHandlerClient::ScrollInputType type)
{
    TRACE_EVENT0("cc", "LayerTreeHostImpl::scrollBegin");

    if (m_topControlsManager)
        m_topControlsManager->ScrollBegin();

    DCHECK(!currentlyScrollingLayer());
    clearCurrentlyScrollingLayer();

    if (!ensureRenderSurfaceLayerList())
        return ScrollIgnored;

    gfx::PointF deviceViewportPoint = gfx::ScalePoint(viewportPoint, m_deviceScaleFactor);

    // First find out which layer was hit from the saved list of visible layers
    // in the most recent frame.
    LayerImpl* layerImpl = LayerTreeHostCommon::findLayerThatIsHitByPoint(deviceViewportPoint, activeTree()->RenderSurfaceLayerList());

    // Walk up the hierarchy and look for a scrollable layer.
    LayerImpl* potentiallyScrollingLayerImpl = 0;
    for (; layerImpl; layerImpl = layerImpl->parent()) {
        // The content layer can also block attempts to scroll outside the main thread.
        if (layerImpl->tryScroll(deviceViewportPoint, type) == ScrollOnMainThread) {
            m_numMainThreadScrolls++;
            return ScrollOnMainThread;
        }

        LayerImpl* scrollLayerImpl = findScrollLayerForContentLayer(layerImpl);
        if (!scrollLayerImpl)
            continue;

        ScrollStatus status = scrollLayerImpl->tryScroll(deviceViewportPoint, type);

        // If any layer wants to divert the scroll event to the main thread, abort.
        if (status == ScrollOnMainThread) {
            m_numMainThreadScrolls++;
            return ScrollOnMainThread;
        }

        if (status == ScrollStarted && !potentiallyScrollingLayerImpl)
            potentiallyScrollingLayerImpl = scrollLayerImpl;
    }

    if (potentiallyScrollingLayerImpl) {
        m_activeTree->set_currently_scrolling_layer(potentiallyScrollingLayerImpl);
        // Gesture events need to be transformed from viewport coordinates to local layer coordinates
        // so that the scrolling contents exactly follow the user's finger. In contrast, wheel
        // events are already in local layer coordinates so we can just apply them directly.
        m_scrollDeltaIsInViewportSpace = (type == Gesture);
        m_numImplThreadScrolls++;
        m_client->renewTreePriority();
        setNeedsUpdateDrawProperties();
        return ScrollStarted;
    }
    return ScrollIgnored;
}

static gfx::Vector2dF scrollLayerWithViewportSpaceDelta(PinchZoomViewport* viewport, LayerImpl& layerImpl, float scaleFromViewportToScreenSpace, gfx::PointF viewportPoint, gfx::Vector2dF viewportDelta)
{
    // Layers with non-invertible screen space transforms should not have passed the scroll hit
    // test in the first place.
    DCHECK(layerImpl.screenSpaceTransform().IsInvertible());
    gfx::Transform inverseScreenSpaceTransform(gfx::Transform::kSkipInitialization);
    bool didInvert = layerImpl.screenSpaceTransform().GetInverse(&inverseScreenSpaceTransform);
    // TODO: With the advent of impl-side crolling for non-root layers, we may
    // need to explicitly handle uninvertible transforms here.
    DCHECK(didInvert);

    gfx::PointF screenSpacePoint = gfx::ScalePoint(viewportPoint, scaleFromViewportToScreenSpace);

    gfx::Vector2dF screenSpaceDelta = viewportDelta;
    screenSpaceDelta.Scale(scaleFromViewportToScreenSpace);

    // First project the scroll start and end points to local layer space to find the scroll delta
    // in layer coordinates.
    bool startClipped, endClipped;
    gfx::PointF screenSpaceEndPoint = screenSpacePoint + screenSpaceDelta;
    gfx::PointF localStartPoint = MathUtil::projectPoint(inverseScreenSpaceTransform, screenSpacePoint, startClipped);
    gfx::PointF localEndPoint = MathUtil::projectPoint(inverseScreenSpaceTransform, screenSpaceEndPoint, endClipped);

    // In general scroll point coordinates should not get clipped.
    DCHECK(!startClipped);
    DCHECK(!endClipped);
    if (startClipped || endClipped)
        return gfx::Vector2dF();

    // localStartPoint and localEndPoint are in content space but we want to move them to layer space for scrolling.
    float widthScale = 1 / layerImpl.contentsScaleX();
    float heightScale = 1 / layerImpl.contentsScaleY();
    localStartPoint.Scale(widthScale, heightScale);
    localEndPoint.Scale(widthScale, heightScale);

    // Apply the scroll delta.
    gfx::Vector2dF previousDelta = layerImpl.scrollDelta();
    gfx::Vector2dF unscrolled = layerImpl.scrollBy(localEndPoint - localStartPoint);
    gfx::Vector2dF scrollAmount = localEndPoint - localStartPoint;

    gfx::Vector2dF viewportAppliedPan;
    if (viewport)
        viewportAppliedPan = unscrolled - viewport->ApplyScroll(unscrolled);

    // Get the end point in the layer's content space so we can apply its screenSpaceTransform.
    gfx::PointF actualLocalEndPoint = localStartPoint + layerImpl.scrollDelta() + viewportAppliedPan - previousDelta;
    gfx::PointF actualLocalContentEndPoint = gfx::ScalePoint(actualLocalEndPoint, 1 / widthScale, 1 / heightScale);

    // Calculate the applied scroll delta in viewport space coordinates.
    gfx::PointF actualScreenSpaceEndPoint = MathUtil::mapPoint(layerImpl.screenSpaceTransform(), actualLocalContentEndPoint, endClipped);
    DCHECK(!endClipped);
    if (endClipped)
        return gfx::Vector2dF();
    gfx::PointF actualViewportEndPoint = gfx::ScalePoint(actualScreenSpaceEndPoint, 1 / scaleFromViewportToScreenSpace);
    return actualViewportEndPoint - viewportPoint;
}

static gfx::Vector2dF scrollLayerWithLocalDelta(LayerImpl& layerImpl, gfx::Vector2dF localDelta)
{
    gfx::Vector2dF previousDelta(layerImpl.scrollDelta());
    layerImpl.scrollBy(localDelta);
    return layerImpl.scrollDelta() - previousDelta;
}

bool LayerTreeHostImpl::scrollBy(const gfx::Point& viewportPoint,
                                 const gfx::Vector2d& scrollDelta)
{
    TRACE_EVENT0("cc", "LayerTreeHostImpl::scrollBy");
    if (!currentlyScrollingLayer())
        return false;

    gfx::Vector2dF pendingDelta = scrollDelta;
    bool didScroll = false;

    for (LayerImpl* layerImpl = currentlyScrollingLayer(); layerImpl; layerImpl = layerImpl->parent()) {
        if (!layerImpl->scrollable())
            continue;

        PinchZoomViewport* viewport = NULL;
        if (m_settings.pageScalePinchZoomEnabled && layerImpl == rootScrollLayer())
            viewport = &m_pinchZoomViewport;
        gfx::Vector2dF appliedDelta;
        if (m_topControlsManager && layerImpl == rootScrollLayer())
            pendingDelta = m_topControlsManager->ScrollBy(pendingDelta);

        if (m_scrollDeltaIsInViewportSpace) {
            float scaleFromViewportToScreenSpace = m_deviceScaleFactor;
            appliedDelta = scrollLayerWithViewportSpaceDelta(viewport, *layerImpl, scaleFromViewportToScreenSpace, viewportPoint, pendingDelta);
        } else
            appliedDelta = scrollLayerWithLocalDelta(*layerImpl, pendingDelta);

        // If the layer wasn't able to move, try the next one in the hierarchy.
        float moveThresholdSquared = 0.1f * 0.1f;
        if (appliedDelta.LengthSquared() < moveThresholdSquared)
            continue;
        didScroll = true;

        // If the applied delta is within 45 degrees of the input delta, bail out to make it easier
        // to scroll just one layer in one direction without affecting any of its parents.
        float angleThreshold = 45;
        if (MathUtil::smallestAngleBetweenVectors(appliedDelta, pendingDelta) < angleThreshold) {
            pendingDelta = gfx::Vector2d();
            break;
        }

        // Allow further movement only on an axis perpendicular to the direction in which the layer
        // moved.
        gfx::Vector2dF perpendicularAxis(-appliedDelta.y(), appliedDelta.x());
        pendingDelta = MathUtil::projectVector(pendingDelta, perpendicularAxis);

        if (gfx::ToFlooredVector2d(pendingDelta).IsZero())
            break;
    }

    if (didScroll) {
        m_client->setNeedsCommitOnImplThread();
        m_client->setNeedsRedrawOnImplThread();
        m_client->renewTreePriority();
        setNeedsUpdateDrawProperties();
    }
    return didScroll;
}

void LayerTreeHostImpl::clearCurrentlyScrollingLayer()
{
    m_activeTree->ClearCurrentlyScrollingLayer();
}

void LayerTreeHostImpl::scrollEnd()
{
    if (m_topControlsManager)
        m_topControlsManager->ScrollEnd();
    clearCurrentlyScrollingLayer();
}

void LayerTreeHostImpl::pinchGestureBegin()
{
    m_pinchGestureActive = true;
    m_previousPinchAnchor = gfx::Point();
    m_client->renewTreePriority();
}

void LayerTreeHostImpl::pinchGestureUpdate(float magnifyDelta, gfx::Point anchor)
{
    TRACE_EVENT0("cc", "LayerTreeHostImpl::pinchGestureUpdate");

    if (!rootScrollLayer())
        return;

    // Keep the center-of-pinch anchor specified by (x, y) in a stable
    // position over the course of the magnify.
    float pageScaleDelta = m_pinchZoomViewport.page_scale_delta();
    gfx::PointF previousScaleAnchor = gfx::ScalePoint(anchor, 1 / pageScaleDelta);
    setPageScaleDelta(pageScaleDelta * magnifyDelta);
    pageScaleDelta = m_pinchZoomViewport.page_scale_delta();
    gfx::PointF newScaleAnchor = gfx::ScalePoint(anchor, 1 / pageScaleDelta);
    gfx::Vector2dF move = previousScaleAnchor - newScaleAnchor;

    m_previousPinchAnchor = anchor;

    if (m_settings.pageScalePinchZoomEnabled) {
        // Compute the application of the delta with respect to the current page zoom of the page.
        move.Scale(1 / m_pinchZoomViewport.page_scale_factor());
    }

    gfx::Vector2dF scrollOverflow = m_settings.pageScalePinchZoomEnabled ? m_pinchZoomViewport.ApplyScroll(move) : move;
    rootScrollLayer()->scrollBy(scrollOverflow);

    if (rootScrollLayer()->scrollbarAnimationController())
        rootScrollLayer()->scrollbarAnimationController()->didPinchGestureUpdate(base::TimeTicks::Now());

    m_client->setNeedsCommitOnImplThread();
    m_client->setNeedsRedrawOnImplThread();
    m_client->renewTreePriority();
    setNeedsUpdateDrawProperties();
}

void LayerTreeHostImpl::pinchGestureEnd()
{
    m_pinchGestureActive = false;

    if (rootScrollLayer() && rootScrollLayer()->scrollbarAnimationController())
        rootScrollLayer()->scrollbarAnimationController()->didPinchGestureEnd(base::TimeTicks::Now());

    m_client->setNeedsCommitOnImplThread();
}

void LayerTreeHostImpl::computeDoubleTapZoomDeltas(ScrollAndScaleSet* scrollInfo)
{
    gfx::Vector2dF scaledScrollOffset = m_pageScaleAnimation->targetScrollOffset();
    if (!m_settings.pageScalePinchZoomEnabled)
        scaledScrollOffset.Scale(m_pinchZoomViewport.page_scale_factor());
    makeScrollAndScaleSet(scrollInfo, ToFlooredVector2d(scaledScrollOffset), m_pageScaleAnimation->targetPageScaleFactor());
}

void LayerTreeHostImpl::computePinchZoomDeltas(ScrollAndScaleSet* scrollInfo)
{
    if (!rootScrollLayer())
        return;

    // Only send fake scroll/zoom deltas if we're pinch zooming out by a
    // significant amount. This also ensures only one fake delta set will be
    // sent.
    const float pinchZoomOutSensitivity = 0.95f;
    if (m_pinchZoomViewport.page_scale_delta() > pinchZoomOutSensitivity)
        return;

    // Compute where the scroll offset/page scale would be if fully pinch-zoomed
    // out from the anchor point.
    gfx::Vector2dF scrollBegin = rootScrollLayer()->scrollOffset() + rootScrollLayer()->scrollDelta();
    scrollBegin.Scale(m_pinchZoomViewport.page_scale_delta());
    float scaleBegin = m_pinchZoomViewport.total_page_scale_factor();
    float pageScaleDeltaToSend = m_pinchZoomViewport.min_page_scale_factor() / m_pinchZoomViewport.page_scale_factor();
    gfx::SizeF scaledScrollableSize = gfx::ScaleSize(activeTree()->ScrollableSize(), pageScaleDeltaToSend);

    gfx::Vector2d anchorOffset = m_previousPinchAnchor.OffsetFromOrigin();
    gfx::Vector2dF scrollEnd = scrollBegin + anchorOffset;
    scrollEnd.Scale(m_pinchZoomViewport.min_page_scale_factor() / scaleBegin);
    scrollEnd -= anchorOffset;
    scrollEnd.ClampToMax(gfx::RectF(scaledScrollableSize).bottom_right() - gfx::Rect(m_deviceViewportSize).bottom_right());
    scrollEnd.ClampToMin(gfx::Vector2d());
    scrollEnd.Scale(1 / pageScaleDeltaToSend);
    scrollEnd.Scale(m_deviceScaleFactor);

    makeScrollAndScaleSet(scrollInfo, gfx::ToRoundedVector2d(scrollEnd), m_pinchZoomViewport.min_page_scale_factor());
}

void LayerTreeHostImpl::makeScrollAndScaleSet(ScrollAndScaleSet* scrollInfo, gfx::Vector2d scrollOffset, float pageScale)
{
    if (!rootScrollLayer())
        return;

    LayerTreeHostCommon::ScrollUpdateInfo scroll;
    scroll.layerId = rootScrollLayer()->id();
    scroll.scrollDelta = scrollOffset - rootScrollLayer()->scrollOffset();
    scrollInfo->scrolls.push_back(scroll);
    activeTree()->RootScrollLayer()->setSentScrollDelta(scroll.scrollDelta);
    scrollInfo->pageScaleDelta = pageScale / m_pinchZoomViewport.page_scale_factor();
    m_pinchZoomViewport.set_sent_page_scale_delta(scrollInfo->pageScaleDelta);
}

static void collectScrollDeltas(ScrollAndScaleSet* scrollInfo, LayerImpl* layerImpl)
{
    if (!layerImpl)
        return;

    if (!layerImpl->scrollDelta().IsZero()) {
        gfx::Vector2d scrollDelta = gfx::ToFlooredVector2d(layerImpl->scrollDelta());
        LayerTreeHostCommon::ScrollUpdateInfo scroll;
        scroll.layerId = layerImpl->id();
        scroll.scrollDelta = scrollDelta;
        scrollInfo->scrolls.push_back(scroll);
        layerImpl->setSentScrollDelta(scrollDelta);
    }

    for (size_t i = 0; i < layerImpl->children().size(); ++i)
        collectScrollDeltas(scrollInfo, layerImpl->children()[i]);
}

scoped_ptr<ScrollAndScaleSet> LayerTreeHostImpl::processScrollDeltas()
{
    scoped_ptr<ScrollAndScaleSet> scrollInfo(new ScrollAndScaleSet());

    if (m_pinchGestureActive || m_pageScaleAnimation) {
        scrollInfo->pageScaleDelta = 1;
        m_pinchZoomViewport.set_sent_page_scale_delta(1);
        // FIXME(aelias): Make pinch-zoom painting optimization compatible with
        // compositor-side scaling.
        if (!m_settings.pageScalePinchZoomEnabled && m_pinchGestureActive)
            computePinchZoomDeltas(scrollInfo.get());
        else if (m_pageScaleAnimation.get())
            computeDoubleTapZoomDeltas(scrollInfo.get());
        return scrollInfo.Pass();
    }

    collectScrollDeltas(scrollInfo.get(), rootLayer());
    scrollInfo->pageScaleDelta = m_pinchZoomViewport.page_scale_delta();
    m_pinchZoomViewport.set_sent_page_scale_delta(scrollInfo->pageScaleDelta);

    return scrollInfo.Pass();
}

gfx::Transform LayerTreeHostImpl::implTransform() const
{
    return m_pinchZoomViewport.ImplTransform(m_settings.pageScalePinchZoomEnabled);
}

void LayerTreeHostImpl::setFullRootLayerDamage()
{
    if (rootLayer()) {
        RenderSurfaceImpl* renderSurface = rootLayer()->renderSurface();
        if (renderSurface)
            renderSurface->damageTracker()->forceFullDamageNextUpdate();
    }
}

void LayerTreeHostImpl::animatePageScale(base::TimeTicks time)
{
    if (!m_pageScaleAnimation || !rootScrollLayer())
        return;

    double monotonicTime = (time - base::TimeTicks()).InSecondsF();
    gfx::Vector2dF scrollTotal = rootScrollLayer()->scrollOffset() + rootScrollLayer()->scrollDelta();

    setPageScaleDelta(m_pageScaleAnimation->pageScaleFactorAtTime(monotonicTime) / m_pinchZoomViewport.page_scale_factor());
    gfx::Vector2dF nextScroll = m_pageScaleAnimation->scrollOffsetAtTime(monotonicTime);

    if (!m_settings.pageScalePinchZoomEnabled)
        nextScroll.Scale(m_pinchZoomViewport.page_scale_factor());
    rootScrollLayer()->scrollBy(nextScroll - scrollTotal);
    m_client->setNeedsRedrawOnImplThread();
    setNeedsUpdateDrawProperties();

    if (m_pageScaleAnimation->isAnimationCompleteAtTime(monotonicTime)) {
        m_pageScaleAnimation.reset();
        m_client->setNeedsCommitOnImplThread();
    }
}

void LayerTreeHostImpl::animateLayers(base::TimeTicks monotonicTime, base::Time wallClockTime)
{
    if (!m_settings.acceleratedAnimationEnabled || m_animationRegistrar->active_animation_controllers().empty() || !rootLayer())
        return;

    TRACE_EVENT0("cc", "LayerTreeHostImpl::animateLayers");

    double monotonicSeconds = (monotonicTime - base::TimeTicks()).InSecondsF();

    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(new AnimationEventsVector));
    AnimationRegistrar::AnimationControllerMap copy = m_animationRegistrar->active_animation_controllers();
    for (AnimationRegistrar::AnimationControllerMap::iterator iter = copy.begin(); iter != copy.end(); ++iter)
        (*iter).second->animate(monotonicSeconds, events.get());

    if (!events->empty())
        m_client->postAnimationEventsToMainThreadOnImplThread(events.Pass(), wallClockTime);

    m_client->setNeedsRedrawOnImplThread();
    setNeedsUpdateDrawProperties();
    setBackgroundTickingEnabled(!m_visible && !m_animationRegistrar->active_animation_controllers().empty());
}

base::TimeDelta LayerTreeHostImpl::lowFrequencyAnimationInterval() const
{
    return base::TimeDelta::FromSeconds(1);
}

void LayerTreeHostImpl::sendDidLoseOutputSurfaceRecursive(LayerImpl* current)
{
    DCHECK(current);
    current->didLoseOutputSurface();
    if (current->maskLayer())
        sendDidLoseOutputSurfaceRecursive(current->maskLayer());
    if (current->replicaLayer())
        sendDidLoseOutputSurfaceRecursive(current->replicaLayer());
    for (size_t i = 0; i < current->children().size(); ++i)
        sendDidLoseOutputSurfaceRecursive(current->children()[i]);
}

void LayerTreeHostImpl::clearRenderSurfaces()
{
    activeTree()->ClearRenderSurfaces();
    if (pendingTree())
        pendingTree()->ClearRenderSurfaces();
}

std::string LayerTreeHostImpl::layerTreeAsText() const
{
    std::string str;
    if (rootLayer()) {
        str = rootLayer()->layerTreeAsText();
        str +=  "RenderSurfaces:\n";
        dumpRenderSurfaces(&str, 1, rootLayer());
    }
    return str;
}

std::string LayerTreeHostImpl::layerTreeAsJson() const
{
    std::string str;
    if (rootLayer()) {
        scoped_ptr<base::Value> json(rootLayer()->layerTreeAsJson());
        base::JSONWriter::WriteWithOptions(
            json.get(), base::JSONWriter::OPTIONS_PRETTY_PRINT, &str);
    }
    return str;
}

void LayerTreeHostImpl::dumpRenderSurfaces(std::string* str, int indent, const LayerImpl* layer) const
{
    if (layer->renderSurface())
        layer->renderSurface()->dumpSurface(str, indent);

    for (size_t i = 0; i < layer->children().size(); ++i)
        dumpRenderSurfaces(str, indent, layer->children()[i]);
}

int LayerTreeHostImpl::sourceAnimationFrameNumber() const
{
    return fpsCounter()->currentFrameNumber();
}

void LayerTreeHostImpl::renderingStats(RenderingStats* stats) const
{
    stats->numFramesSentToScreen = fpsCounter()->currentFrameNumber();
    stats->droppedFrameCount = fpsCounter()->droppedFrameCount();
    stats->numImplThreadScrolls = m_numImplThreadScrolls;
    stats->numMainThreadScrolls = m_numMainThreadScrolls;
    stats->numLayersDrawn = m_cumulativeNumLayersDrawn;
    stats->numMissingTiles = m_cumulativeNumMissingTiles;

    if (m_tileManager)
        m_tileManager->GetRenderingStats(stats);
}

void LayerTreeHostImpl::sendManagedMemoryStats(
    size_t memoryVisibleBytes,
    size_t memoryVisibleAndNearbyBytes,
    size_t memoryUseBytes)
{
    if (!renderer())
      return;

    // Round the numbers being sent up to the next 8MB, to throttle the rate
    // at which we spam the GPU process.
    static const size_t roundingStep = 8 * 1024 * 1024;
    memoryVisibleBytes = RoundUp(memoryVisibleBytes, roundingStep);
    memoryVisibleAndNearbyBytes = RoundUp(memoryVisibleAndNearbyBytes, roundingStep);
    memoryUseBytes = RoundUp(memoryUseBytes, roundingStep);
    if (m_lastSentMemoryVisibleBytes == memoryVisibleBytes &&
        m_lastSentMemoryVisibleAndNearbyBytes == memoryVisibleAndNearbyBytes &&
        m_lastSentMemoryUseBytes == memoryUseBytes) {
        return;
    }
    m_lastSentMemoryVisibleBytes = memoryVisibleBytes;
    m_lastSentMemoryVisibleAndNearbyBytes = memoryVisibleAndNearbyBytes;
    m_lastSentMemoryUseBytes = memoryUseBytes;

    renderer()->sendManagedMemoryStats(m_lastSentMemoryVisibleBytes,
                                       m_lastSentMemoryVisibleAndNearbyBytes,
                                       m_lastSentMemoryUseBytes);
}

void LayerTreeHostImpl::animateScrollbars(base::TimeTicks time)
{
    animateScrollbarsRecursive(rootLayer(), time);
}

void LayerTreeHostImpl::animateScrollbarsRecursive(LayerImpl* layer, base::TimeTicks time)
{
    if (!layer)
        return;

    ScrollbarAnimationController* scrollbarController = layer->scrollbarAnimationController();
    if (scrollbarController && scrollbarController->animate(time))
        m_client->setNeedsRedrawOnImplThread();

    for (size_t i = 0; i < layer->children().size(); ++i)
        animateScrollbarsRecursive(layer->children()[i], time);
}

void LayerTreeHostImpl::setTreePriority(TreePriority priority)
{
    if (!m_tileManager)
        return;

    GlobalStateThatImpactsTilePriority new_state(m_tileManager->GlobalState());
    if (new_state.tree_priority == priority)
        return;

    TRACE_COUNTER_ID1("cc", "TreePriority", this, priority);
    new_state.tree_priority = priority;
    m_tileManager->SetGlobalState(new_state);
}

// static
LayerImpl* LayerTreeHostImpl::getNonCompositedContentLayerRecursive(LayerImpl* layer)
{
    if (!layer)
        return NULL;

    if (layer->drawsContent())
        return layer;

    for (LayerImpl::LayerList::const_iterator it = layer->children().begin();
            it != layer->children().end(); ++it) {
        LayerImpl* nccr = getNonCompositedContentLayerRecursive(*it);
        if (nccr)
            return nccr;
    }

    return NULL;
}

skia::RefPtr<SkPicture> LayerTreeHostImpl::capturePicture()
{
    LayerTreeImpl* tree = pendingTree() ? pendingTree() : activeTree();
    LayerImpl* layer = getNonCompositedContentLayerRecursive(tree->RootLayer());
    return layer ? layer->getPicture() : skia::RefPtr<SkPicture>();
}

void LayerTreeHostImpl::savePaintTime(const base::TimeDelta& totalPaintTime)
{
    m_paintTimeCounter->SavePaintTime(totalPaintTime);
}

}  // namespace cc
