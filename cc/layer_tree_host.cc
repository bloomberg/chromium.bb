// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host.h"

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "cc/font_atlas.h"
#include "cc/graphics_context.h"
#include "cc/heads_up_display_layer.h"
#include "cc/heads_up_display_layer_impl.h"
#include "cc/layer.h"
#include "cc/layer_animation_controller.h"
#include "cc/layer_iterator.h"
#include "cc/layer_tree_host_client.h"
#include "cc/layer_tree_host_common.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/math_util.h"
#include "cc/occlusion_tracker.h"
#include "cc/overdraw_metrics.h"
#include "cc/single_thread_proxy.h"
#include "cc/switches.h"
#include "cc/thread.h"
#include "cc/thread_proxy.h"
#include "cc/tree_synchronizer.h"

using namespace std;

namespace {
static int numLayerTreeInstances;
}

namespace cc {

bool LayerTreeHost::s_needsFilterContext = false;

LayerTreeDebugState::LayerTreeDebugState()
    : showFPSCounter(false)
    , showPlatformLayerTree(false)
    , showDebugBorders(false)
    , showPaintRects(false)
    , showPropertyChangedRects(false)
    , showSurfaceDamageRects(false)
    , showScreenSpaceRects(false)
    , showReplicaScreenSpaceRects(false)
    , showOccludingRects(false)
    , showNonOccludingRects(false)
{
}

LayerTreeDebugState::~LayerTreeDebugState()
{
}

bool LayerTreeDebugState::showHudInfo() const
{
    return showFPSCounter || showPlatformLayerTree || showHudRects();
}

bool LayerTreeDebugState::showHudRects() const
{
    return showPaintRects || showPropertyChangedRects || showSurfaceDamageRects || showScreenSpaceRects || showReplicaScreenSpaceRects || showOccludingRects || showNonOccludingRects;
}

bool LayerTreeDebugState::hudNeedsFont() const
{
    return showFPSCounter || showPlatformLayerTree;
}

bool LayerTreeDebugState::equal(const LayerTreeDebugState& a, const LayerTreeDebugState& b)
{
    return memcmp(&a, &b, sizeof(LayerTreeDebugState)) == 0;
}

LayerTreeDebugState LayerTreeDebugState::unite(const LayerTreeDebugState& a, const LayerTreeDebugState& b)
{
    LayerTreeDebugState r(a);

    r.showFPSCounter |= b.showFPSCounter;
    r.showPlatformLayerTree |= b.showPlatformLayerTree;
    r.showDebugBorders |= b.showDebugBorders;

    r.showPaintRects |= b.showPaintRects;
    r.showPropertyChangedRects |= b.showPropertyChangedRects;
    r.showSurfaceDamageRects |= b.showSurfaceDamageRects;
    r.showScreenSpaceRects |= b.showScreenSpaceRects;
    r.showReplicaScreenSpaceRects |= b.showReplicaScreenSpaceRects;
    r.showOccludingRects |= b.showOccludingRects;
    r.showNonOccludingRects |= b.showNonOccludingRects;

    return r;
}

LayerTreeSettings::LayerTreeSettings()
    : acceleratePainting(false)
    , implSidePainting(false)
    , renderVSyncEnabled(true)
    , perTilePaintingEnabled(false)
    , partialSwapEnabled(false)
    , acceleratedAnimationEnabled(true)
    , pageScalePinchZoomEnabled(false)
    , backgroundColorInsteadOfCheckerboard(false)
    , showOverdrawInTracing(false)
    , refreshRate(0)
    , maxPartialTextureUpdates(std::numeric_limits<size_t>::max())
    , defaultTileSize(gfx::Size(256, 256))
    , maxUntiledLayerSize(gfx::Size(512, 512))
    , minimumOcclusionTrackingSize(gfx::Size(160, 160))
{
    // TODO(danakj): Move this to chromium when we don't go through the WebKit API anymore.
    implSidePainting = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kEnableImplSidePainting);
    partialSwapEnabled = CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnablePartialSwap);
    backgroundColorInsteadOfCheckerboard = CommandLine::ForCurrentProcess()->HasSwitch(switches::kBackgroundColorInsteadOfCheckerboard);
    showOverdrawInTracing = CommandLine::ForCurrentProcess()->HasSwitch(switches::kTraceOverdraw);

    initialDebugState.showPropertyChangedRects = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kShowPropertyChangedRects);
    initialDebugState.showSurfaceDamageRects = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kShowSurfaceDamageRects);
    initialDebugState.showScreenSpaceRects = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kShowScreenSpaceRects);
    initialDebugState.showReplicaScreenSpaceRects = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kShowReplicaScreenSpaceRects);
    initialDebugState.showOccludingRects = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kShowOccludingRects);
    initialDebugState.showNonOccludingRects = CommandLine::ForCurrentProcess()->HasSwitch(cc::switches::kShowNonOccludingRects);
}

LayerTreeSettings::~LayerTreeSettings()
{
}

RendererCapabilities::RendererCapabilities()
    : bestTextureFormat(0)
    , contextHasCachedFrontBuffer(false)
    , usingPartialSwap(false)
    , usingAcceleratedPainting(false)
    , usingSetVisibility(false)
    , usingSwapCompleteCallback(false)
    , usingGpuMemoryManager(false)
    , usingDiscardFramebuffer(false)
    , usingEglImage(false)
    , maxTextureSize(0)
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
    , m_needsAnimateLayers(false)
    , m_client(client)
    , m_commitNumber(0)
    , m_renderingStats()
    , m_rendererInitialized(false)
    , m_contextLost(false)
    , m_numTimesRecreateShouldFail(0)
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
{
    numLayerTreeInstances++;
}

bool LayerTreeHost::initialize(scoped_ptr<Thread> implThread)
{
    TRACE_EVENT0("cc", "LayerTreeHost::initialize");

    if (implThread)
        m_proxy = ThreadProxy::create(this, implThread.Pass());
    else
        m_proxy = SingleThreadProxy::create(this);
    m_proxy->start();

    return m_proxy->initializeContext();
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
}

void LayerTreeHost::setSurfaceReady()
{
    m_proxy->setSurfaceReady();
}

void LayerTreeHost::initializeRenderer()
{
    TRACE_EVENT0("cc", "LayerTreeHost::initializeRenderer");
    if (!m_proxy->initializeRenderer()) {
        // Uh oh, better tell the client that we can't do anything with this context.
        m_client->didRecreateOutputSurface(false);
        return;
    }

    // Update m_settings based on capabilities that we got back from the renderer.
    m_settings.acceleratePainting = m_proxy->rendererCapabilities().usingAcceleratedPainting;

    // Update m_settings based on partial update capability.
    m_settings.maxPartialTextureUpdates = min(m_settings.maxPartialTextureUpdates, m_proxy->maxPartialTextureUpdates());

    m_contentsTextureManager = PrioritizedResourceManager::create(Renderer::ContentPool, m_proxy.get());
    m_surfaceMemoryPlaceholder = m_contentsTextureManager->createTexture(gfx::Size(), GL_RGBA);

    m_rendererInitialized = true;

    m_settings.defaultTileSize = gfx::Size(min(m_settings.defaultTileSize.width(), m_proxy->rendererCapabilities().maxTextureSize),
                                           min(m_settings.defaultTileSize.height(), m_proxy->rendererCapabilities().maxTextureSize));
    m_settings.maxUntiledLayerSize = gfx::Size(min(m_settings.maxUntiledLayerSize.width(), m_proxy->rendererCapabilities().maxTextureSize),
                                               min(m_settings.maxUntiledLayerSize.height(), m_proxy->rendererCapabilities().maxTextureSize));
}

LayerTreeHost::RecreateResult LayerTreeHost::recreateContext()
{
    TRACE_EVENT0("cc", "LayerTreeHost::recreateContext");
    DCHECK(m_contextLost);

    bool recreated = false;
    if (!m_numTimesRecreateShouldFail)
        recreated = m_proxy->recreateContext();
    else
        m_numTimesRecreateShouldFail--;

    if (recreated) {
        m_client->didRecreateOutputSurface(true);
        m_contextLost = false;
        return RecreateSucceeded;
    }

    // Tolerate a certain number of recreation failures to work around races
    // in the context-lost machinery.
    m_numFailedRecreateAttempts++;
    if (m_numFailedRecreateAttempts < 5) {
        // FIXME: The single thread does not self-schedule context
        // recreation. So force another recreation attempt to happen by requesting
        // another commit.
        if (!m_proxy->hasImplThread())
            setNeedsCommit();
        return RecreateFailedButTryAgain;
    }

    // We have tried too many times to recreate the context. Tell the host to fall
    // back to software rendering.
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

void LayerTreeHost::updateAnimations(base::TimeTicks frameBeginTime)
{
    m_animating = true;
    m_client->animate((frameBeginTime - base::TimeTicks()).InSecondsF());
    animateLayers(frameBeginTime);
    m_animating = false;

    m_renderingStats.numAnimationFrames++;
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

    m_contentsTextureManager->updateBackingsInDrawingImplTree();
    m_contentsTextureManager->reduceMemory(hostImpl->resourceProvider());

    hostImpl->setRootLayer(TreeSynchronizer::synchronizeTrees(rootLayer(), hostImpl->detachLayerTree(), hostImpl));

    if (m_rootLayer && m_hudLayer)
        hostImpl->setHudLayer(static_cast<HeadsUpDisplayLayerImpl*>(LayerTreeHostCommon::findLayerInSubtree(hostImpl->rootLayer(), m_hudLayer->id())));
    else
        hostImpl->setHudLayer(0);

    // We may have added an animation during the tree sync. This will cause both layer tree hosts
    // to visit their controllers.
    if (rootLayer() && m_needsAnimateLayers)
        hostImpl->setNeedsAnimateLayers();

    hostImpl->setSourceFrameNumber(commitNumber());
    hostImpl->setViewportSize(layoutViewportSize(), deviceViewportSize());
    hostImpl->setDeviceScaleFactor(deviceScaleFactor());
    hostImpl->setPageScaleFactorAndLimits(m_pageScaleFactor, m_minPageScaleFactor, m_maxPageScaleFactor);
    hostImpl->setBackgroundColor(m_backgroundColor);
    hostImpl->setHasTransparentBackground(m_hasTransparentBackground);
    hostImpl->setDebugState(m_debugState);

    m_commitNumber++;
}

void LayerTreeHost::willCommit()
{
    m_client->willCommit();

    if (m_debugState.showHudInfo()) {
        if (!m_hudLayer)
            m_hudLayer = HeadsUpDisplayLayer::create();

        if (m_debugState.hudNeedsFont() && !m_hudLayer->hasFontAtlas())
            m_hudLayer->setFontAtlas(m_client->createFontAtlas());

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

scoped_ptr<GraphicsContext> LayerTreeHost::createContext()
{
    return m_client->createOutputSurface();
}

scoped_ptr<InputHandler> LayerTreeHost::createInputHandler()
{
    return m_client->createInputHandler();
}

scoped_ptr<LayerTreeHostImpl> LayerTreeHost::createLayerTreeHostImpl(LayerTreeHostImplClient* client)
{
    return LayerTreeHostImpl::create(m_settings, client, m_proxy.get());
}

void LayerTreeHost::didLoseContext()
{
    TRACE_EVENT0("cc", "LayerTreeHost::didLoseContext");
    DCHECK(m_proxy->isMainThread());
    m_contextLost = true;
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

void LayerTreeHost::didAddAnimation()
{
    m_needsAnimateLayers = true;
    m_proxy->didAddAnimation();
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

    setNeedsCommit();
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

void LayerTreeHost::loseContext(int numTimes)
{
    TRACE_EVENT1("cc", "LayerTreeHost::loseCompositorContext", "numTimes", numTimes);
    m_numTimesRecreateShouldFail = numTimes - 1;
    m_proxy->loseContext();
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
    if (m_contextLost) {
        if (recreateContext() != RecreateSucceeded)
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
        if (m_settings.pageScalePinchZoomEnabled) {
            Layer* rootScroll = findFirstScrollableLayer(rootLayer);
            if (rootScroll)
                rootScroll->setImplTransform(m_implTransform);
        }

        TRACE_EVENT0("cc", "LayerTreeHost::updateLayers::calcDrawEtc");
        LayerTreeHostCommon::calculateDrawTransforms(rootLayer, deviceViewportSize(), m_deviceScaleFactor, m_pageScaleFactor, rendererCapabilities().maxTextureSize, updateList);
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

    bool needMoreUpdates = false;
    Layer* maskLayer = renderSurfaceLayer->maskLayer();
    if (maskLayer) {
        maskLayer->update(queue, 0, m_renderingStats);
        needMoreUpdates |= maskLayer->needMoreUpdates();
    }

    Layer* replicaMaskLayer = renderSurfaceLayer->replicaLayer() ? renderSurfaceLayer->replicaLayer()->maskLayer() : 0;
    if (replicaMaskLayer) {
        replicaMaskLayer->update(queue, 0, m_renderingStats);
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

    LayerIteratorType end = LayerIteratorType::end(&renderSurfaceLayerList);
    for (LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList); it != end; ++it) {
        occlusionTracker.enterLayer(it);

        if (it.representsTargetRenderSurface()) {
            DCHECK(it->renderSurface()->drawOpacity() || it->renderSurface()->drawOpacityIsAnimating());
            needMoreUpdates |= paintMasksForRenderSurface(*it, queue);
        } else if (it.representsItself()) {
            DCHECK(!it->bounds().IsEmpty());
            it->update(queue, &occlusionTracker, m_renderingStats);
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

gfx::PointF LayerTreeHost::adjustEventPointForPinchZoom(const gfx::PointF& zoomedViewportPoint)
    const
{
    if (m_implTransform.IsIdentity())
        return zoomedViewportPoint;

    DCHECK(m_implTransform.IsInvertible());

    // Scale to screen space before applying implTransform inverse.
    gfx::PointF zoomedScreenspacePoint = gfx::ScalePoint(zoomedViewportPoint, deviceScaleFactor());
    gfx::Transform inverseImplTransform = MathUtil::inverse(m_implTransform);

    bool wasClipped = false;
    gfx::PointF unzoomedScreenspacePoint = MathUtil::projectPoint(inverseImplTransform, zoomedScreenspacePoint, wasClipped);
    DCHECK(!wasClipped);

    // Convert back to logical pixels for hit testing.
    gfx::PointF unzoomedViewportPoint = gfx::ScalePoint(unzoomedScreenspacePoint, 1 / deviceScaleFactor());

    return unzoomedViewportPoint;
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
    return m_settings.maxPartialTextureUpdates != numeric_limits<size_t>::max();
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

void LayerTreeHost::animateLayers(base::TimeTicks time)
{
    if (!m_settings.acceleratedAnimationEnabled || !m_needsAnimateLayers)
        return;

    TRACE_EVENT0("cc", "LayerTreeHostImpl::animateLayers");
    m_needsAnimateLayers = animateLayersRecursive(m_rootLayer.get(), time);
}

bool LayerTreeHost::animateLayersRecursive(Layer* current, base::TimeTicks time)
{
    if (!current)
        return false;

    bool subtreeNeedsAnimateLayers = false;
    LayerAnimationController* currentController = current->layerAnimationController();
    double monotonicTime = (time - base::TimeTicks()).InSecondsF();
    currentController->animate(monotonicTime, 0);

    // If the current controller still has an active animation, we must continue animating layers.
    if (currentController->hasActiveAnimation())
         subtreeNeedsAnimateLayers = true;

    for (size_t i = 0; i < current->children().size(); ++i) {
        if (animateLayersRecursive(current->children()[i].get(), time))
            subtreeNeedsAnimateLayers = true;
    }

    return subtreeNeedsAnimateLayers;
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

}  // namespace cc
