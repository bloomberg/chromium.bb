// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/thread_proxy.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "cc/delay_based_time_source.h"
#include "cc/draw_quad.h"
#include "cc/frame_rate_controller.h"
#include "cc/input_handler.h"
#include "cc/layer_tree_host.h"
#include "cc/output_surface.h"
#include "cc/scheduler.h"
#include "cc/thread.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSharedGraphicsContext3D.h"

using WebKit::WebSharedGraphicsContext3D;

namespace {

// Measured in seconds.
const double contextRecreationTickRate = 0.03;

}  // namespace

namespace cc {

scoped_ptr<Proxy> ThreadProxy::create(LayerTreeHost* layerTreeHost, scoped_ptr<Thread> implThread)
{
    return make_scoped_ptr(new ThreadProxy(layerTreeHost, implThread.Pass())).PassAs<Proxy>();
}

ThreadProxy::ThreadProxy(LayerTreeHost* layerTreeHost, scoped_ptr<Thread> implThread)
    : Proxy(implThread.Pass())
    , m_animateRequested(false)
    , m_commitRequested(false)
    , m_commitRequestSentToImplThread(false)
    , m_layerTreeHost(layerTreeHost)
    , m_rendererInitialized(false)
    , m_started(false)
    , m_texturesAcquired(true)
    , m_inCompositeAndReadback(false)
    , m_manageTilesPending(false)
    , m_weakFactoryOnImplThread(ALLOW_THIS_IN_INITIALIZER_LIST(this))
    , m_weakFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this))
    , m_beginFrameCompletionEventOnImplThread(0)
    , m_readbackRequestOnImplThread(0)
    , m_commitCompletionEventOnImplThread(0)
    , m_textureAcquisitionCompletionEventOnImplThread(0)
    , m_nextFrameIsNewlyCommittedFrameOnImplThread(false)
    , m_renderVSyncEnabled(layerTreeHost->settings().renderVSyncEnabled)
    , m_totalCommitCount(0)
    , m_deferCommits(false)
{
    TRACE_EVENT0("cc", "ThreadProxy::ThreadProxy");
    DCHECK(isMainThread());
}

ThreadProxy::~ThreadProxy()
{
    TRACE_EVENT0("cc", "ThreadProxy::~ThreadProxy");
    DCHECK(isMainThread());
    DCHECK(!m_started);
}

bool ThreadProxy::compositeAndReadback(void *pixels, const gfx::Rect& rect)
{
    TRACE_EVENT0("cc", "ThreadProxy::compositeAndReadback");
    DCHECK(isMainThread());
    DCHECK(m_layerTreeHost);
    DCHECK(!m_deferCommits);

    if (!m_layerTreeHost->initializeRendererIfNeeded()) {
        TRACE_EVENT0("cc", "compositeAndReadback_EarlyOut_LR_Uninitialized");
        return false;
    }


    // Perform a synchronous commit.
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
        CompletionEvent beginFrameCompletion;
        Proxy::implThread()->postTask(base::Bind(&ThreadProxy::forceBeginFrameOnImplThread, m_implThreadWeakPtr, &beginFrameCompletion));
        beginFrameCompletion.wait();
    }
    m_inCompositeAndReadback = true;
    beginFrame(scoped_ptr<BeginFrameAndCommitState>());
    m_inCompositeAndReadback = false;

    // Perform a synchronous readback.
    ReadbackRequest request;
    request.rect = rect;
    request.pixels = pixels;
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
        Proxy::implThread()->postTask(base::Bind(&ThreadProxy::requestReadbackOnImplThread, m_implThreadWeakPtr, &request));
        request.completion.wait();
    }
    return request.success;
}

void ThreadProxy::requestReadbackOnImplThread(ReadbackRequest* request)
{
    DCHECK(Proxy::isImplThread());
    DCHECK(!m_readbackRequestOnImplThread);
    if (!m_layerTreeHostImpl.get()) {
        request->success = false;
        request->completion.signal();
        return;
    }

    m_readbackRequestOnImplThread = request;
    m_schedulerOnImplThread->setNeedsRedraw();
    m_schedulerOnImplThread->setNeedsForcedRedraw();
}

void ThreadProxy::startPageScaleAnimation(gfx::Vector2d targetOffset, bool useAnchor, float scale, base::TimeDelta duration)
{
    DCHECK(Proxy::isMainThread());
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::requestStartPageScaleAnimationOnImplThread, m_implThreadWeakPtr, targetOffset, useAnchor, scale, duration));
}

void ThreadProxy::requestStartPageScaleAnimationOnImplThread(gfx::Vector2d targetOffset, bool useAnchor, float scale, base::TimeDelta duration)
{
    DCHECK(Proxy::isImplThread());
    if (m_layerTreeHostImpl.get())
        m_layerTreeHostImpl->startPageScaleAnimation(targetOffset, useAnchor, scale, base::TimeTicks::Now(), duration);
}

void ThreadProxy::finishAllRendering()
{
    DCHECK(Proxy::isMainThread());
    DCHECK(!m_deferCommits);

    // Make sure all GL drawing is finished on the impl thread.
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    CompletionEvent completion;
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::finishAllRenderingOnImplThread, m_implThreadWeakPtr, &completion));
    completion.wait();
}

bool ThreadProxy::isStarted() const
{
    DCHECK(Proxy::isMainThread());
    return m_started;
}

bool ThreadProxy::initializeOutputSurface()
{
    TRACE_EVENT0("cc", "ThreadProxy::initializeOutputSurface");
    scoped_ptr<OutputSurface> context = m_layerTreeHost->createOutputSurface();
    if (!context.get())
        return false;

    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::initializeOutputSurfaceOnImplThread, m_implThreadWeakPtr, base::Passed(&context)));
    return true;
}

void ThreadProxy::setSurfaceReady()
{
    TRACE_EVENT0("cc", "ThreadProxy::setSurfaceReady");
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::setSurfaceReadyOnImplThread, m_implThreadWeakPtr));
}

void ThreadProxy::setSurfaceReadyOnImplThread()
{
    TRACE_EVENT0("cc", "ThreadProxy::setSurfaceReadyOnImplThread");
    m_schedulerOnImplThread->setCanBeginFrame(true);
}

void ThreadProxy::setVisible(bool visible)
{
    TRACE_EVENT0("cc", "ThreadProxy::setVisible");
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    CompletionEvent completion;
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::setVisibleOnImplThread, m_implThreadWeakPtr, &completion, visible));
    completion.wait();
}

void ThreadProxy::setVisibleOnImplThread(CompletionEvent* completion, bool visible)
{
    TRACE_EVENT0("cc", "ThreadProxy::setVisibleOnImplThread");
    m_layerTreeHostImpl->setVisible(visible);
    m_schedulerOnImplThread->setVisible(visible);
    completion->signal();
}

bool ThreadProxy::initializeRenderer()
{
    TRACE_EVENT0("cc", "ThreadProxy::initializeRenderer");
    // Make a blocking call to initializeRendererOnImplThread. The results of that call
    // are pushed into the initializeSucceeded and capabilities local variables.
    CompletionEvent completion;
    bool initializeSucceeded = false;
    RendererCapabilities capabilities;
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::initializeRendererOnImplThread,
                                             m_implThreadWeakPtr,
                                             &completion,
                                             &initializeSucceeded,
                                             &capabilities));
    completion.wait();

    if (initializeSucceeded) {
        m_rendererInitialized = true;
        m_RendererCapabilitiesMainThreadCopy = capabilities;
    }
    return initializeSucceeded;
}

bool ThreadProxy::recreateOutputSurface()
{
    TRACE_EVENT0("cc", "ThreadProxy::recreateOutputSurface");
    DCHECK(isMainThread());

    // Try to create the surface.
    scoped_ptr<OutputSurface> outputSurface = m_layerTreeHost->createOutputSurface();
    if (!outputSurface.get())
        return false;
    if (m_layerTreeHost->needsSharedContext())
        if (!WebSharedGraphicsContext3D::createCompositorThreadContext())
            return false;

    // Make a blocking call to recreateOutputSurfaceOnImplThread. The results of that
    // call are pushed into the recreateSucceeded and capabilities local
    // variables.
    CompletionEvent completion;
    bool recreateSucceeded = false;
    RendererCapabilities capabilities;
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::recreateOutputSurfaceOnImplThread,
                                             m_implThreadWeakPtr,
                                             &completion,
                                             base::Passed(&outputSurface),
                                             &recreateSucceeded,
                                             &capabilities));
    completion.wait();

    if (recreateSucceeded)
        m_RendererCapabilitiesMainThreadCopy = capabilities;
    return recreateSucceeded;
}

void ThreadProxy::renderingStats(RenderingStats* stats)
{
    DCHECK(isMainThread());

    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    CompletionEvent completion;
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::renderingStatsOnImplThread,
                                             m_implThreadWeakPtr,  &completion, stats));
    stats->totalCommitTimeInSeconds = m_totalCommitTime.InSecondsF();
    stats->totalCommitCount = m_totalCommitCount;

    completion.wait();
}

const RendererCapabilities& ThreadProxy::rendererCapabilities() const
{
    DCHECK(m_rendererInitialized);
    return m_RendererCapabilitiesMainThreadCopy;
}

void ThreadProxy::setNeedsAnimate()
{
    DCHECK(isMainThread());
    if (m_animateRequested)
        return;

    TRACE_EVENT0("cc", "ThreadProxy::setNeedsAnimate");
    m_animateRequested = true;

    if (m_commitRequestSentToImplThread)
        return;
    m_commitRequestSentToImplThread = true;
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::setNeedsCommitOnImplThread, m_implThreadWeakPtr));
}

void ThreadProxy::setNeedsCommit()
{
    DCHECK(isMainThread());
    if (m_commitRequested)
        return;
    TRACE_EVENT0("cc", "ThreadProxy::setNeedsCommit");
    m_commitRequested = true;

    if (m_commitRequestSentToImplThread)
        return;
    m_commitRequestSentToImplThread = true;
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::setNeedsCommitOnImplThread, m_implThreadWeakPtr));
}

void ThreadProxy::didLoseOutputSurfaceOnImplThread()
{
    DCHECK(isImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::didLoseOutputSurfaceOnImplThread");
    m_schedulerOnImplThread->didLoseOutputSurface();
}

void ThreadProxy::onSwapBuffersCompleteOnImplThread()
{
    DCHECK(isImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::onSwapBuffersCompleteOnImplThread");
    m_schedulerOnImplThread->didSwapBuffersComplete();
    Proxy::mainThread()->postTask(base::Bind(&ThreadProxy::didCompleteSwapBuffers, m_mainThreadWeakPtr));
}

void ThreadProxy::onVSyncParametersChanged(base::TimeTicks timebase, base::TimeDelta interval)
{
    DCHECK(isImplThread());
    TRACE_EVENT2("cc", "ThreadProxy::onVSyncParametersChanged", "timebase", (timebase - base::TimeTicks()).InMilliseconds(), "interval", interval.InMilliseconds());
    m_schedulerOnImplThread->setTimebaseAndInterval(timebase, interval);
}

void ThreadProxy::onCanDrawStateChanged(bool canDraw)
{
    DCHECK(isImplThread());
    TRACE_EVENT1("cc", "ThreadProxy::onCanDrawStateChanged", "canDraw", canDraw);
    m_schedulerOnImplThread->setCanDraw(canDraw);
}

void ThreadProxy::onHasPendingTreeStateChanged(bool hasPendingTree)
{
    DCHECK(isImplThread());
    TRACE_EVENT1("cc", "ThreadProxy::onHasPendingTreeStateChanged", "hasPendingTree", hasPendingTree);
    m_schedulerOnImplThread->setHasPendingTree(hasPendingTree);
}

void ThreadProxy::setNeedsCommitOnImplThread()
{
    DCHECK(isImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::setNeedsCommitOnImplThread");
    m_schedulerOnImplThread->setNeedsCommit();
}

void ThreadProxy::setNeedsManageTilesOnImplThread()
{
    if (m_manageTilesPending)
      return;
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::manageTilesOnImplThread, m_implThreadWeakPtr));
    m_manageTilesPending = true;
}

void ThreadProxy::manageTilesOnImplThread()
{
    // TODO(nduca): If needed, move this into CCSchedulerStateMachine.
    m_manageTilesPending = false;
    if (m_layerTreeHostImpl)
        m_layerTreeHostImpl->manageTiles();
}

void ThreadProxy::setNeedsForcedCommitOnImplThread()
{
    DCHECK(isImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::setNeedsForcedCommitOnImplThread");
    m_schedulerOnImplThread->setNeedsForcedCommit();
}

void ThreadProxy::postAnimationEventsToMainThreadOnImplThread(scoped_ptr<AnimationEventsVector> events, base::Time wallClockTime)
{
    DCHECK(isImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::postAnimationEventsToMainThreadOnImplThread");
    Proxy::mainThread()->postTask(base::Bind(&ThreadProxy::setAnimationEvents, m_mainThreadWeakPtr, base::Passed(&events), wallClockTime));
}

bool ThreadProxy::reduceContentsTextureMemoryOnImplThread(size_t limitBytes, int priorityCutoff)
{
    DCHECK(isImplThread());

    if (!m_layerTreeHost->contentsTextureManager())
        return false;

    bool reduceResult = m_layerTreeHost->contentsTextureManager()->reduceMemoryOnImplThread(limitBytes, priorityCutoff, m_layerTreeHostImpl->resourceProvider());
    if (!reduceResult)
        return false;

    // The texture upload queue may reference textures that were just purged, clear
    // them from the queue.
    if (m_currentResourceUpdateControllerOnImplThread.get())
        m_currentResourceUpdateControllerOnImplThread->discardUploadsToEvictedResources();
    return true;
}

void ThreadProxy::sendManagedMemoryStats()
{
    DCHECK(isImplThread());
    if (!m_layerTreeHostImpl.get())
        return;
    if (!m_layerTreeHost->contentsTextureManager())
        return;

    // If we are using impl-side painting, then sendManagedMemoryStats is called
    // directly after the tile manager's manage function, and doesn't need to
    // interact with main thread's layer tree.
    if (m_layerTreeHost->settings().implSidePainting)
        return;

    m_layerTreeHostImpl->sendManagedMemoryStats(
        m_layerTreeHost->contentsTextureManager()->memoryVisibleBytes(),
        m_layerTreeHost->contentsTextureManager()->memoryVisibleAndNearbyBytes(),
        m_layerTreeHost->contentsTextureManager()->memoryUseBytes());
}

void ThreadProxy::setNeedsRedraw()
{
    DCHECK(isMainThread());
    TRACE_EVENT0("cc", "ThreadProxy::setNeedsRedraw");
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::setFullRootLayerDamageOnImplThread, m_implThreadWeakPtr));
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::setNeedsRedrawOnImplThread, m_implThreadWeakPtr));
}

void ThreadProxy::setDeferCommits(bool deferCommits)
{
    DCHECK(isMainThread());
    DCHECK_NE(m_deferCommits, deferCommits);
    m_deferCommits = deferCommits;

    if (m_deferCommits)
        TRACE_EVENT_ASYNC_BEGIN0("cc", "ThreadProxy::setDeferCommits", this);
    else
        TRACE_EVENT_ASYNC_END0("cc", "ThreadProxy::setDeferCommits", this);

    if (!m_deferCommits && m_pendingDeferredCommit)
        Proxy::mainThread()->postTask(base::Bind(&ThreadProxy::beginFrame, m_mainThreadWeakPtr, base::Passed(&m_pendingDeferredCommit)));
}

bool ThreadProxy::commitRequested() const
{
    DCHECK(isMainThread());
    return m_commitRequested;
}

void ThreadProxy::setNeedsRedrawOnImplThread()
{
    DCHECK(isImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::setNeedsRedrawOnImplThread");
    m_schedulerOnImplThread->setNeedsRedraw();
}

void ThreadProxy::mainThreadHasStoppedFlinging()
{
    if (m_inputHandlerOnImplThread)
        m_inputHandlerOnImplThread->mainThreadHasStoppedFlinging();
}

void ThreadProxy::start()
{
    DCHECK(isMainThread());
    DCHECK(Proxy::implThread());
    // Create LayerTreeHostImpl.
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    CompletionEvent completion;
    scoped_ptr<InputHandler> handler = m_layerTreeHost->createInputHandler();
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::initializeImplOnImplThread, base::Unretained(this), &completion, handler.release()));
    completion.wait();

    m_mainThreadWeakPtr = m_weakFactory.GetWeakPtr();

    m_started = true;
}

void ThreadProxy::stop()
{
    TRACE_EVENT0("cc", "ThreadProxy::stop");
    DCHECK(isMainThread());
    DCHECK(m_started);

    // Synchronously deletes the impl.
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked(this);

        CompletionEvent completion;
        Proxy::implThread()->postTask(base::Bind(&ThreadProxy::layerTreeHostClosedOnImplThread, m_implThreadWeakPtr, &completion));
        completion.wait();
    }

    m_weakFactory.InvalidateWeakPtrs();

    DCHECK(!m_layerTreeHostImpl.get()); // verify that the impl deleted.
    m_layerTreeHost = 0;
    m_started = false;
}

void ThreadProxy::forceSerializeOnSwapBuffers()
{
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    CompletionEvent completion;
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::forceSerializeOnSwapBuffersOnImplThread, m_implThreadWeakPtr, &completion));
    completion.wait();
}

void ThreadProxy::forceSerializeOnSwapBuffersOnImplThread(CompletionEvent* completion)
{
    if (m_rendererInitialized)
        m_layerTreeHostImpl->renderer()->doNoOp();
    completion->signal();
}


void ThreadProxy::finishAllRenderingOnImplThread(CompletionEvent* completion)
{
    TRACE_EVENT0("cc", "ThreadProxy::finishAllRenderingOnImplThread");
    DCHECK(isImplThread());
    m_layerTreeHostImpl->finishAllRendering();
    completion->signal();
}

void ThreadProxy::forceBeginFrameOnImplThread(CompletionEvent* completion)
{
    TRACE_EVENT0("cc", "ThreadProxy::forceBeginFrameOnImplThread");
    DCHECK(!m_beginFrameCompletionEventOnImplThread);

    setNeedsForcedCommitOnImplThread();
    if (m_schedulerOnImplThread->commitPending()) {
        completion->signal();
        return;
    }

    m_beginFrameCompletionEventOnImplThread = completion;
}

void ThreadProxy::scheduledActionBeginFrame()
{
    TRACE_EVENT0("cc", "ThreadProxy::scheduledActionBeginFrame");
    scoped_ptr<BeginFrameAndCommitState> beginFrameState(new BeginFrameAndCommitState);
    beginFrameState->monotonicFrameBeginTime = base::TimeTicks::Now();
    beginFrameState->scrollInfo = m_layerTreeHostImpl->processScrollDeltas();
    beginFrameState->implTransform = m_layerTreeHostImpl->implTransform();
    DCHECK_GT(m_layerTreeHostImpl->memoryAllocationLimitBytes(), 0u);
    beginFrameState->memoryAllocationLimitBytes = m_layerTreeHostImpl->memoryAllocationLimitBytes();
    Proxy::mainThread()->postTask(base::Bind(&ThreadProxy::beginFrame, m_mainThreadWeakPtr, base::Passed(&beginFrameState)));

    if (m_beginFrameCompletionEventOnImplThread) {
        m_beginFrameCompletionEventOnImplThread->signal();
        m_beginFrameCompletionEventOnImplThread = 0;
    }
}

void ThreadProxy::beginFrame(scoped_ptr<BeginFrameAndCommitState> beginFrameState)
{
    TRACE_EVENT0("cc", "ThreadProxy::beginFrame");
    DCHECK(isMainThread());
    if (!m_layerTreeHost)
        return;

    if (m_deferCommits) {
        m_pendingDeferredCommit = beginFrameState.Pass();
        m_layerTreeHost->didDeferCommit();
        TRACE_EVENT0("cc", "EarlyOut_DeferCommits");
        return;
    }

    if (m_layerTreeHost->needsSharedContext() && !WebSharedGraphicsContext3D::haveCompositorThreadContext())
        WebSharedGraphicsContext3D::createCompositorThreadContext();

    // Do not notify the impl thread of commit requests that occur during
    // the apply/animate/layout part of the beginFrameAndCommit process since
    // those commit requests will get painted immediately. Once we have done
    // the paint, m_commitRequested will be set to false to allow new commit
    // requests to be scheduled.
    m_commitRequested = true;
    m_commitRequestSentToImplThread = true;

    // On the other hand, the animationRequested flag needs to be cleared
    // here so that any animation requests generated by the apply or animate
    // callbacks will trigger another frame.
    m_animateRequested = false;

    if (beginFrameState) {
        m_layerTreeHost->applyScrollAndScale(*beginFrameState->scrollInfo);
        m_layerTreeHost->setImplTransform(beginFrameState->implTransform);
    }

    if (!m_inCompositeAndReadback && !m_layerTreeHost->visible()) {
        m_commitRequested = false;
        m_commitRequestSentToImplThread = false;

        TRACE_EVENT0("cc", "EarlyOut_NotVisible");
        Proxy::implThread()->postTask(base::Bind(&ThreadProxy::beginFrameAbortedOnImplThread, m_implThreadWeakPtr));
        return;
    }

    m_layerTreeHost->willBeginFrame();

    if (beginFrameState)
        m_layerTreeHost->updateAnimations(beginFrameState->monotonicFrameBeginTime);

    // Unlink any backings that the impl thread has evicted, so that we know to re-paint
    // them in updateLayers.
    if (m_layerTreeHost->contentsTextureManager())
        m_layerTreeHost->contentsTextureManager()->unlinkAndClearEvictedBackings();

    m_layerTreeHost->layout();

    // Clear the commit flag after updating animations and layout here --- objects that only
    // layout when painted will trigger another setNeedsCommit inside
    // updateLayers.
    m_commitRequested = false;
    m_commitRequestSentToImplThread = false;

    if (!m_layerTreeHost->initializeRendererIfNeeded()) {
        TRACE_EVENT0("cc", "EarlyOut_InitializeFailed");
        return;
    }

    scoped_ptr<ResourceUpdateQueue> queue = make_scoped_ptr(new ResourceUpdateQueue);
    m_layerTreeHost->updateLayers(*(queue.get()), beginFrameState ? beginFrameState->memoryAllocationLimitBytes : 0);

    // Once single buffered layers are committed, they cannot be modified until
    // they are drawn by the impl thread.
    m_texturesAcquired = false;

    m_layerTreeHost->willCommit();
    // Before applying scrolls and calling animate, we set m_animateRequested to
    // false. If it is true now, it means setNeedAnimate was called again, but
    // during a state when m_commitRequestSentToImplThread = true. We need to
    // force that call to happen again now so that the commit request is sent to
    // the impl thread.
    if (m_animateRequested) {
        // Forces setNeedsAnimate to consider posting a commit task.
        m_animateRequested = false;
        setNeedsAnimate();
    }

    // Notify the impl thread that the beginFrame has completed. This will
    // begin the commit process, which is blocking from the main thread's
    // point of view, but asynchronously performed on the impl thread,
    // coordinated by the Scheduler.
    {
        TRACE_EVENT0("cc", "commit");

        DebugScopedSetMainThreadBlocked mainThreadBlocked(this);

        base::TimeTicks startTime = base::TimeTicks::HighResNow();
        CompletionEvent completion;
        Proxy::implThread()->postTask(base::Bind(&ThreadProxy::beginFrameCompleteOnImplThread, m_implThreadWeakPtr, &completion, queue.release()));
        completion.wait();
        base::TimeTicks endTime = base::TimeTicks::HighResNow();

        m_totalCommitTime += endTime - startTime;
        m_totalCommitCount++;
    }

    m_layerTreeHost->commitComplete();
    m_layerTreeHost->didBeginFrame();
}

void ThreadProxy::beginFrameCompleteOnImplThread(CompletionEvent* completion, ResourceUpdateQueue* rawQueue)
{
    scoped_ptr<ResourceUpdateQueue> queue(rawQueue);

    TRACE_EVENT0("cc", "ThreadProxy::beginFrameCompleteOnImplThread");
    DCHECK(!m_commitCompletionEventOnImplThread);
    DCHECK(isImplThread() && isMainThreadBlocked());
    DCHECK(m_schedulerOnImplThread);
    DCHECK(m_schedulerOnImplThread->commitPending());

    if (!m_layerTreeHostImpl.get()) {
        TRACE_EVENT0("cc", "EarlyOut_NoLayerTree");
        completion->signal();
        return;
    }

    if (m_layerTreeHost->contentsTextureManager()->linkedEvictedBackingsExist()) {
        // Clear any uploads we were making to textures linked to evicted
        // resources
        queue->clearUploadsToEvictedResources();
        // Some textures in the layer tree are invalid. Kick off another commit
        // to fill them again.
        setNeedsCommitOnImplThread();
    }

    m_layerTreeHost->contentsTextureManager()->pushTexturePrioritiesToBackings();

    m_currentResourceUpdateControllerOnImplThread = ResourceUpdateController::create(this, Proxy::implThread(), queue.Pass(), m_layerTreeHostImpl->resourceProvider(), hasImplThread());
    m_currentResourceUpdateControllerOnImplThread->performMoreUpdates(
        m_schedulerOnImplThread->anticipatedDrawTime());

    m_commitCompletionEventOnImplThread = completion;
}

void ThreadProxy::beginFrameAbortedOnImplThread()
{
    TRACE_EVENT0("cc", "ThreadProxy::beginFrameAbortedOnImplThread");
    DCHECK(isImplThread());
    DCHECK(m_schedulerOnImplThread);
    DCHECK(m_schedulerOnImplThread->commitPending());

    m_schedulerOnImplThread->beginFrameAborted();
}

void ThreadProxy::scheduledActionCommit()
{
    TRACE_EVENT0("cc", "ThreadProxy::scheduledActionCommit");
    DCHECK(isImplThread());
    DCHECK(m_commitCompletionEventOnImplThread);
    DCHECK(m_currentResourceUpdateControllerOnImplThread);

    // Complete all remaining texture updates.
    m_currentResourceUpdateControllerOnImplThread->finalize();
    m_currentResourceUpdateControllerOnImplThread.reset();

    // If there are linked evicted backings, these backings' resources may be put into the
    // impl tree, so we can't draw yet. Determine this before clearing all evicted backings.
    bool newImplTreeHasNoEvictedResources = !m_layerTreeHost->contentsTextureManager()->linkedEvictedBackingsExist();

    m_layerTreeHostImpl->beginCommit();
    m_layerTreeHost->beginCommitOnImplThread(m_layerTreeHostImpl.get());
    m_layerTreeHost->finishCommitOnImplThread(m_layerTreeHostImpl.get());

    if (newImplTreeHasNoEvictedResources) {
        if (m_layerTreeHostImpl->contentsTexturesPurged())
            m_layerTreeHostImpl->resetContentsTexturesPurged();
    }

    m_layerTreeHostImpl->commitComplete();

    m_nextFrameIsNewlyCommittedFrameOnImplThread = true;

    m_commitCompletionEventOnImplThread->signal();
    m_commitCompletionEventOnImplThread = 0;

    // SetVisible kicks off the next scheduler action, so this must be last.
    m_schedulerOnImplThread->setVisible(m_layerTreeHostImpl->visible());
}

void ThreadProxy::scheduledActionBeginContextRecreation()
{
    DCHECK(isImplThread());
    Proxy::mainThread()->postTask(base::Bind(&ThreadProxy::beginContextRecreation, m_mainThreadWeakPtr));
}

ScheduledActionDrawAndSwapResult ThreadProxy::scheduledActionDrawAndSwapInternal(bool forcedDraw)
{
    TRACE_EVENT0("cc", "ThreadProxy::scheduledActionDrawAndSwap");
    ScheduledActionDrawAndSwapResult result;
    result.didDraw = false;
    result.didSwap = false;
    DCHECK(isImplThread());
    DCHECK(m_layerTreeHostImpl.get());
    if (!m_layerTreeHostImpl.get())
        return result;

    DCHECK(m_layerTreeHostImpl->renderer());
    if (!m_layerTreeHostImpl->renderer())
        return result;

    // FIXME: compute the frame display time more intelligently
    base::TimeTicks monotonicTime = base::TimeTicks::Now();
    base::Time wallClockTime = base::Time::Now();

    if (m_inputHandlerOnImplThread.get())
        m_inputHandlerOnImplThread->animate(monotonicTime);

    // TODO(nduca): make animation happen after tree activation.
    m_layerTreeHostImpl->animate(monotonicTime, wallClockTime);

    // This method is called on a forced draw, regardless of whether we are able to produce a frame,
    // as the calling site on main thread is blocked until its request completes, and we signal
    // completion here. If canDraw() is false, we will indicate success=false to the caller, but we
    // must still signal completion to avoid deadlock.

    // We guard prepareToDraw() with canDraw() because it always returns a valid frame, so can only
    // be used when such a frame is possible. Since drawLayers() depends on the result of
    // prepareToDraw(), it is guarded on canDraw() as well.

    LayerTreeHostImpl::FrameData frame;
    bool drawFrame = m_layerTreeHostImpl->canDraw() && (m_layerTreeHostImpl->prepareToDraw(frame) || forcedDraw);
    if (drawFrame) {
        m_layerTreeHostImpl->drawLayers(frame);
        result.didDraw = true;
    }
    m_layerTreeHostImpl->didDrawAllLayers(frame);

    // Check for a pending compositeAndReadback.
    if (m_readbackRequestOnImplThread) {
        m_readbackRequestOnImplThread->success = false;
        if (drawFrame) {
            m_layerTreeHostImpl->readback(m_readbackRequestOnImplThread->pixels, m_readbackRequestOnImplThread->rect);
            m_readbackRequestOnImplThread->success = !m_layerTreeHostImpl->isContextLost();
        }
        m_readbackRequestOnImplThread->completion.signal();
        m_readbackRequestOnImplThread = 0;
    } else if (drawFrame)
        result.didSwap = m_layerTreeHostImpl->swapBuffers();

    // Tell the main thread that the the newly-commited frame was drawn.
    if (m_nextFrameIsNewlyCommittedFrameOnImplThread) {
        m_nextFrameIsNewlyCommittedFrameOnImplThread = false;
        Proxy::mainThread()->postTask(base::Bind(&ThreadProxy::didCommitAndDrawFrame, m_mainThreadWeakPtr));
    }

    return result;
}

void ThreadProxy::acquireLayerTextures()
{
    // Called when the main thread needs to modify a layer texture that is used
    // directly by the compositor.
    // This method will block until the next compositor draw if there is a
    // previously committed frame that is still undrawn. This is necessary to
    // ensure that the main thread does not monopolize access to the textures.
    DCHECK(isMainThread());

    if (m_texturesAcquired)
        return;

    TRACE_EVENT0("cc", "ThreadProxy::acquireLayerTextures");
    DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
    CompletionEvent completion;
    Proxy::implThread()->postTask(base::Bind(&ThreadProxy::acquireLayerTexturesForMainThreadOnImplThread, m_implThreadWeakPtr, &completion));
    completion.wait(); // Block until it is safe to write to layer textures from the main thread.

    m_texturesAcquired = true;
}

void ThreadProxy::acquireLayerTexturesForMainThreadOnImplThread(CompletionEvent* completion)
{
    DCHECK(isImplThread());
    DCHECK(!m_textureAcquisitionCompletionEventOnImplThread);

    m_textureAcquisitionCompletionEventOnImplThread = completion;
    m_schedulerOnImplThread->setMainThreadNeedsLayerTextures();
}

void ThreadProxy::scheduledActionAcquireLayerTexturesForMainThread()
{
    DCHECK(m_textureAcquisitionCompletionEventOnImplThread);
    m_textureAcquisitionCompletionEventOnImplThread->signal();
    m_textureAcquisitionCompletionEventOnImplThread = 0;
}

ScheduledActionDrawAndSwapResult ThreadProxy::scheduledActionDrawAndSwapIfPossible()
{
    return scheduledActionDrawAndSwapInternal(false);
}

ScheduledActionDrawAndSwapResult ThreadProxy::scheduledActionDrawAndSwapForced()
{
    return scheduledActionDrawAndSwapInternal(true);
}

void ThreadProxy::didAnticipatedDrawTimeChange(base::TimeTicks time)
{
    if (!m_currentResourceUpdateControllerOnImplThread)
        return;

    m_currentResourceUpdateControllerOnImplThread->performMoreUpdates(time);
}

void ThreadProxy::readyToFinalizeTextureUpdates()
{
    DCHECK(isImplThread());
    m_schedulerOnImplThread->beginFrameComplete();
}

void ThreadProxy::didCommitAndDrawFrame()
{
    DCHECK(isMainThread());
    if (!m_layerTreeHost)
        return;
    m_layerTreeHost->didCommitAndDrawFrame();
}

void ThreadProxy::didCompleteSwapBuffers()
{
    DCHECK(isMainThread());
    if (!m_layerTreeHost)
        return;
    m_layerTreeHost->didCompleteSwapBuffers();
}

void ThreadProxy::setAnimationEvents(scoped_ptr<AnimationEventsVector> events, base::Time wallClockTime)
{
    TRACE_EVENT0("cc", "ThreadProxy::setAnimationEvents");
    DCHECK(isMainThread());
    if (!m_layerTreeHost)
        return;
    m_layerTreeHost->setAnimationEvents(events.Pass(), wallClockTime);
}

void ThreadProxy::beginContextRecreation()
{
    TRACE_EVENT0("cc", "ThreadProxy::beginContextRecreation");
    DCHECK(isMainThread());
    m_layerTreeHost->didLoseOutputSurface();
    m_outputSurfaceRecreationCallback.Reset(base::Bind(&ThreadProxy::tryToRecreateOutputSurface, base::Unretained(this)));
    Proxy::mainThread()->postTask(m_outputSurfaceRecreationCallback.callback());
}

void ThreadProxy::tryToRecreateOutputSurface()
{
    DCHECK(isMainThread());
    DCHECK(m_layerTreeHost);
    LayerTreeHost::RecreateResult result = m_layerTreeHost->recreateOutputSurface();
    if (result == LayerTreeHost::RecreateFailedButTryAgain)
        Proxy::mainThread()->postTask(m_outputSurfaceRecreationCallback.callback());
    else if (result == LayerTreeHost::RecreateSucceeded)
        m_outputSurfaceRecreationCallback.Cancel();
}

void ThreadProxy::initializeImplOnImplThread(CompletionEvent* completion, InputHandler* handler)
{
    TRACE_EVENT0("cc", "ThreadProxy::initializeImplOnImplThread");
    DCHECK(isImplThread());
    m_layerTreeHostImpl = m_layerTreeHost->createLayerTreeHostImpl(this);
    const base::TimeDelta displayRefreshInterval = base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond / 60);
    scoped_ptr<FrameRateController> frameRateController;
    if (m_renderVSyncEnabled)
        frameRateController.reset(new FrameRateController(DelayBasedTimeSource::create(displayRefreshInterval, Proxy::implThread())));
    else
        frameRateController.reset(new FrameRateController(Proxy::implThread()));
    m_schedulerOnImplThread = Scheduler::create(this, frameRateController.Pass());
    m_schedulerOnImplThread->setVisible(m_layerTreeHostImpl->visible());

    m_inputHandlerOnImplThread = scoped_ptr<InputHandler>(handler);
    if (m_inputHandlerOnImplThread.get())
        m_inputHandlerOnImplThread->bindToClient(m_layerTreeHostImpl.get());

    m_implThreadWeakPtr = m_weakFactoryOnImplThread.GetWeakPtr();
    completion->signal();
}

void ThreadProxy::initializeOutputSurfaceOnImplThread(scoped_ptr<OutputSurface> outputSurface)
{
    TRACE_EVENT0("cc", "ThreadProxy::initializeContextOnImplThread");
    DCHECK(isImplThread());
    m_outputSurfaceBeforeInitializationOnImplThread = outputSurface.Pass();
}

void ThreadProxy::initializeRendererOnImplThread(CompletionEvent* completion, bool* initializeSucceeded, RendererCapabilities* capabilities)
{
    TRACE_EVENT0("cc", "ThreadProxy::initializeRendererOnImplThread");
    DCHECK(isImplThread());
    DCHECK(m_outputSurfaceBeforeInitializationOnImplThread.get());
    *initializeSucceeded = m_layerTreeHostImpl->initializeRenderer(m_outputSurfaceBeforeInitializationOnImplThread.Pass());
    if (*initializeSucceeded) {
        *capabilities = m_layerTreeHostImpl->rendererCapabilities();
        m_schedulerOnImplThread->setSwapBuffersCompleteSupported(
                capabilities->usingSwapCompleteCallback);
    }

    completion->signal();
}

void ThreadProxy::layerTreeHostClosedOnImplThread(CompletionEvent* completion)
{
    TRACE_EVENT0("cc", "ThreadProxy::layerTreeHostClosedOnImplThread");
    DCHECK(isImplThread());
    m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->resourceProvider());
    m_inputHandlerOnImplThread.reset();
    m_layerTreeHostImpl.reset();
    m_schedulerOnImplThread.reset();
    m_weakFactoryOnImplThread.InvalidateWeakPtrs();
    completion->signal();
}

void ThreadProxy::setFullRootLayerDamageOnImplThread()
{
    DCHECK(isImplThread());
    m_layerTreeHostImpl->setFullRootLayerDamage();
}

size_t ThreadProxy::maxPartialTextureUpdates() const
{
    return ResourceUpdateController::maxPartialTextureUpdates();
}

void ThreadProxy::recreateOutputSurfaceOnImplThread(CompletionEvent* completion, scoped_ptr<OutputSurface> outputSurface, bool* recreateSucceeded, RendererCapabilities* capabilities)
{
    TRACE_EVENT0("cc", "ThreadProxy::recreateOutputSurfaceOnImplThread");
    DCHECK(isImplThread());
    m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->resourceProvider());
    *recreateSucceeded = m_layerTreeHostImpl->initializeRenderer(outputSurface.Pass());
    if (*recreateSucceeded) {
        *capabilities = m_layerTreeHostImpl->rendererCapabilities();
        m_schedulerOnImplThread->didRecreateOutputSurface();
    }
    completion->signal();
}

void ThreadProxy::renderingStatsOnImplThread(CompletionEvent* completion, RenderingStats* stats)
{
    DCHECK(isImplThread());
    m_layerTreeHostImpl->renderingStats(stats);
    completion->signal();
}

ThreadProxy::BeginFrameAndCommitState::BeginFrameAndCommitState()
    : memoryAllocationLimitBytes(0)
{
}

ThreadProxy::BeginFrameAndCommitState::~BeginFrameAndCommitState()
{
}

bool ThreadProxy::commitPendingForTesting()
{
    DCHECK(isMainThread());
    CommitPendingRequest commitPendingRequest;
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked(this);
        Proxy::implThread()->postTask(base::Bind(&ThreadProxy::commitPendingOnImplThreadForTesting, m_implThreadWeakPtr, &commitPendingRequest));
        commitPendingRequest.completion.wait();
    }
    return commitPendingRequest.commitPending;
}

void ThreadProxy::commitPendingOnImplThreadForTesting(CommitPendingRequest* request)
{
    DCHECK(isImplThread());
    request->commitPending = m_schedulerOnImplThread->commitPending();
    request->completion.signal();
}

}  // namespace cc
