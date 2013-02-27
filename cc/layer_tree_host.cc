// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "cc/animation_registrar.h"
#include "cc/heads_up_display_layer.h"
#include "cc/heads_up_display_layer_impl.h"
#include "cc/layer.h"
#include "cc/layer_animation_controller.h"
#include "cc/layer_iterator.h"
#include "cc/layer_tree_host_client.h"
#include "cc/layer_tree_host_common.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/layer_tree_impl.h"
#include "cc/math_util.h"
#include "cc/occlusion_tracker.h"
#include "cc/overdraw_metrics.h"
#include "cc/prioritized_resource_manager.h"
#include "cc/single_thread_proxy.h"
#include "cc/switches.h"
#include "cc/thread.h"
#include "cc/thread_proxy.h"
#include "cc/top_controls_manager.h"
#include "cc/tree_synchronizer.h"

namespace {
static int numLayerTreeInstances;
}

namespace cc {

RendererCapabilities::RendererCapabilities()
    : bestTextureFormat(0)
    , usingPartialSwap(false)
    , usingAcceleratedPainting(false)
    , usingSetVisibility(false)
    , usingSwapCompleteCallback(false)
    , usingGpuMemoryManager(false)
    , usingDiscardBackbuffer(false)
    , usingEglImage(false)
    , allowPartialTextureUpdates(false)
    , usingOffscreenContext3d(false)
    , maxTextureSize(0)
    , avoidPow2Textures(false)
{
}

RendererCapabilities::~RendererCapabilities()
{
}

bool LayerTreeHost::anyLayerTreeHostInstanceExists()
{
    return numLayerTreeInstances > 0;
}

scoped_ptr<LayerTreeHost> LayerTreeHost::create(LayerTreeHostClient* client, const LayerTreeSettings& settings, scoped_ptr<Thread> implThread)
{
    scoped_ptr<LayerTreeHost> layerTreeHost(new LayerTreeHost(client, settings));
    if (!layerTreeHost->initialize(implThread.Pass()))
        return scoped_ptr<LayerTreeHost>();
    return layerTreeHost.Pass();
}

LayerTreeHost::LayerTreeHost(LayerTreeHostClient* client, const LayerTreeSettings& settings)
    : m_animating(false)
    , m_needsFullTreeSync(true)
    , m_needsFilterContext(false)
    , m_client(client)
    , m_commitNumber(0)
    , m_renderingStats()
    , m_rendererInitialized(false)
    , m_outputSurfaceLost(false)
    , m_numFailedRecreateAttempts(0)
    , m_settings(settings)
    , m_debugState(settings.initialDebugState)
    , m_deviceScaleFactor(1)
    , m_visible(true)
    , m_pageScaleFactor(1)
    , m_minPageScaleFactor(1)
    , m_maxPageScaleFactor(1)
    , m_triggerIdleUpdates(true)
    , m_backgroundColor(SK_ColorWHITE)
    , m_hasTransparentBackground(false)
    , m_partialTextureUpdateRequests(0)
    , m_animationRegistrar(AnimationRegistrar::create())
{
    numLayerTreeInstances++;
}

bool LayerTreeHost::initialize(scoped_ptr<Thread> implThread)
{
    if (implThread)
        return initializeProxy(ThreadProxy::create(this, implThread.Pass()));
    else
        return initializeProxy(SingleThreadProxy::create(this));
}

bool LayerTreeHost::initializeForTesting(scoped_ptr<Proxy> proxyForTesting)
{
    return initializeProxy(proxyForTesting.Pass());
}

bool LayerTreeHost::initializeProxy(scoped_ptr<Proxy> proxy)
{
    TRACE_EVENT0("cc", "LayerTreeHost::initializeForReal");

    m_proxy = proxy.Pass();
    m_proxy->start();
    return m_proxy->initializeOutputSurface();
}

LayerTreeHost::~LayerTreeHost()
{
    if (m_rootLayer)
        m_rootLayer->setLayerTreeHost(0);
    DCHECK(m_proxy);
    DCHECK(m_proxy->isMainThread());
    TRACE_EVENT0("cc", "LayerTreeHost::~LayerTreeHost");
    m_proxy->stop();
    numLayerTreeInstances--;
    RateLimiterMap::iterator it = m_rateLimiters.begin();
    if (it != m_rateLimiters.end())
        it->second->stop();

    if (m_rootLayer) {
        // The layer tree must be destroyed before the layer tree host. We've
        // made a contract with our animation controllers that the registrar
        // will outlive them, and we must make good.
        m_rootLayer = NULL;
    }
}

void LayerTreeHost::setSurfaceReady()
{
    m_proxy->setSurfaceReady();
}

void LayerTreeHost::initializeRenderer()
{
    TRACE_EVENT0("cc", "LayerTreeHost::initializeRenderer");
    if (!m_proxy->initializeRenderer()) {
        // Uh oh, better tell the client that we can't do anything with this output surface.
        m_client->didRecreateOutputSurface(false);
        return;
    }

    // Update m_settings based on capabilities that we got back from the renderer.
    m_settings.acceleratePainting = m_proxy->rendererCapabilities().usingAcceleratedPainting;

    // Update m_settings based on partial update capability.
    size_t maxPartialTextureUpdates = 0;
    if (m_proxy->rendererCapabilities().allowPartialTextureUpdates && !m_settings.implSidePainting)
        maxPartialTextureUpdates = std::min(m_settings.maxPartialTextureUpdates, m_proxy->maxPartialTextureUpdates());
    m_settings.maxPartialTextureUpdates = maxPartialTextureUpdates;

    m_contentsTextureManager = PrioritizedResourceManager::create(m_proxy.get());
    m_surfaceMemoryPlaceholder = m_contentsTextureManager->createTexture(gfx::Size(), GL_RGBA);

    m_rendererInitialized = true;

    m_settings.defaultTileSize = gfx::Size(std::min(m_settings.defaultTileSize.width(), m_proxy->rendererCapabilities().maxTextureSize),
                                           std::min(m_settings.defaultTileSize.height(), m_proxy->rendererCapabilities().maxTextureSize));
    m_settings.maxUntiledLayerSize = gfx::Size(std::min(m_settings.maxUntiledLayerSize.width(), m_proxy->rendererCapabilities().maxTextureSize),
                                               std::min(m_settings.maxUntiledLayerSize.height(), m_proxy->rendererCapabilities().maxTextureSize));
}

LayerTreeHost::RecreateResult LayerTreeHost::recreateOutputSurface()
{
    TRACE_EVENT0("cc", "LayerTreeHost::recreateOutputSurface");
    DCHECK(m_outputSurfaceLost);

    if (m_proxy->recreateOutputSurface()) {
        m_client->didRecreateOutputSurface(true);
        m_outputSurfaceLost = false;
        return RecreateSucceeded;
    }

    m_client->willRetryRecreateOutputSurface();

    // Tolerate a certain number of recreation failures to work around races
    // in the output-surface-lost machinery.
    m_numFailedRecreateAttempts++;
    if (m_numFailedRecreateAttempts < 5) {
        // FIXME: The single thread does not self-schedule output surface
        // recreation. So force another recreation attempt to happen by requesting
        // another commit.
        if (!m_proxy->hasImplThread())
            setNeedsCommit();
        return RecreateFailedButTryAgain;
    }

    // We have tried too many times to recreate the output surface. Tell the
    // host to fall back to software rendering.
    m_client->didRecreateOutputSurface(false);
    return RecreateFailedAndGaveUp;
}

void LayerTreeHost::deleteContentsTexturesOnImplThread(ResourceProvider* resourceProvider)
{
    DCHECK(m_proxy->isImplThread());
    if (m_rendererInitialized)
        m_contentsTextureManager->clearAllMemory(resourceProvider);
}

void LayerTreeHost::acquireLayerTextures()
{
    DCHECK(m_proxy->isMainThread());
    m_proxy->acquireLayerTextures();
}

void LayerTreeHost::didBeginFrame()
{
    m_client->didBeginFrame();
}

void LayerTreeHost::updateAnimations(base::TimeTicks frameBeginTime)
{
    m_animating = true;
    m_client->animate((frameBeginTime - base::TimeTicks()).InSecondsF());
    animateLayers(frameBeginTime);
    m_animating = false;

    m_renderingStats.numAnimationFrames++;
}

void LayerTreeHost::didStopFlinging()
{
  m_proxy->mainThreadHasStoppedFlinging();
}

void LayerTreeHost::layout()
{
    m_client->layout();
}

void LayerTreeHost::beginCommitOnImplThread(LayerTreeHostImpl* hostImpl)
{
    DCHECK(m_proxy->isImplThread());
    TRACE_EVENT0("cc", "LayerTreeHost::commitTo");
}

// This function commits the LayerTreeHost to an impl tree. When modifying
// this function, keep in mind that the function *runs* on the impl thread! Any
// code that is logically a main thread operation, e.g. deletion of a Layer,
// should be delayed until the LayerTreeHost::commitComplete, which will run
// after the commit, but on the main thread.
void LayerTreeHost::finishCommitOnImplThread(LayerTreeHostImpl* hostImpl)
{
    DCHECK(m_proxy->isImplThread());

    // If there are linked evicted backings, these backings' resources may be put into the
    // impl tree, so we can't draw yet. Determine this before clearing all evicted backings.
    bool newImplTreeHasNoEvictedResources = !m_contentsTextureManager->linkedEvictedBackingsExist();

    m_contentsTextureManager->updateBackingsInDrawingImplTree();

    // In impl-side painting, synchronize to the pending tree so that it has
    // time to raster before being displayed.  If no pending tree is needed,
    // synchronization can happen directly to the active tree and
    // unlinked contents resources can be reclaimed immediately.
    LayerTreeImpl* syncTree;
    if (m_settings.implSidePainting) {
        // Commits should not occur while there is already a pending tree.
        DCHECK(!hostImpl->pendingTree());
        hostImpl->createPendingTree();
        syncTree = hostImpl->pendingTree();
    } else {
        m_contentsTextureManager->reduceMemory(hostImpl->resourceProvider());
        syncTree = hostImpl->activeTree();
    }

    if (m_needsFullTreeSync)
        syncTree->SetRootLayer(TreeSynchronizer::synchronizeTrees(rootLayer(), syncTree->DetachLayerTree(), syncTree));
    {
        TRACE_EVENT0("cc", "LayerTreeHost::pushProperties");
        TreeSynchronizer::pushProperties(rootLayer(), syncTree->RootLayer());
    }

    syncTree->set_needs_full_tree_sync(m_needsFullTreeSync);
    m_needsFullTreeSync = false;

    if (m_rootLayer && m_hudLayer)
        syncTree->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(LayerTreeHostCommon::findLayerInSubtree(syncTree->RootLayer(), m_hudLayer->id())));
    else
        syncTree->set_hud_layer(0);

    syncTree->set_source_frame_number(commitNumber());
    syncTree->set_background_color(m_backgroundColor);
    syncTree->set_has_transparent_background(m_hasTransparentBackground);

    syncTree->FindRootScrollLayer();

    float page_scale_delta, sent_page_scale_delta;
    if (m_settings.implSidePainting) {
        // Update the delta from the active tree, which may have
        // adjusted its delta prior to the pending tree being created.
        // This code is equivalent to that in LayerTreeImpl::SetPageScaleDelta.
        DCHECK_EQ(1, syncTree->sent_page_scale_delta());
        page_scale_delta = hostImpl->activeTree()->page_scale_delta();
        sent_page_scale_delta = hostImpl->activeTree()->sent_page_scale_delta();
    } else {
        page_scale_delta = syncTree->page_scale_delta();
        sent_page_scale_delta = syncTree->sent_page_scale_delta();
        syncTree->set_sent_page_scale_delta(1);
    }

    syncTree->SetPageScaleFactorAndLimits(m_pageScaleFactor, m_minPageScaleFactor, m_maxPageScaleFactor);
    syncTree->SetPageScaleDelta(page_scale_delta / sent_page_scale_delta);

    hostImpl->setViewportSize(layoutViewportSize(), deviceViewportSize());
    hostImpl->setDeviceScaleFactor(deviceScaleFactor());
    hostImpl->setDebugState(m_debugState);

    DCHECK(!syncTree->ViewportSizeInvalid());

    if (newImplTreeHasNoEvictedResources) {
        if (syncTree->ContentsTexturesPurged())
            syncTree->ResetContentsTexturesPurged();
    }

    if (!m_settings.implSidePainting) {
        // If we're not in impl-side painting, the tree is immediately
        // considered active.
        syncTree->DidBecomeActive();
    }

    if (m_debugState.continuousPainting)
        hostImpl->savePaintTime(m_renderingStats.totalPaintTime, commitNumber());

    m_commitNumber++;
}

void LayerTreeHost::willCommit()
{
    m_client->willCommit();
}

void LayerTreeHost::updateHudLayer()
{
    if (m_debugState.showHudInfo()) {
        if (!m_hudLayer)
            m_hudLayer = HeadsUpDisplayLayer::create();

        if (m_rootLayer && !m_hudLayer->parent())
            m_rootLayer->addChild(m_hudLayer);
    } else if (m_hudLayer) {
        m_hudLayer->removeFromParent();
        m_hudLayer = 0;
    }
}

void LayerTreeHost::commitComplete()
{
    m_client->didCommit();
}

scoped_ptr<OutputSurface> LayerTreeHost::createOutputSurface()
{
    return m_client->createOutputSurface();
}

scoped_ptr<InputHandler> LayerTreeHost::createInputHandler()
{
    return m_client->createInputHandler();
}

scoped_ptr<LayerTreeHostImpl> LayerTreeHost::createLayerTreeHostImpl(LayerTreeHostImplClient* client)
{
    DCHECK(m_proxy->isImplThread());
    scoped_ptr<LayerTreeHostImpl> hostImpl(LayerTreeHostImpl::create(m_settings, client, m_proxy.get()));
    if (m_settings.calculateTopControlsPosition && hostImpl->topControlsManager())
        m_topControlsManagerWeakPtr = hostImpl->topControlsManager()->AsWeakPtr();
    return hostImpl.Pass();
}

void LayerTreeHost::didLoseOutputSurface()
{
    TRACE_EVENT0("cc", "LayerTreeHost::didLoseOutputSurface");
    DCHECK(m_proxy->isMainThread());
    m_outputSurfaceLost = true;
    m_numFailedRecreateAttempts = 0;
    setNeedsCommit();
}

bool LayerTreeHost::compositeAndReadback(void *pixels, const gfx::Rect& rect)
{
    m_triggerIdleUpdates = false;
    bool ret = m_proxy->compositeAndReadback(pixels, rect);
    m_triggerIdleUpdates = true;
    return ret;
}

void LayerTreeHost::finishAllRendering()
{
    if (!m_rendererInitialized)
        return;
    m_proxy->finishAllRendering();
}

void LayerTreeHost::setDeferCommits(bool deferCommits)
{
    m_proxy->setDeferCommits(deferCommits);
}

void LayerTreeHost::didDeferCommit()
{
}

void LayerTreeHost::renderingStats(RenderingStats* stats) const
{
    CHECK(m_debugState.recordRenderingStats());
    *stats = m_renderingStats;
    m_proxy->renderingStats(stats);
}

const RendererCapabilities& LayerTreeHost::rendererCapabilities() const
{
    return m_proxy->rendererCapabilities();
}

void LayerTreeHost::setNeedsAnimate()
{
    DCHECK(m_proxy->hasImplThread());
    m_proxy->setNeedsAnimate();
}

void LayerTreeHost::setNeedsCommit()
{
    if (!m_prepaintCallback.IsCancelled()) {
        TRACE_EVENT_INSTANT0("cc", "LayerTreeHost::setNeedsCommit::cancel prepaint");
        m_prepaintCallback.Cancel();
    }
    m_proxy->setNeedsCommit();
}

void LayerTreeHost::setNeedsFullTreeSync()
{
    m_needsFullTreeSync = true;
    setNeedsCommit();
}

void LayerTreeHost::setNeedsRedraw()
{
    m_proxy->setNeedsRedraw();
    if (!m_proxy->implThread())
        m_client->scheduleComposite();
}

bool LayerTreeHost::commitRequested() const
{
    return m_proxy->commitRequested();
}

void LayerTreeHost::setAnimationEvents(scoped_ptr<AnimationEventsVector> events, base::Time wallClockTime)
{
    DCHECK(m_proxy->isMainThread());
    setAnimationEventsRecursive(*events.get(), m_rootLayer.get(), wallClockTime);
}

void LayerTreeHost::setRootLayer(scoped_refptr<Layer> rootLayer)
{
    if (m_rootLayer == rootLayer)
        return;

    if (m_rootLayer)
        m_rootLayer->setLayerTreeHost(0);
    m_rootLayer = rootLayer;
    if (m_rootLayer)
        m_rootLayer->setLayerTreeHost(this);

    if (m_hudLayer)
        m_hudLayer->removeFromParent();

    setNeedsFullTreeSync();
}

void LayerTreeHost::setDebugState(const LayerTreeDebugState& debugState)
{
    LayerTreeDebugState newDebugState = LayerTreeDebugState::unite(m_settings.initialDebugState, debugState);

    if (LayerTreeDebugState::equal(m_debugState, newDebugState))
        return;

    m_debugState = newDebugState;
    setNeedsCommit();
}

void LayerTreeHost::setViewportSize(const gfx::Size& layoutViewportSize, const gfx::Size& deviceViewportSize)
{
    if (layoutViewportSize == m_layoutViewportSize && deviceViewportSize == m_deviceViewportSize)
        return;

    m_layoutViewportSize = layoutViewportSize;
    m_deviceViewportSize = deviceViewportSize;

    setNeedsCommit();
}

void LayerTreeHost::setPageScaleFactorAndLimits(float pageScaleFactor, float minPageScaleFactor, float maxPageScaleFactor)
{
    if (pageScaleFactor == m_pageScaleFactor && minPageScaleFactor == m_minPageScaleFactor && maxPageScaleFactor == m_maxPageScaleFactor)
        return;

    m_pageScaleFactor = pageScaleFactor;
    m_minPageScaleFactor = minPageScaleFactor;
    m_maxPageScaleFactor = maxPageScaleFactor;
    setNeedsCommit();
}

void LayerTreeHost::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    m_proxy->setVisible(visible);
}

void LayerTreeHost::startPageScaleAnimation(gfx::Vector2d targetOffset, bool useAnchor, float scale, base::TimeDelta duration)
{
    m_proxy->startPageScaleAnimation(targetOffset, useAnchor, scale, duration);
}

PrioritizedResourceManager* LayerTreeHost::contentsTextureManager() const
{
    return m_contentsTextureManager.get();
}

void LayerTreeHost::composite()
{
    if (!m_proxy->hasImplThread())
        static_cast<SingleThreadProxy*>(m_proxy.get())->compositeImmediately();
    else
        setNeedsCommit();
}

void LayerTreeHost::scheduleComposite()
{
    m_client->scheduleComposite();
}

bool LayerTreeHost::initializeRendererIfNeeded()
{
    if (!m_rendererInitialized) {
        initializeRenderer();
        // If we couldn't initialize, then bail since we're returning to software mode.
        if (!m_rendererInitialized)
            return false;
    }
    if (m_outputSurfaceLost) {
        if (recreateOutputSurface() != RecreateSucceeded)
            return false;
    }
    return true;
}

void LayerTreeHost::updateLayers(ResourceUpdateQueue& queue, size_t memoryAllocationLimitBytes)
{
    DCHECK(m_rendererInitialized);

    if (!rootLayer())
        return;

    if (layoutViewportSize().IsEmpty())
        return;

    if (memoryAllocationLimitBytes)
        m_contentsTextureManager->setMaxMemoryLimitBytes(memoryAllocationLimitBytes);

    updateLayers(rootLayer(), queue);
}

static Layer* findFirstScrollableLayer(Layer* layer)
{
    if (!layer)
        return 0;

    if (layer->scrollable())
        return layer;

    for (size_t i = 0; i < layer->children().size(); ++i) {
        Layer* found = findFirstScrollableLayer(layer->children()[i].get());
        if (found)
            return found;
    }

    return 0;
}

void LayerTreeHost::updateLayers(Layer* rootLayer, ResourceUpdateQueue& queue)
{
    TRACE_EVENT0("cc", "LayerTreeHost::updateLayers");

    LayerList updateList;

    {
        Layer* rootScroll = findFirstScrollableLayer(rootLayer);
        if (rootScroll)
            rootScroll->setImplTransform(m_implTransform);

        updateHudLayer();

        TRACE_EVENT0("cc", "LayerTreeHost::updateLayers::calcDrawEtc");
        LayerTreeHostCommon::calculateDrawProperties(rootLayer, deviceViewportSize(), m_deviceScaleFactor, m_pageScaleFactor, rendererCapabilities().maxTextureSize, m_settings.canUseLCDText, updateList);
    }

    // Reset partial texture update requests.
    m_partialTextureUpdateRequests = 0;

    bool needMoreUpdates = paintLayerContents(updateList, queue);
    if (m_triggerIdleUpdates && needMoreUpdates) {
        TRACE_EVENT0("cc", "LayerTreeHost::updateLayers::posting prepaint task");
        m_prepaintCallback.Reset(base::Bind(&LayerTreeHost::triggerPrepaint, base::Unretained(this)));
        static base::TimeDelta prepaintDelay = base::TimeDelta::FromMilliseconds(100);
        MessageLoop::current()->PostDelayedTask(FROM_HERE, m_prepaintCallback.callback(), prepaintDelay);
    }

    for (size_t i = 0; i < updateList.size(); ++i)
        updateList[i]->clearRenderSurface();
}

void LayerTreeHost::triggerPrepaint()
{
    m_prepaintCallback.Cancel();
    TRACE_EVENT0("cc", "LayerTreeHost::triggerPrepaint");
    setNeedsCommit();
}

void LayerTreeHost::setPrioritiesForSurfaces(size_t surfaceMemoryBytes)
{
    // Surfaces have a place holder for their memory since they are managed
    // independantly but should still be tracked and reduce other memory usage.
    m_surfaceMemoryPlaceholder->setTextureManager(m_contentsTextureManager.get());
    m_surfaceMemoryPlaceholder->setRequestPriority(PriorityCalculator::renderSurfacePriority());
    m_surfaceMemoryPlaceholder->setToSelfManagedMemoryPlaceholder(surfaceMemoryBytes);
}

void LayerTreeHost::setPrioritiesForLayers(const LayerList& updateList)
{
    // Use BackToFront since it's cheap and this isn't order-dependent.
    typedef LayerIterator<Layer, LayerList, RenderSurface, LayerIteratorActions::BackToFront> LayerIteratorType;

    PriorityCalculator calculator;
    LayerIteratorType end = LayerIteratorType::end(&updateList);
    for (LayerIteratorType it = LayerIteratorType::begin(&updateList); it != end; ++it) {
        if (it.representsItself())
            it->setTexturePriorities(calculator);
        else if (it.representsTargetRenderSurface()) {
            if (it->maskLayer())
                it->maskLayer()->setTexturePriorities(calculator);
            if (it->replicaLayer() && it->replicaLayer()->maskLayer())
                it->replicaLayer()->maskLayer()->setTexturePriorities(calculator);
        }
    }
}

void LayerTreeHost::prioritizeTextures(const LayerList& renderSurfaceLayerList, OverdrawMetrics& metrics)
{
    m_contentsTextureManager->clearPriorities();

    size_t memoryForRenderSurfacesMetric = calculateMemoryForRenderSurfaces(renderSurfaceLayerList);

    setPrioritiesForLayers(renderSurfaceLayerList);
    setPrioritiesForSurfaces(memoryForRenderSurfacesMetric);

    metrics.didUseContentsTextureMemoryBytes(m_contentsTextureManager->memoryAboveCutoffBytes());
    metrics.didUseRenderSurfaceTextureMemoryBytes(memoryForRenderSurfacesMetric);

    m_contentsTextureManager->prioritizeTextures();
}

size_t LayerTreeHost::calculateMemoryForRenderSurfaces(const LayerList& updateList)
{
    size_t readbackBytes = 0;
    size_t maxBackgroundTextureBytes = 0;
    size_t contentsTextureBytes = 0;

    // Start iteration at 1 to skip the root surface as it does not have a texture cost.
    for (size_t i = 1; i < updateList.size(); ++i) {
        Layer* renderSurfaceLayer = updateList[i].get();
        RenderSurface* renderSurface = renderSurfaceLayer->renderSurface();

        size_t bytes = Resource::MemorySizeBytes(renderSurface->contentRect().size(), GL_RGBA);
        contentsTextureBytes += bytes;

        if (renderSurfaceLayer->backgroundFilters().isEmpty())
            continue;

        if (bytes > maxBackgroundTextureBytes)
            maxBackgroundTextureBytes = bytes;
        if (!readbackBytes)
            readbackBytes = Resource::MemorySizeBytes(m_deviceViewportSize, GL_RGBA);
    }
    return readbackBytes + maxBackgroundTextureBytes + contentsTextureBytes;
}

bool LayerTreeHost::paintMasksForRenderSurface(Layer* renderSurfaceLayer, ResourceUpdateQueue& queue)
{
    // Note: Masks and replicas only exist for layers that own render surfaces. If we reach this point
    // in code, we already know that at least something will be drawn into this render surface, so the
    // mask and replica should be painted.

    RenderingStats* stats = m_debugState.recordRenderingStats() ? &m_renderingStats : NULL;

    bool needMoreUpdates = false;
    Layer* maskLayer = renderSurfaceLayer->maskLayer();
    if (maskLayer) {
        maskLayer->update(queue, 0, stats);
        needMoreUpdates |= maskLayer->needMoreUpdates();
    }

    Layer* replicaMaskLayer = renderSurfaceLayer->replicaLayer() ? renderSurfaceLayer->replicaLayer()->maskLayer() : 0;
    if (replicaMaskLayer) {
        replicaMaskLayer->update(queue, 0, stats);
        needMoreUpdates |= replicaMaskLayer->needMoreUpdates();
    }
    return needMoreUpdates;
}

bool LayerTreeHost::paintLayerContents(const LayerList& renderSurfaceLayerList, ResourceUpdateQueue& queue)
{
    // Use FrontToBack to allow for testing occlusion and performing culling during the tree walk.
    typedef LayerIterator<Layer, LayerList, RenderSurface, LayerIteratorActions::FrontToBack> LayerIteratorType;

    bool needMoreUpdates = false;
    bool recordMetricsForFrame = m_settings.showOverdrawInTracing && base::debug::TraceLog::GetInstance() && base::debug::TraceLog::GetInstance()->IsEnabled();
    OcclusionTracker occlusionTracker(m_rootLayer->renderSurface()->contentRect(), recordMetricsForFrame);
    occlusionTracker.setMinimumTrackingSize(m_settings.minimumOcclusionTrackingSize);

    prioritizeTextures(renderSurfaceLayerList, occlusionTracker.overdrawMetrics());

    RenderingStats* stats = m_debugState.recordRenderingStats() ? &m_renderingStats : NULL;

    LayerIteratorType end = LayerIteratorType::end(&renderSurfaceLayerList);
    for (LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList); it != end; ++it) {
        occlusionTracker.enterLayer(it);

        if (it.representsTargetRenderSurface()) {
            DCHECK(it->renderSurface()->drawOpacity() || it->renderSurface()->drawOpacityIsAnimating());
            needMoreUpdates |= paintMasksForRenderSurface(*it, queue);
        } else if (it.representsItself()) {
            DCHECK(!it->bounds().IsEmpty());
            it->update(queue, &occlusionTracker, stats);
            needMoreUpdates |= it->needMoreUpdates();
        }

        occlusionTracker.leaveLayer(it);
    }

    occlusionTracker.overdrawMetrics().recordMetrics(this);

    return needMoreUpdates;
}

void LayerTreeHost::applyScrollAndScale(const ScrollAndScaleSet& info)
{
    if (!m_rootLayer)
        return;

    Layer* rootScrollLayer = findFirstScrollableLayer(m_rootLayer.get());
    gfx::Vector2d rootScrollDelta;

    for (size_t i = 0; i < info.scrolls.size(); ++i) {
        Layer* layer = LayerTreeHostCommon::findLayerInSubtree(m_rootLayer.get(), info.scrolls[i].layerId);
        if (!layer)
            continue;
        if (layer == rootScrollLayer)
            rootScrollDelta += info.scrolls[i].scrollDelta;
        else
            layer->setScrollOffset(layer->scrollOffset() + info.scrolls[i].scrollDelta);
    }
    if (!rootScrollDelta.IsZero() || info.pageScaleDelta != 1)
        m_client->applyScrollAndScale(rootScrollDelta, info.pageScaleDelta);
}

void LayerTreeHost::setImplTransform(const gfx::Transform& transform)
{
    m_implTransform = transform;
}

void LayerTreeHost::startRateLimiter(WebKit::WebGraphicsContext3D* context)
{
    if (m_animating)
        return;

    DCHECK(context);
    RateLimiterMap::iterator it = m_rateLimiters.find(context);
    if (it != m_rateLimiters.end())
        it->second->start();
    else {
        scoped_refptr<RateLimiter> rateLimiter = RateLimiter::create(context, this, m_proxy->mainThread());
        m_rateLimiters[context] = rateLimiter;
        rateLimiter->start();
    }
}

void LayerTreeHost::stopRateLimiter(WebKit::WebGraphicsContext3D* context)
{
    RateLimiterMap::iterator it = m_rateLimiters.find(context);
    if (it != m_rateLimiters.end()) {
        it->second->stop();
        m_rateLimiters.erase(it);
    }
}

void LayerTreeHost::rateLimit()
{
    // Force a no-op command on the compositor context, so that any ratelimiting commands will wait for the compositing
    // context, and therefore for the SwapBuffers.
    m_proxy->forceSerializeOnSwapBuffers();
}

bool LayerTreeHost::bufferedUpdates()
{
    return m_settings.maxPartialTextureUpdates != std::numeric_limits<size_t>::max();
}

bool LayerTreeHost::requestPartialTextureUpdate()
{
    if (m_partialTextureUpdateRequests >= m_settings.maxPartialTextureUpdates)
        return false;

    m_partialTextureUpdateRequests++;
    return true;
}

void LayerTreeHost::setDeviceScaleFactor(float deviceScaleFactor)
{
    if (deviceScaleFactor ==  m_deviceScaleFactor)
        return;
    m_deviceScaleFactor = deviceScaleFactor;

    setNeedsCommit();
}

void LayerTreeHost::enableHidingTopControls(bool enable)
{
    if (!m_settings.calculateTopControlsPosition)
        return;

    m_proxy->implThread()->postTask(
        base::Bind(&TopControlsManager::enable_hiding_top_controls,
                   m_topControlsManagerWeakPtr, enable));
}

bool LayerTreeHost::blocksPendingCommit() const
{
    if (!m_rootLayer)
        return false;
    return m_rootLayer->blocksPendingCommitRecursive();
}

scoped_ptr<base::Value> LayerTreeHost::asValue() const
{
    scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
    state->Set("proxy", m_proxy->asValue().release());
    return state.PassAs<base::Value>();
}

void LayerTreeHost::animateLayers(base::TimeTicks time)
{
    if (!m_settings.acceleratedAnimationEnabled || m_animationRegistrar->active_animation_controllers().empty())
        return;

    TRACE_EVENT0("cc", "LayerTreeHostImpl::animateLayers");

    double monotonicTime = (time - base::TimeTicks()).InSecondsF();

    AnimationRegistrar::AnimationControllerMap copy = m_animationRegistrar->active_animation_controllers();
    for (AnimationRegistrar::AnimationControllerMap::iterator iter = copy.begin(); iter != copy.end(); ++iter) {
        (*iter).second->animate(monotonicTime);
        (*iter).second->updateState(0);
    }
}

void LayerTreeHost::setAnimationEventsRecursive(const AnimationEventsVector& events, Layer* layer, base::Time wallClockTime)
{
    if (!layer)
        return;

    for (size_t eventIndex = 0; eventIndex < events.size(); ++eventIndex) {
        if (layer->id() == events[eventIndex].layerId) {
            if (events[eventIndex].type == AnimationEvent::Started)
                layer->notifyAnimationStarted(events[eventIndex], wallClockTime.ToDoubleT());
            else
                layer->notifyAnimationFinished(wallClockTime.ToDoubleT());
        }
    }

    for (size_t childIndex = 0; childIndex < layer->children().size(); ++childIndex)
        setAnimationEventsRecursive(events, layer->children()[childIndex].get(), wallClockTime);
}

skia::RefPtr<SkPicture> LayerTreeHost::capturePicture()
{
    return m_proxy->capturePicture();
}

}  // namespace cc
