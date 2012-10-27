// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/single_thread_proxy.h"

#include "base/debug/trace_event.h"
#include "cc/draw_quad.h"
#include "cc/graphics_context.h"
#include "cc/layer_tree_host.h"
#include "cc/resource_update_controller.h"
#include "cc/timer.h"
#include <wtf/CurrentTime.h>

namespace cc {

scoped_ptr<Proxy> SingleThreadProxy::create(LayerTreeHost* layerTreeHost)
{
    return make_scoped_ptr(new SingleThreadProxy(layerTreeHost)).PassAs<Proxy>();
}

SingleThreadProxy::SingleThreadProxy(LayerTreeHost* layerTreeHost)
    : m_layerTreeHost(layerTreeHost)
    , m_contextLost(false)
    , m_rendererInitialized(false)
    , m_nextFrameIsNewlyCommittedFrame(false)
    , m_totalCommitCount(0)
{
    TRACE_EVENT0("cc", "SingleThreadProxy::SingleThreadProxy");
    DCHECK(Proxy::isMainThread());
}

void SingleThreadProxy::start()
{
    DebugScopedSetImplThread impl;
    m_layerTreeHostImpl = m_layerTreeHost->createLayerTreeHostImpl(this);
}

SingleThreadProxy::~SingleThreadProxy()
{
    TRACE_EVENT0("cc", "SingleThreadProxy::~SingleThreadProxy");
    DCHECK(Proxy::isMainThread());
    DCHECK(!m_layerTreeHostImpl.get() && !m_layerTreeHost); // make sure stop() got called.
}

bool SingleThreadProxy::compositeAndReadback(void *pixels, const IntRect& rect)
{
    TRACE_EVENT0("cc", "SingleThreadProxy::compositeAndReadback");
    DCHECK(Proxy::isMainThread());

    if (!commitAndComposite())
        return false;

    m_layerTreeHostImpl->readback(pixels, rect);

    if (m_layerTreeHostImpl->isContextLost())
        return false;

    m_layerTreeHostImpl->swapBuffers();
    didSwapFrame();

    return true;
}

void SingleThreadProxy::startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double duration)
{
    m_layerTreeHostImpl->startPageScaleAnimation(targetPosition, useAnchor, scale, monotonicallyIncreasingTime(), duration);
}

void SingleThreadProxy::finishAllRendering()
{
    DCHECK(Proxy::isMainThread());
    {
        DebugScopedSetImplThread impl;
        m_layerTreeHostImpl->finishAllRendering();
    }
}

bool SingleThreadProxy::isStarted() const
{
    DCHECK(Proxy::isMainThread());
    return m_layerTreeHostImpl.get();
}

bool SingleThreadProxy::initializeContext()
{
    DCHECK(Proxy::isMainThread());
    scoped_ptr<GraphicsContext> context = m_layerTreeHost->createContext();
    if (!context.get())
        return false;
    m_contextBeforeInitialization = context.Pass();
    return true;
}

void SingleThreadProxy::setSurfaceReady()
{
    // Scheduling is controlled by the embedder in the single thread case, so nothing to do.
}

void SingleThreadProxy::setVisible(bool visible)
{
    DebugScopedSetImplThread impl;
    m_layerTreeHostImpl->setVisible(visible);
}

bool SingleThreadProxy::initializeRenderer()
{
    DCHECK(Proxy::isMainThread());
    DCHECK(m_contextBeforeInitialization.get());
    {
        DebugScopedSetImplThread impl;
        bool ok = m_layerTreeHostImpl->initializeRenderer(m_contextBeforeInitialization.Pass());
        if (ok) {
            m_rendererInitialized = true;
            m_RendererCapabilitiesForMainThread = m_layerTreeHostImpl->rendererCapabilities();
        }

        return ok;
    }
}

bool SingleThreadProxy::recreateContext()
{
    TRACE_EVENT0("cc", "SingleThreadProxy::recreateContext");
    DCHECK(Proxy::isMainThread());
    DCHECK(m_contextLost);

    scoped_ptr<GraphicsContext> context = m_layerTreeHost->createContext();
    if (!context.get())
        return false;

    bool initialized;
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked;
        DebugScopedSetImplThread impl;
        if (!m_layerTreeHostImpl->contentsTexturesPurged())
            m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->resourceProvider());
        initialized = m_layerTreeHostImpl->initializeRenderer(context.Pass());
        if (initialized) {
            m_RendererCapabilitiesForMainThread = m_layerTreeHostImpl->rendererCapabilities();
        }
    }

    if (initialized)
        m_contextLost = false;

    return initialized;
}

void SingleThreadProxy::renderingStats(RenderingStats* stats)
{
    stats->totalCommitTimeInSeconds = m_totalCommitTime.InSecondsF();
    stats->totalCommitCount = m_totalCommitCount;
    m_layerTreeHostImpl->renderingStats(stats);
}

const RendererCapabilities& SingleThreadProxy::rendererCapabilities() const
{
    DCHECK(m_rendererInitialized);
    // Note: this gets called during the commit by the "impl" thread
    return m_RendererCapabilitiesForMainThread;
}

void SingleThreadProxy::loseContext()
{
    DCHECK(Proxy::isMainThread());
    m_layerTreeHost->didLoseContext();
    m_contextLost = true;
}

void SingleThreadProxy::setNeedsAnimate()
{
    // Thread-only feature
    NOTREACHED();
}

void SingleThreadProxy::doCommit(scoped_ptr<ResourceUpdateQueue> queue)
{
    DCHECK(Proxy::isMainThread());
    // Commit immediately
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked;
        DebugScopedSetImplThread impl;

        base::TimeTicks startTime = base::TimeTicks::HighResNow();
        m_layerTreeHostImpl->beginCommit();

        m_layerTreeHost->contentsTextureManager()->pushTexturePrioritiesToBackings();
        m_layerTreeHost->beginCommitOnImplThread(m_layerTreeHostImpl.get());

        scoped_ptr<ResourceUpdateController> updateController =
            ResourceUpdateController::create(
                NULL,
                Proxy::mainThread(),
                queue.Pass(),
                m_layerTreeHostImpl->resourceProvider());
        updateController->finalize();

        m_layerTreeHost->finishCommitOnImplThread(m_layerTreeHostImpl.get());

        m_layerTreeHostImpl->commitComplete();

#ifndef NDEBUG
        // In the single-threaded case, the scroll deltas should never be
        // touched on the impl layer tree.
        scoped_ptr<ScrollAndScaleSet> scrollInfo = m_layerTreeHostImpl->processScrollDeltas();
        DCHECK(!scrollInfo->scrolls.size());
#endif

        base::TimeTicks endTime = base::TimeTicks::HighResNow();
        m_totalCommitTime += endTime - startTime;
        m_totalCommitCount++;
    }
    m_layerTreeHost->commitComplete();
    m_nextFrameIsNewlyCommittedFrame = true;
}

void SingleThreadProxy::setNeedsCommit()
{
    DCHECK(Proxy::isMainThread());
    m_layerTreeHost->scheduleComposite();
}

void SingleThreadProxy::setNeedsRedraw()
{
    // FIXME: Once we move render_widget scheduling into this class, we can
    // treat redraw requests more efficiently than commitAndRedraw requests.
    m_layerTreeHostImpl->setFullRootLayerDamage();
    setNeedsCommit();
}

void SingleThreadProxy::setDeferCommits(bool deferCommits)
{
    // Thread-only feature.
    NOTREACHED();
}

bool SingleThreadProxy::commitRequested() const
{
    return false;
}

void SingleThreadProxy::didAddAnimation()
{
}

size_t SingleThreadProxy::maxPartialTextureUpdates() const
{
    return std::numeric_limits<size_t>::max();
}

void SingleThreadProxy::stop()
{
    TRACE_EVENT0("cc", "SingleThreadProxy::stop");
    DCHECK(Proxy::isMainThread());
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked;
        DebugScopedSetImplThread impl;

        if (!m_layerTreeHostImpl->contentsTexturesPurged())
            m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->resourceProvider());
        m_layerTreeHostImpl.reset();
    }
    m_layerTreeHost = 0;
}

void SingleThreadProxy::setNeedsRedrawOnImplThread()
{
    m_layerTreeHost->scheduleComposite();
}

void SingleThreadProxy::setNeedsCommitOnImplThread()
{
    m_layerTreeHost->scheduleComposite();
}

void SingleThreadProxy::postAnimationEventsToMainThreadOnImplThread(scoped_ptr<AnimationEventsVector> events, double wallClockTime)
{
    DCHECK(Proxy::isImplThread());
    DebugScopedSetMainThread main;
    m_layerTreeHost->setAnimationEvents(events.Pass(), wallClockTime);
}

bool SingleThreadProxy::reduceContentsTextureMemoryOnImplThread(size_t limitBytes, int priorityCutoff)
{
    DCHECK(isImplThread());
    if (!m_layerTreeHost->contentsTextureManager())
        return false;

    return m_layerTreeHost->contentsTextureManager()->reduceMemoryOnImplThread(limitBytes, priorityCutoff, m_layerTreeHostImpl->resourceProvider());
}

void SingleThreadProxy::sendManagedMemoryStats()
{
    DCHECK(Proxy::isImplThread());
    if (!m_layerTreeHostImpl.get())
        return;
    if (!m_layerTreeHostImpl->renderer())
        return;
    if (!m_layerTreeHost->contentsTextureManager())
        return;

    m_layerTreeHostImpl->renderer()->sendManagedMemoryStats(
        m_layerTreeHost->contentsTextureManager()->memoryVisibleBytes(),
        m_layerTreeHost->contentsTextureManager()->memoryVisibleAndNearbyBytes(),
        m_layerTreeHost->contentsTextureManager()->memoryUseBytes());
}

// Called by the legacy scheduling path (e.g. where render_widget does the scheduling)
void SingleThreadProxy::compositeImmediately()
{
    if (commitAndComposite()) {
        m_layerTreeHostImpl->swapBuffers();
        didSwapFrame();
    }
}

void SingleThreadProxy::forceSerializeOnSwapBuffers()
{
    {
        DebugScopedSetImplThread impl;
        if (m_rendererInitialized)
            m_layerTreeHostImpl->renderer()->doNoOp();
    }
}

void SingleThreadProxy::onSwapBuffersCompleteOnImplThread()
{
    NOTREACHED();
}

bool SingleThreadProxy::commitAndComposite()
{
    DCHECK(Proxy::isMainThread());

    if (!m_layerTreeHost->initializeRendererIfNeeded())
        return false;

    // Unlink any texture backings that were deleted
    PrioritizedTextureManager::BackingList evictedContentsTexturesBackings;
    {
        DebugScopedSetImplThread implThread;
        m_layerTreeHost->contentsTextureManager()->getEvictedBackings(evictedContentsTexturesBackings);
    }
    m_layerTreeHost->contentsTextureManager()->unlinkEvictedBackings(evictedContentsTexturesBackings);

    scoped_ptr<ResourceUpdateQueue> queue = make_scoped_ptr(new ResourceUpdateQueue);
    m_layerTreeHost->updateLayers(*(queue.get()), m_layerTreeHostImpl->memoryAllocationLimitBytes());

    if (m_layerTreeHostImpl->contentsTexturesPurged())
        m_layerTreeHostImpl->resetContentsTexturesPurged();

    m_layerTreeHost->willCommit();
    doCommit(queue.Pass());
    bool result = doComposite();
    m_layerTreeHost->didBeginFrame();
    return result;
}

bool SingleThreadProxy::doComposite()
{
    DCHECK(!m_contextLost);
    {
        DebugScopedSetImplThread impl;

        if (!m_layerTreeHostImpl->visible())
            return false;

        double monotonicTime = monotonicallyIncreasingTime();
        double wallClockTime = currentTime();
        m_layerTreeHostImpl->animate(monotonicTime, wallClockTime);

        // We guard prepareToDraw() with canDraw() because it always returns a valid frame, so can only
        // be used when such a frame is possible. Since drawLayers() depends on the result of
        // prepareToDraw(), it is guarded on canDraw() as well.
        if (!m_layerTreeHostImpl->canDraw())
            return false;

        LayerTreeHostImpl::FrameData frame;
        m_layerTreeHostImpl->prepareToDraw(frame);
        m_layerTreeHostImpl->drawLayers(frame);
        m_layerTreeHostImpl->didDrawAllLayers(frame);
    }

    if (m_layerTreeHostImpl->isContextLost()) {
        m_contextLost = true;
        m_layerTreeHost->didLoseContext();
        return false;
    }

    return true;
}

void SingleThreadProxy::didSwapFrame()
{
    if (m_nextFrameIsNewlyCommittedFrame) {
        m_nextFrameIsNewlyCommittedFrame = false;
        m_layerTreeHost->didCommitAndDrawFrame();
    }
}

}
