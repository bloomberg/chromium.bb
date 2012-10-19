// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/single_thread_proxy.h"

#include "CCDrawQuad.h"
#include "CCGraphicsContext.h"
#include "CCLayerTreeHost.h"
#include "CCTimer.h"
#include "base/debug/trace_event.h"
#include "cc/texture_update_controller.h"
#include <wtf/CurrentTime.h>

namespace cc {

scoped_ptr<CCProxy> CCSingleThreadProxy::create(CCLayerTreeHost* layerTreeHost)
{
    return make_scoped_ptr(new CCSingleThreadProxy(layerTreeHost)).PassAs<CCProxy>();
}

CCSingleThreadProxy::CCSingleThreadProxy(CCLayerTreeHost* layerTreeHost)
    : m_layerTreeHost(layerTreeHost)
    , m_contextLost(false)
    , m_rendererInitialized(false)
    , m_nextFrameIsNewlyCommittedFrame(false)
    , m_totalCommitCount(0)
{
    TRACE_EVENT0("cc", "CCSingleThreadProxy::CCSingleThreadProxy");
    DCHECK(CCProxy::isMainThread());
}

void CCSingleThreadProxy::start()
{
    DebugScopedSetImplThread impl;
    m_layerTreeHostImpl = m_layerTreeHost->createLayerTreeHostImpl(this);
}

CCSingleThreadProxy::~CCSingleThreadProxy()
{
    TRACE_EVENT0("cc", "CCSingleThreadProxy::~CCSingleThreadProxy");
    DCHECK(CCProxy::isMainThread());
    DCHECK(!m_layerTreeHostImpl.get() && !m_layerTreeHost); // make sure stop() got called.
}

bool CCSingleThreadProxy::compositeAndReadback(void *pixels, const IntRect& rect)
{
    TRACE_EVENT0("cc", "CCSingleThreadProxy::compositeAndReadback");
    DCHECK(CCProxy::isMainThread());

    if (!commitAndComposite())
        return false;

    m_layerTreeHostImpl->readback(pixels, rect);

    if (m_layerTreeHostImpl->isContextLost())
        return false;

    m_layerTreeHostImpl->swapBuffers();
    didSwapFrame();

    return true;
}

void CCSingleThreadProxy::startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double duration)
{
    m_layerTreeHostImpl->startPageScaleAnimation(targetPosition, useAnchor, scale, monotonicallyIncreasingTime(), duration);
}

void CCSingleThreadProxy::finishAllRendering()
{
    DCHECK(CCProxy::isMainThread());
    {
        DebugScopedSetImplThread impl;
        m_layerTreeHostImpl->finishAllRendering();
    }
}

bool CCSingleThreadProxy::isStarted() const
{
    DCHECK(CCProxy::isMainThread());
    return m_layerTreeHostImpl.get();
}

bool CCSingleThreadProxy::initializeContext()
{
    DCHECK(CCProxy::isMainThread());
    scoped_ptr<CCGraphicsContext> context = m_layerTreeHost->createContext();
    if (!context.get())
        return false;
    m_contextBeforeInitialization = context.Pass();
    return true;
}

void CCSingleThreadProxy::setSurfaceReady()
{
    // Scheduling is controlled by the embedder in the single thread case, so nothing to do.
}

void CCSingleThreadProxy::setVisible(bool visible)
{
    DebugScopedSetImplThread impl;
    m_layerTreeHostImpl->setVisible(visible);
}

bool CCSingleThreadProxy::initializeRenderer()
{
    DCHECK(CCProxy::isMainThread());
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

bool CCSingleThreadProxy::recreateContext()
{
    TRACE_EVENT0("cc", "CCSingleThreadProxy::recreateContext");
    DCHECK(CCProxy::isMainThread());
    DCHECK(m_contextLost);

    scoped_ptr<CCGraphicsContext> context = m_layerTreeHost->createContext();
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

void CCSingleThreadProxy::renderingStats(CCRenderingStats* stats)
{
    stats->totalCommitTimeInSeconds = m_totalCommitTime.InSecondsF();
    stats->totalCommitCount = m_totalCommitCount;
    m_layerTreeHostImpl->renderingStats(stats);
}

const RendererCapabilities& CCSingleThreadProxy::rendererCapabilities() const
{
    DCHECK(m_rendererInitialized);
    // Note: this gets called during the commit by the "impl" thread
    return m_RendererCapabilitiesForMainThread;
}

void CCSingleThreadProxy::loseContext()
{
    DCHECK(CCProxy::isMainThread());
    m_layerTreeHost->didLoseContext();
    m_contextLost = true;
}

void CCSingleThreadProxy::setNeedsAnimate()
{
    // CCThread-only feature
    NOTREACHED();
}

void CCSingleThreadProxy::doCommit(scoped_ptr<CCTextureUpdateQueue> queue)
{
    DCHECK(CCProxy::isMainThread());
    // Commit immediately
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked;
        DebugScopedSetImplThread impl;

        base::TimeTicks startTime = base::TimeTicks::HighResNow();
        m_layerTreeHostImpl->beginCommit();

        m_layerTreeHost->contentsTextureManager()->pushTexturePrioritiesToBackings();
        m_layerTreeHost->beginCommitOnImplThread(m_layerTreeHostImpl.get());

        scoped_ptr<CCTextureUpdateController> updateController =
            CCTextureUpdateController::create(
                NULL,
                CCProxy::mainThread(),
                queue.Pass(),
                m_layerTreeHostImpl->resourceProvider());
        updateController->finalize();

        m_layerTreeHost->finishCommitOnImplThread(m_layerTreeHostImpl.get());

        m_layerTreeHostImpl->commitComplete();

#ifndef NDEBUG
        // In the single-threaded case, the scroll deltas should never be
        // touched on the impl layer tree.
        scoped_ptr<CCScrollAndScaleSet> scrollInfo = m_layerTreeHostImpl->processScrollDeltas();
        DCHECK(!scrollInfo->scrolls.size());
#endif

        base::TimeTicks endTime = base::TimeTicks::HighResNow();
        m_totalCommitTime += endTime - startTime;
        m_totalCommitCount++;
    }
    m_layerTreeHost->commitComplete();
    m_nextFrameIsNewlyCommittedFrame = true;
}

void CCSingleThreadProxy::setNeedsCommit()
{
    DCHECK(CCProxy::isMainThread());
    m_layerTreeHost->scheduleComposite();
}

void CCSingleThreadProxy::setNeedsRedraw()
{
    // FIXME: Once we move render_widget scheduling into this class, we can
    // treat redraw requests more efficiently than commitAndRedraw requests.
    m_layerTreeHostImpl->setFullRootLayerDamage();
    setNeedsCommit();
}

bool CCSingleThreadProxy::commitRequested() const
{
    return false;
}

void CCSingleThreadProxy::didAddAnimation()
{
}

size_t CCSingleThreadProxy::maxPartialTextureUpdates() const
{
    return std::numeric_limits<size_t>::max();
}

void CCSingleThreadProxy::stop()
{
    TRACE_EVENT0("cc", "CCSingleThreadProxy::stop");
    DCHECK(CCProxy::isMainThread());
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked;
        DebugScopedSetImplThread impl;

        if (!m_layerTreeHostImpl->contentsTexturesPurged())
            m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->resourceProvider());
        m_layerTreeHostImpl.reset();
    }
    m_layerTreeHost = 0;
}

void CCSingleThreadProxy::setNeedsRedrawOnImplThread()
{
    m_layerTreeHost->scheduleComposite();
}

void CCSingleThreadProxy::setNeedsCommitOnImplThread()
{
    m_layerTreeHost->scheduleComposite();
}

void CCSingleThreadProxy::postAnimationEventsToMainThreadOnImplThread(scoped_ptr<CCAnimationEventsVector> events, double wallClockTime)
{
    DCHECK(CCProxy::isImplThread());
    DebugScopedSetMainThread main;
    m_layerTreeHost->setAnimationEvents(events.Pass(), wallClockTime);
}

bool CCSingleThreadProxy::reduceContentsTextureMemoryOnImplThread(size_t limitBytes)
{
    DCHECK(isImplThread());
    if (!m_layerTreeHost->contentsTextureManager())
        return false;

    return m_layerTreeHost->contentsTextureManager()->reduceMemoryOnImplThread(limitBytes, m_layerTreeHostImpl->resourceProvider());
}

// Called by the legacy scheduling path (e.g. where render_widget does the scheduling)
void CCSingleThreadProxy::compositeImmediately()
{
    if (commitAndComposite()) {
        m_layerTreeHostImpl->swapBuffers();
        didSwapFrame();
    }
}

void CCSingleThreadProxy::forceSerializeOnSwapBuffers()
{
    {
        DebugScopedSetImplThread impl;
        if (m_rendererInitialized)
            m_layerTreeHostImpl->renderer()->doNoOp();
    }
}

void CCSingleThreadProxy::onSwapBuffersCompleteOnImplThread()
{
    NOTREACHED();
}

bool CCSingleThreadProxy::commitAndComposite()
{
    DCHECK(CCProxy::isMainThread());

    if (!m_layerTreeHost->initializeRendererIfNeeded())
        return false;

    // Unlink any texture backings that were deleted
    CCPrioritizedTextureManager::BackingList evictedContentsTexturesBackings;
    {
        DebugScopedSetImplThread implThread;
        m_layerTreeHost->contentsTextureManager()->getEvictedBackings(evictedContentsTexturesBackings);
    }
    m_layerTreeHost->contentsTextureManager()->unlinkEvictedBackings(evictedContentsTexturesBackings);

    scoped_ptr<CCTextureUpdateQueue> queue = make_scoped_ptr(new CCTextureUpdateQueue);
    m_layerTreeHost->updateLayers(*(queue.get()), m_layerTreeHostImpl->memoryAllocationLimitBytes());

    if (m_layerTreeHostImpl->contentsTexturesPurged())
        m_layerTreeHostImpl->resetContentsTexturesPurged();

    m_layerTreeHost->willCommit();
    doCommit(queue.Pass());
    bool result = doComposite();
    m_layerTreeHost->didBeginFrame();
    return result;
}

bool CCSingleThreadProxy::doComposite()
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

        CCLayerTreeHostImpl::FrameData frame;
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

void CCSingleThreadProxy::didSwapFrame()
{
    if (m_nextFrameIsNewlyCommittedFrame) {
        m_nextFrameIsNewlyCommittedFrame = false;
        m_layerTreeHost->didCommitAndDrawFrame();
    }
}

}
