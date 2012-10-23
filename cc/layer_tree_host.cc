// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/layer_tree_host.h"

#include "CCFontAtlas.h"
#include "CCGraphicsContext.h"
#include "Region.h"
#include "base/debug/trace_event.h"
#include "cc/heads_up_display_layer.h"
#include "cc/heads_up_display_layer_impl.h"
#include "cc/layer.h"
#include "cc/layer_animation_controller.h"
#include "cc/layer_iterator.h"
#include "cc/layer_tree_host_client.h"
#include "cc/layer_tree_host_common.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/occlusion_tracker.h"
#include "cc/overdraw_metrics.h"
#include "cc/settings.h"
#include "cc/single_thread_proxy.h"
#include "cc/thread_proxy.h"
#include "cc/tree_synchronizer.h"

using namespace std;
using WebKit::WebTransformationMatrix;

namespace {
static int numLayerTreeInstances;
}

namespace cc {

bool LayerTreeHost::s_needsFilterContext = false;

LayerTreeSettings::LayerTreeSettings()
    : acceleratePainting(false)
    , showFPSCounter(false)
    , showPlatformLayerTree(false)
    , showPaintRects(false)
    , showPropertyChangedRects(false)
    , showSurfaceDamageRects(false)
    , showScreenSpaceRects(false)
    , showReplicaScreenSpaceRects(false)
    , showOccludingRects(false)
    , renderVSyncEnabled(true)
    , refreshRate(0)
    , maxPartialTextureUpdates(std::numeric_limits<size_t>::max())
    , defaultTileSize(IntSize(256, 256))
    , maxUntiledLayerSize(IntSize(512, 512))
    , minimumOcclusionTrackingSize(IntSize(160, 160))
{
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

scoped_ptr<LayerTreeHost> LayerTreeHost::create(LayerTreeHostClient* client, const LayerTreeSettings& settings)
{
    scoped_ptr<LayerTreeHost> layerTreeHost(new LayerTreeHost(client, settings));
    if (!layerTreeHost->initialize())
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
    DCHECK(Proxy::isMainThread());
    numLayerTreeInstances++;
}

bool LayerTreeHost::initialize()
{
    TRACE_EVENT0("cc", "LayerTreeHost::initialize");

    if (Proxy::hasImplThread())
        m_proxy = ThreadProxy::create(this);
    else
        m_proxy = SingleThreadProxy::create(this);
    m_proxy->start();

    return m_proxy->initializeContext();
}

LayerTreeHost::~LayerTreeHost()
{
    if (m_rootLayer)
        m_rootLayer->setLayerTreeHost(0);
    DCHECK(Proxy::isMainThread());
    TRACE_EVENT0("cc", "LayerTreeHost::~LayerTreeHost");
    DCHECK(m_proxy.get());
    m_proxy->stop();
    m_proxy.reset();
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

    m_contentsTextureManager = PrioritizedTextureManager::create(0, m_proxy->rendererCapabilities().maxTextureSize, Renderer::ContentPool);
    m_surfaceMemoryPlaceholder = m_contentsTextureManager->createTexture(IntSize(), GL_RGBA);

    m_rendererInitialized = true;

    m_settings.defaultTileSize = IntSize(min(m_settings.defaultTileSize.width(), m_proxy->rendererCapabilities().maxTextureSize),
                                         min(m_settings.defaultTileSize.height(), m_proxy->rendererCapabilities().maxTextureSize));
    m_settings.maxUntiledLayerSize = IntSize(min(m_settings.maxUntiledLayerSize.width(), m_proxy->rendererCapabilities().maxTextureSize),
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
        if (!Proxy::hasImplThread())
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
    DCHECK(Proxy::isImplThread());
    if (m_rendererInitialized)
        m_contentsTextureManager->clearAllMemory(resourceProvider);
}

void LayerTreeHost::acquireLayerTextures()
{
    DCHECK(Proxy::isMainThread());
    m_proxy->acquireLayerTextures();
}

void LayerTreeHost::updateAnimations(double monotonicFrameBeginTime)
{
    m_animating = true;
    m_client->animate(monotonicFrameBeginTime);
    animateLayers(monotonicFrameBeginTime);
    m_animating = false;

    m_renderingStats.numAnimationFrames++;
}

void LayerTreeHost::layout()
{
    m_client->layout();
}

void LayerTreeHost::beginCommitOnImplThread(LayerTreeHostImpl* hostImpl)
{
    DCHECK(Proxy::isImplThread());
    TRACE_EVENT0("cc", "LayerTreeHost::commitTo");
}

// This function commits the LayerTreeHost to an impl tree. When modifying
// this function, keep in mind that the function *runs* on the impl thread! Any
// code that is logically a main thread operation, e.g. deletion of a Layer,
// should be delayed until the LayerTreeHost::commitComplete, which will run
// after the commit, but on the main thread.
void LayerTreeHost::finishCommitOnImplThread(LayerTreeHostImpl* hostImpl)
{
    DCHECK(Proxy::isImplThread());

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

    m_commitNumber++;
}

void LayerTreeHost::setFontAtlas(scoped_ptr<FontAtlas> fontAtlas)
{
    m_fontAtlas = fontAtlas.Pass();
    setNeedsCommit();
}

void LayerTreeHost::willCommit()
{
    m_client->willCommit();
    if (m_rootLayer && m_settings.showDebugInfo()) {
        if (!m_hudLayer)
            m_hudLayer = HeadsUpDisplayLayer::create();

        if (m_fontAtlas.get())
            m_hudLayer->setFontAtlas(m_fontAtlas.Pass());

        if (!m_hudLayer->parent())
            m_rootLayer->addChild(m_hudLayer);
    }
}

void LayerTreeHost::commitComplete()
{
    m_deleteTextureAfterCommitList.clear();
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
    return LayerTreeHostImpl::create(m_settings, client);
}

void LayerTreeHost::didLoseContext()
{
    TRACE_EVENT0("cc", "LayerTreeHost::didLoseContext");
    DCHECK(Proxy::isMainThread());
    m_contextLost = true;
    m_numFailedRecreateAttempts = 0;
    setNeedsCommit();
}

bool LayerTreeHost::compositeAndReadback(void *pixels, const IntRect& rect)
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
    DCHECK(Proxy::hasImplThread());
    m_proxy->setNeedsAnimate();
}

void LayerTreeHost::setNeedsCommit()
{
    m_proxy->setNeedsCommit();
}

void LayerTreeHost::setNeedsRedraw()
{
    m_proxy->setNeedsRedraw();
    if (!ThreadProxy::implThread())
        m_client->scheduleComposite();
}

bool LayerTreeHost::commitRequested() const
{
    return m_proxy->commitRequested();
}

void LayerTreeHost::setAnimationEvents(scoped_ptr<AnimationEventsVector> events, double wallClockTime)
{
    DCHECK(ThreadProxy::isMainThread());
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

void LayerTreeHost::setViewportSize(const IntSize& layoutViewportSize, const IntSize& deviceViewportSize)
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

void LayerTreeHost::startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double durationSec)
{
    m_proxy->startPageScaleAnimation(targetPosition, useAnchor, scale, durationSec);
}

void LayerTreeHost::loseContext(int numTimes)
{
    TRACE_EVENT1("cc", "LayerTreeHost::loseCompositorContext", "numTimes", numTimes);
    m_numTimesRecreateShouldFail = numTimes - 1;
    m_proxy->loseContext();
}

PrioritizedTextureManager* LayerTreeHost::contentsTextureManager() const
{
    return m_contentsTextureManager.get();
}

void LayerTreeHost::composite()
{
    DCHECK(!ThreadProxy::implThread());
    static_cast<SingleThreadProxy*>(m_proxy.get())->compositeImmediately();
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

void LayerTreeHost::updateLayers(TextureUpdateQueue& queue, size_t memoryAllocationLimitBytes)
{
    DCHECK(m_rendererInitialized);
    DCHECK(memoryAllocationLimitBytes);

    if (!rootLayer())
        return;

    if (layoutViewportSize().isEmpty())
        return;

    m_contentsTextureManager->setMaxMemoryLimitBytes(memoryAllocationLimitBytes);

    updateLayers(rootLayer(), queue);
}

static void setScale(Layer* layer, float deviceScaleFactor, float pageScaleFactor)
{
    if (layer->boundsContainPageScale())
        layer->setContentsScale(deviceScaleFactor);
    else
        layer->setContentsScale(deviceScaleFactor * pageScaleFactor);
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

static void updateLayerScale(Layer* layer, float deviceScaleFactor, float pageScaleFactor)
{
    setScale(layer, deviceScaleFactor, pageScaleFactor);

    Layer* maskLayer = layer->maskLayer();
    if (maskLayer)
        setScale(maskLayer, deviceScaleFactor, pageScaleFactor);

    Layer* replicaMaskLayer = layer->replicaLayer() ? layer->replicaLayer()->maskLayer() : 0;
    if (replicaMaskLayer)
        setScale(replicaMaskLayer, deviceScaleFactor, pageScaleFactor);

    const std::vector<scoped_refptr<Layer> >& children = layer->children();
    for (unsigned int i = 0; i < children.size(); ++i)
        updateLayerScale(children[i].get(), deviceScaleFactor, pageScaleFactor);
}

void LayerTreeHost::updateLayers(Layer* rootLayer, TextureUpdateQueue& queue)
{
    TRACE_EVENT0("cc", "LayerTreeHost::updateLayers");

    updateLayerScale(rootLayer, m_deviceScaleFactor, m_pageScaleFactor);

    LayerList updateList;

    {
        if (Settings::pageScalePinchZoomEnabled()) {
            Layer* rootScroll = findFirstScrollableLayer(rootLayer);
            if (rootScroll)
                rootScroll->setImplTransform(m_implTransform);
        }

        TRACE_EVENT0("cc", "LayerTreeHost::updateLayers::calcDrawEtc");
        LayerTreeHostCommon::calculateDrawTransforms(rootLayer, deviceViewportSize(), m_deviceScaleFactor, rendererCapabilities().maxTextureSize, updateList);
    }

    // Reset partial texture update requests.
    m_partialTextureUpdateRequests = 0;

    bool needMoreUpdates = paintLayerContents(updateList, queue);
    if (m_triggerIdleUpdates && needMoreUpdates)
        setNeedsCommit();

    for (size_t i = 0; i < updateList.size(); ++i)
        updateList[i]->clearRenderSurface();
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

        size_t bytes = Texture::memorySizeBytes(renderSurface->contentRect().size(), GL_RGBA);
        contentsTextureBytes += bytes;

        if (renderSurfaceLayer->backgroundFilters().isEmpty())
            continue;

        if (bytes > maxBackgroundTextureBytes)
            maxBackgroundTextureBytes = bytes;
        if (!readbackBytes)
            readbackBytes = Texture::memorySizeBytes(m_deviceViewportSize, GL_RGBA);
    }
    return readbackBytes + maxBackgroundTextureBytes + contentsTextureBytes;
}

bool LayerTreeHost::paintMasksForRenderSurface(Layer* renderSurfaceLayer, TextureUpdateQueue& queue)
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

bool LayerTreeHost::paintLayerContents(const LayerList& renderSurfaceLayerList, TextureUpdateQueue& queue)
{
    // Use FrontToBack to allow for testing occlusion and performing culling during the tree walk.
    typedef LayerIterator<Layer, LayerList, RenderSurface, LayerIteratorActions::FrontToBack> LayerIteratorType;

    bool needMoreUpdates = false;
    bool recordMetricsForFrame = true; // FIXME: In the future, disable this when about:tracing is off.
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
            DCHECK(!it->bounds().isEmpty());
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
    IntSize rootScrollDelta;

    for (size_t i = 0; i < info.scrolls.size(); ++i) {
        Layer* layer = LayerTreeHostCommon::findLayerInSubtree(m_rootLayer.get(), info.scrolls[i].layerId);
        if (!layer)
            continue;
        if (layer == rootScrollLayer)
            rootScrollDelta += info.scrolls[i].scrollDelta;
        else
            layer->setScrollPosition(layer->scrollPosition() + info.scrolls[i].scrollDelta);
    }
    if (!rootScrollDelta.isZero() || info.pageScaleDelta != 1)
        m_client->applyScrollAndScale(rootScrollDelta, info.pageScaleDelta);
}

void LayerTreeHost::setImplTransform(const WebKit::WebTransformationMatrix& transform)
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
        scoped_refptr<RateLimiter> rateLimiter = RateLimiter::create(context, this);
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

void LayerTreeHost::deleteTextureAfterCommit(scoped_ptr<PrioritizedTexture> texture)
{
    m_deleteTextureAfterCommitList.append(texture.Pass());
}

void LayerTreeHost::setDeviceScaleFactor(float deviceScaleFactor)
{
    if (deviceScaleFactor ==  m_deviceScaleFactor)
        return;
    m_deviceScaleFactor = deviceScaleFactor;

    setNeedsCommit();
}

void LayerTreeHost::animateLayers(double monotonicTime)
{
    if (!Settings::acceleratedAnimationEnabled() || !m_needsAnimateLayers)
        return;

    TRACE_EVENT0("cc", "LayerTreeHostImpl::animateLayers");
    m_needsAnimateLayers = animateLayersRecursive(m_rootLayer.get(), monotonicTime);
}

bool LayerTreeHost::animateLayersRecursive(Layer* current, double monotonicTime)
{
    if (!current)
        return false;

    bool subtreeNeedsAnimateLayers = false;
    LayerAnimationController* currentController = current->layerAnimationController();
    currentController->animate(monotonicTime, 0);

    // If the current controller still has an active animation, we must continue animating layers.
    if (currentController->hasActiveAnimation())
         subtreeNeedsAnimateLayers = true;

    for (size_t i = 0; i < current->children().size(); ++i) {
        if (animateLayersRecursive(current->children()[i].get(), monotonicTime))
            subtreeNeedsAnimateLayers = true;
    }

    return subtreeNeedsAnimateLayers;
}

void LayerTreeHost::setAnimationEventsRecursive(const AnimationEventsVector& events, Layer* layer, double wallClockTime)
{
    if (!layer)
        return;

    for (size_t eventIndex = 0; eventIndex < events.size(); ++eventIndex) {
        if (layer->id() == events[eventIndex].layerId) {
            if (events[eventIndex].type == AnimationEvent::Started)
                layer->notifyAnimationStarted(events[eventIndex], wallClockTime);
            else
                layer->notifyAnimationFinished(wallClockTime);
        }
    }

    for (size_t childIndex = 0; childIndex < layer->children().size(); ++childIndex)
        setAnimationEventsRecursive(events, layer->children()[childIndex].get(), wallClockTime);
}

} // namespace cc
