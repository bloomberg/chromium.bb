// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/single_thread_proxy.h"

#include "base/debug/trace_event.h"
#include "cc/draw_quad.h"
#include "cc/layer_tree_host.h"
#include "cc/layer_tree_impl.h"
#include "cc/output_surface.h"
#include "cc/prioritized_resource_manager.h"
#include "cc/resource_update_controller.h"
#include "cc/thread.h"

namespace cc {

scoped_ptr<Proxy> SingleThreadProxy::create(LayerTreeHost* layerTreeHost)
{
    return make_scoped_ptr(new SingleThreadProxy(layerTreeHost)).PassAs<Proxy>();
}

SingleThreadProxy::SingleThreadProxy(LayerTreeHost* layerTreeHost)
    : Proxy(scoped_ptr<Thread>(NULL))
    , m_layerTreeHost(layerTreeHost)
    , m_outputSurfaceLost(false)
    , m_rendererInitialized(false)
    , m_nextFrameIsNewlyCommittedFrame(false)
    , m_totalCommitCount(0)
{
    TRACE_EVENT0("cc", "SingleThreadProxy::SingleThreadProxy");
    DCHECK(Proxy::isMainThread());
    DCHECK(layerTreeHost);

    // Impl-side painting not supported without threaded compositing
    DCHECK(!layerTreeHost->settings().implSidePainting);
}

void SingleThreadProxy::start()
{
    DebugScopedSetImplThread impl(this);
    m_layerTreeHostImpl = m_layerTreeHost->createLayerTreeHostImpl(this);
}

SingleThreadProxy::~SingleThreadProxy()
{
    TRACE_EVENT0("cc", "SingleThreadProxy::~SingleThreadProxy");
    DCHECK(Proxy::isMainThread());
    DCHECK(!m_layerTreeHostImpl.get() && !m_layerTreeHost); // make sure stop() got called.
}

bool SingleThreadProxy::compositeAndReadback(void *pixels, const gfx::Rect& rect)
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

void SingleThreadProxy::startPageScaleAnimation(gfx::Vector2d targetOffset, bool useAnchor, float scale, base::TimeDelta duration)
{
    m_layerTreeHostImpl->startPageScaleAnimation(targetOffset, useAnchor, scale, base::TimeTicks::Now(), duration);
}

void SingleThreadProxy::finishAllRendering()
{
    DCHECK(Proxy::isMainThread());
    {
        DebugScopedSetImplThread impl(this);
        m_layerTreeHostImpl->finishAllRendering();
    }
}

bool SingleThreadProxy::isStarted() const
{
    DCHECK(Proxy::isMainThread());
    return m_layerTreeHostImpl.get();
}

bool SingleThreadProxy::initializeOutputSurface()
{
    DCHECK(Proxy::isMainThread());
    scoped_ptr<OutputSurface> outputSurface = m_layerTreeHost->createOutputSurface();
    if (!outputSurface.get())
        return false;
    m_outputSurfaceBeforeInitialization = outputSurface.Pass();
    return true;
}

void SingleThreadProxy::setSurfaceReady()
{
    // Scheduling is controlled by the embedder in the single thread case, so nothing to do.
}

void SingleThreadProxy::setVisible(bool visible)
{
    DebugScopedSetImplThread impl(this);
    m_layerTreeHostImpl->setVisible(visible);
}

bool SingleThreadProxy::initializeRenderer()
{
    DCHECK(Proxy::isMainThread());
    DCHECK(m_outputSurfaceBeforeInitialization.get());
    {
        DebugScopedSetImplThread impl(this);
        bool ok = m_layerTreeHostImpl->initializeRenderer(m_outputSurfaceBeforeInitialization.Pass());
        if (ok) {
            m_rendererInitialized = true;
            m_RendererCapabilitiesForMainThread = m_layerTreeHostImpl->rendererCapabilities();
        }

        return ok;
    }
}

bool SingleThreadProxy::recreateOutputSurface()
{
    TRACE_EVENT0("cc", "SingleThreadProxy::recreateContext");
    DCHECK(Proxy::isMainThread());
    DCHECK(m_outputSurfaceLost);

    scoped_ptr<OutputSurface> outputSurface = m_layerTreeHost->createOutputSurface();
    if (!outputSurface.get())
        return false;

    bool initialized;
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
        DebugScopedSetImplThread impl(this);
        m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->resourceProvider());
        initialized = m_layerTreeHostImpl->initializeRenderer(outputSurface.Pass());
        if (initialized) {
            m_RendererCapabilitiesForMainThread = m_layerTreeHostImpl->rendererCapabilities();
        }
    }

    if (initialized)
        m_outputSurfaceLost = false;

    return initialized;
}

void SingleThreadProxy::renderingStats(RenderingStats* stats)
{
    stats->totalCommitTime = m_totalCommitTime;
    stats->totalCommitCount = m_totalCommitCount;
    m_layerTreeHostImpl->renderingStats(stats);
}

const RendererCapabilities& SingleThreadProxy::rendererCapabilities() const
{
    DCHECK(m_rendererInitialized);
    // Note: this gets called during the commit by the "impl" thread
    return m_RendererCapabilitiesForMainThread;
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
        DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
        DebugScopedSetImplThread impl(this);

        base::TimeTicks startTime = base::TimeTicks::HighResNow();
        m_layerTreeHostImpl->beginCommit();

        m_layerTreeHost->contentsTextureManager()->pushTexturePrioritiesToBackings();
        m_layerTreeHost->beginCommitOnImplThread(m_layerTreeHostImpl.get());

        scoped_ptr<ResourceUpdateController> updateController =
            ResourceUpdateController::create(
                NULL,
                Proxy::mainThread(),
                queue.Pass(),
                m_layerTreeHostImpl->resourceProvider(),
                hasImplThread());
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

void SingleThreadProxy::onHasPendingTreeStateChanged(bool havePendingTree)
{
    // Thread-only feature.
    NOTREACHED();
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

size_t SingleThreadProxy::maxPartialTextureUpdates() const
{
    return std::numeric_limits<size_t>::max();
}

void SingleThreadProxy::stop()
{
    TRACE_EVENT0("cc", "SingleThreadProxy::stop");
    DCHECK(Proxy::isMainThread());
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
        DebugScopedSetImplThread impl(this);

        if (!m_layerTreeHostImpl->activeTree()->ContentsTexturesPurged())
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

void SingleThreadProxy::setNeedsManageTilesOnImplThread()
{
    m_layerTreeHost->scheduleComposite();
}

void SingleThreadProxy::postAnimationEventsToMainThreadOnImplThread(scoped_ptr<AnimationEventsVector> events, base::Time wallClockTime)
{
    DCHECK(Proxy::isImplThread());
    DebugScopedSetMainThread main(this);
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
    if (!m_layerTreeHost->contentsTextureManager())
        return;

    m_layerTreeHostImpl->sendManagedMemoryStats(
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
        DebugScopedSetImplThread impl(this);
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

    m_layerTreeHost->contentsTextureManager()->unlinkAndClearEvictedBackings();

    scoped_ptr<ResourceUpdateQueue> queue = make_scoped_ptr(new ResourceUpdateQueue);
    m_layerTreeHost->updateLayers(*(queue.get()), m_layerTreeHostImpl->memoryAllocationLimitBytes());

    m_layerTreeHost->willCommit();
    doCommit(queue.Pass());
    bool result = doComposite();
    m_layerTreeHost->didBeginFrame();
    return result;
}

bool SingleThreadProxy::doComposite()
{
    DCHECK(!m_outputSurfaceLost);
    {
        DebugScopedSetImplThread impl(this);

        if (!m_layerTreeHostImpl->visible())
            return false;

        m_layerTreeHostImpl->animate(base::TimeTicks::Now(), base::Time::Now());

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
        m_outputSurfaceLost = true;
        m_layerTreeHost->didLoseOutputSurface();
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

bool SingleThreadProxy::commitPendingForTesting()
{
    return false;
}

skia::RefPtr<SkPicture> SingleThreadProxy::capturePicture()
{
    // Requires impl-side painting, which is only supported in threaded compositing.
    NOTREACHED();
    return skia::RefPtr<SkPicture>();
}

}  // namespace cc
