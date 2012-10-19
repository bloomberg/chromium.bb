// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCThreadProxy.h"

#include "CCDelayBasedTimeSource.h"
#include "CCDrawQuad.h"
#include "CCFrameRateController.h"
#include "CCGraphicsContext.h"
#include "CCInputHandler.h"
#include "CCLayerTreeHost.h"
#include "CCScheduler.h"
#include "CCScopedThreadProxy.h"
#include "CCThreadTask.h"
#include "base/debug/trace_event.h"
#include <public/WebSharedGraphicsContext3D.h>
#include <wtf/CurrentTime.h>

using WebKit::WebSharedGraphicsContext3D;

namespace {

// Measured in seconds.
const double contextRecreationTickRate = 0.03;

}  // namespace

namespace cc {

scoped_ptr<Proxy> ThreadProxy::create(LayerTreeHost* layerTreeHost)
{
    return make_scoped_ptr(new ThreadProxy(layerTreeHost)).PassAs<Proxy>();
}

ThreadProxy::ThreadProxy(LayerTreeHost* layerTreeHost)
    : m_animateRequested(false)
    , m_commitRequested(false)
    , m_commitRequestSentToImplThread(false)
    , m_forcedCommitRequested(false)
    , m_layerTreeHost(layerTreeHost)
    , m_rendererInitialized(false)
    , m_started(false)
    , m_texturesAcquired(true)
    , m_inCompositeAndReadback(false)
    , m_mainThreadProxy(ScopedThreadProxy::create(Proxy::mainThread()))
    , m_beginFrameCompletionEventOnImplThread(0)
    , m_readbackRequestOnImplThread(0)
    , m_commitCompletionEventOnImplThread(0)
    , m_textureAcquisitionCompletionEventOnImplThread(0)
    , m_nextFrameIsNewlyCommittedFrameOnImplThread(false)
    , m_renderVSyncEnabled(layerTreeHost->settings().renderVSyncEnabled)
    , m_totalCommitCount(0)
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

bool ThreadProxy::compositeAndReadback(void *pixels, const IntRect& rect)
{
    TRACE_EVENT0("cc", "ThreadPRoxy::compositeAndReadback");
    DCHECK(isMainThread());
    DCHECK(m_layerTreeHost);

    if (!m_layerTreeHost->initializeRendererIfNeeded()) {
        TRACE_EVENT0("cc", "compositeAndReadback_EarlyOut_LR_Uninitialized");
        return false;
    }


    // Perform a synchronous commit.
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked;
        CompletionEvent beginFrameCompletion;
        Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::forceBeginFrameOnImplThread, &beginFrameCompletion));
        beginFrameCompletion.wait();
    }
    m_inCompositeAndReadback = true;
    beginFrame();
    m_inCompositeAndReadback = false;

    // Perform a synchronous readback.
    ReadbackRequest request;
    request.rect = rect;
    request.pixels = pixels;
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked;
        Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::requestReadbackOnImplThread, &request));
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

void ThreadProxy::startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double duration)
{
    DCHECK(Proxy::isMainThread());
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::requestStartPageScaleAnimationOnImplThread, targetPosition, useAnchor, scale, duration));
}

void ThreadProxy::requestStartPageScaleAnimationOnImplThread(IntSize targetPosition, bool useAnchor, float scale, double duration)
{
    DCHECK(Proxy::isImplThread());
    if (m_layerTreeHostImpl.get())
        m_layerTreeHostImpl->startPageScaleAnimation(targetPosition, useAnchor, scale, monotonicallyIncreasingTime(), duration);
}

void ThreadProxy::finishAllRendering()
{
    DCHECK(Proxy::isMainThread());

    // Make sure all GL drawing is finished on the impl thread.
    DebugScopedSetMainThreadBlocked mainThreadBlocked;
    CompletionEvent completion;
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::finishAllRenderingOnImplThread, &completion));
    completion.wait();
}

bool ThreadProxy::isStarted() const
{
    DCHECK(Proxy::isMainThread());
    return m_started;
}

bool ThreadProxy::initializeContext()
{
    TRACE_EVENT0("cc", "ThreadProxy::initializeContext");
    scoped_ptr<GraphicsContext> context = m_layerTreeHost->createContext();
    if (!context.get())
        return false;

    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::initializeContextOnImplThread,
                                                       context.release()));
    return true;
}

void ThreadProxy::setSurfaceReady()
{
    TRACE_EVENT0("cc", "ThreadProxy::setSurfaceReady");
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::setSurfaceReadyOnImplThread));
}

void ThreadProxy::setSurfaceReadyOnImplThread()
{
    TRACE_EVENT0("cc", "ThreadProxy::setSurfaceReadyOnImplThread");
    m_schedulerOnImplThread->setCanBeginFrame(true);
}

void ThreadProxy::setVisible(bool visible)
{
    TRACE_EVENT0("cc", "ThreadProxy::setVisible");
    DebugScopedSetMainThreadBlocked mainThreadBlocked;
    CompletionEvent completion;
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::setVisibleOnImplThread, &completion, visible));
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
    DebugScopedSetMainThreadBlocked mainThreadBlocked;
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::initializeRendererOnImplThread,
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

bool ThreadProxy::recreateContext()
{
    TRACE_EVENT0("cc", "ThreadProxy::recreateContext");
    DCHECK(isMainThread());

    // Try to create the context.
    scoped_ptr<GraphicsContext> context = m_layerTreeHost->createContext();
    if (!context.get())
        return false;
    if (m_layerTreeHost->needsSharedContext())
        if (!WebSharedGraphicsContext3D::createCompositorThreadContext())
            return false;

    // Make a blocking call to recreateContextOnImplThread. The results of that
    // call are pushed into the recreateSucceeded and capabilities local
    // variables.
    CompletionEvent completion;
    bool recreateSucceeded = false;
    RendererCapabilities capabilities;
    DebugScopedSetMainThreadBlocked mainThreadBlocked;
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::recreateContextOnImplThread,
                                                       &completion,
                                                       context.release(),
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

    DebugScopedSetMainThreadBlocked mainThreadBlocked;
    CompletionEvent completion;
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::renderingStatsOnImplThread,
                                                       &completion,
                                                       stats));
    stats->totalCommitTimeInSeconds = m_totalCommitTime.InSecondsF();
    stats->totalCommitCount = m_totalCommitCount;

    completion.wait();
}

const RendererCapabilities& ThreadProxy::rendererCapabilities() const
{
    DCHECK(m_rendererInitialized);
    return m_RendererCapabilitiesMainThreadCopy;
}

void ThreadProxy::loseContext()
{
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::didLoseContextOnImplThread));
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
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::setNeedsCommitOnImplThread));
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
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::setNeedsCommitOnImplThread));
}

void ThreadProxy::didLoseContextOnImplThread()
{
    DCHECK(isImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::didLoseContextOnImplThread");
    m_schedulerOnImplThread->didLoseContext();
}

void ThreadProxy::onSwapBuffersCompleteOnImplThread()
{
    DCHECK(isImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::onSwapBuffersCompleteOnImplThread");
    m_schedulerOnImplThread->didSwapBuffersComplete();
    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadProxy::didCompleteSwapBuffers));
}

void ThreadProxy::onVSyncParametersChanged(double monotonicTimebase, double intervalInSeconds)
{
    DCHECK(isImplThread());
    TRACE_EVENT2("cc", "ThreadProxy::onVSyncParametersChanged", "monotonicTimebase", monotonicTimebase, "intervalInSeconds", intervalInSeconds);
    base::TimeTicks timebase = base::TimeTicks::FromInternalValue(monotonicTimebase * base::Time::kMicrosecondsPerSecond);
    base::TimeDelta interval = base::TimeDelta::FromMicroseconds(intervalInSeconds * base::Time::kMicrosecondsPerSecond);
    m_schedulerOnImplThread->setTimebaseAndInterval(timebase, interval);
}

void ThreadProxy::onCanDrawStateChanged(bool canDraw)
{
    DCHECK(isImplThread());
    TRACE_EVENT1("cc", "ThreadProxy::onCanDrawStateChanged", "canDraw", canDraw);
    m_schedulerOnImplThread->setCanDraw(canDraw);
}

void ThreadProxy::setNeedsCommitOnImplThread()
{
    DCHECK(isImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::setNeedsCommitOnImplThread");
    m_schedulerOnImplThread->setNeedsCommit();
}

void ThreadProxy::setNeedsForcedCommitOnImplThread()
{
    DCHECK(isImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::setNeedsForcedCommitOnImplThread");
    m_schedulerOnImplThread->setNeedsCommit();
    m_schedulerOnImplThread->setNeedsForcedCommit();
}

void ThreadProxy::postAnimationEventsToMainThreadOnImplThread(scoped_ptr<AnimationEventsVector> events, double wallClockTime)
{
    DCHECK(isImplThread());
    TRACE_EVENT0("cc", "ThreadProxy::postAnimationEventsToMainThreadOnImplThread");
    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadProxy::setAnimationEvents, events.release(), wallClockTime));
}

bool ThreadProxy::reduceContentsTextureMemoryOnImplThread(size_t limitBytes)
{
    DCHECK(isImplThread());

    if (!m_layerTreeHost->contentsTextureManager())
        return false;

    if (!m_layerTreeHost->contentsTextureManager()->reduceMemoryOnImplThread(limitBytes, m_layerTreeHostImpl->resourceProvider()))
        return false;

    // The texture upload queue may reference textures that were just purged, clear
    // them from the queue.
    if (m_currentTextureUpdateControllerOnImplThread.get())
        m_currentTextureUpdateControllerOnImplThread->discardUploadsToEvictedResources();
    return true;
}

void ThreadProxy::setNeedsRedraw()
{
    DCHECK(isMainThread());
    TRACE_EVENT0("cc", "ThreadProxy::setNeedsRedraw");
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::setFullRootLayerDamageOnImplThread));
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::setNeedsRedrawOnImplThread));
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

void ThreadProxy::start()
{
    DCHECK(isMainThread());
    DCHECK(Proxy::implThread());
    // Create LayerTreeHostImpl.
    DebugScopedSetMainThreadBlocked mainThreadBlocked;
    CompletionEvent completion;
    scoped_ptr<InputHandler> handler = m_layerTreeHost->createInputHandler();
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::initializeImplOnImplThread, &completion, handler.release()));
    completion.wait();

    m_started = true;
}

void ThreadProxy::stop()
{
    TRACE_EVENT0("cc", "ThreadProxy::stop");
    DCHECK(isMainThread());
    DCHECK(m_started);

    // Synchronously deletes the impl.
    {
        DebugScopedSetMainThreadBlocked mainThreadBlocked;

        CompletionEvent completion;
        Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::layerTreeHostClosedOnImplThread, &completion));
        completion.wait();
    }

    m_mainThreadProxy->shutdown(); // Stop running tasks posted to us.

    DCHECK(!m_layerTreeHostImpl.get()); // verify that the impl deleted.
    m_layerTreeHost = 0;
    m_started = false;
}

void ThreadProxy::forceSerializeOnSwapBuffers()
{
    DebugScopedSetMainThreadBlocked mainThreadBlocked;
    CompletionEvent completion;
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::forceSerializeOnSwapBuffersOnImplThread, &completion));
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

    if (m_schedulerOnImplThread->commitPending()) {
        completion->signal();
        return;
    }

    m_beginFrameCompletionEventOnImplThread = completion;
    setNeedsForcedCommitOnImplThread();
}

void ThreadProxy::scheduledActionBeginFrame()
{
    TRACE_EVENT0("cc", "ThreadProxy::scheduledActionBeginFrame");
    DCHECK(!m_pendingBeginFrameRequest);
    m_pendingBeginFrameRequest = make_scoped_ptr(new BeginFrameAndCommitState());
    m_pendingBeginFrameRequest->monotonicFrameBeginTime = monotonicallyIncreasingTime();
    m_pendingBeginFrameRequest->scrollInfo = m_layerTreeHostImpl->processScrollDeltas();
    m_pendingBeginFrameRequest->implTransform = m_layerTreeHostImpl->implTransform();
    m_pendingBeginFrameRequest->memoryAllocationLimitBytes = m_layerTreeHostImpl->memoryAllocationLimitBytes();
    if (m_layerTreeHost->contentsTextureManager())
         m_layerTreeHost->contentsTextureManager()->getEvictedBackings(m_pendingBeginFrameRequest->evictedContentsTexturesBackings);

    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadProxy::beginFrame));

    if (m_beginFrameCompletionEventOnImplThread) {
        m_beginFrameCompletionEventOnImplThread->signal();
        m_beginFrameCompletionEventOnImplThread = 0;
    }
}

void ThreadProxy::beginFrame()
{
    TRACE_EVENT0("cc", "ThreadProxy::beginFrame");
    DCHECK(isMainThread());
    if (!m_layerTreeHost)
        return;

    if (!m_pendingBeginFrameRequest) {
        TRACE_EVENT0("cc", "EarlyOut_StaleBeginFrameMessage");
        return;
    }

    if (m_layerTreeHost->needsSharedContext() && !WebSharedGraphicsContext3D::haveCompositorThreadContext())
        WebSharedGraphicsContext3D::createCompositorThreadContext();

    scoped_ptr<BeginFrameAndCommitState> request(m_pendingBeginFrameRequest.Pass());

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

    // FIXME: technically, scroll deltas need to be applied for dropped commits as well.
    // Re-do the commit flow so that we don't send the scrollInfo on the BFAC message.
    m_layerTreeHost->applyScrollAndScale(*request->scrollInfo);
    m_layerTreeHost->setImplTransform(request->implTransform);

    if (!m_inCompositeAndReadback && !m_layerTreeHost->visible()) {
        m_commitRequested = false;
        m_commitRequestSentToImplThread = false;
        m_forcedCommitRequested = false;

        TRACE_EVENT0("cc", "EarlyOut_NotVisible");
        Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::beginFrameAbortedOnImplThread));
        return;
    }

    m_layerTreeHost->willBeginFrame();

    m_layerTreeHost->updateAnimations(request->monotonicFrameBeginTime);
    m_layerTreeHost->layout();

    // Clear the commit flag after updating animations and layout here --- objects that only
    // layout when painted will trigger another setNeedsCommit inside
    // updateLayers.
    m_commitRequested = false;
    m_commitRequestSentToImplThread = false;
    m_forcedCommitRequested = false;

    if (!m_layerTreeHost->initializeRendererIfNeeded()) {
        TRACE_EVENT0("cc", "EarlyOut_InitializeFailed");
        return;
    }

    m_layerTreeHost->contentsTextureManager()->unlinkEvictedBackings(request->evictedContentsTexturesBackings);

    scoped_ptr<TextureUpdateQueue> queue = make_scoped_ptr(new TextureUpdateQueue);
    m_layerTreeHost->updateLayers(*(queue.get()), request->memoryAllocationLimitBytes);

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

        DebugScopedSetMainThreadBlocked mainThreadBlocked;

        base::TimeTicks startTime = base::TimeTicks::HighResNow();
        CompletionEvent completion;
        Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::beginFrameCompleteOnImplThread, &completion, queue.release()));
        completion.wait();
        base::TimeTicks endTime = base::TimeTicks::HighResNow();

        m_totalCommitTime += endTime - startTime;
        m_totalCommitCount++;
    }

    m_layerTreeHost->commitComplete();
    m_layerTreeHost->didBeginFrame();
}

void ThreadProxy::beginFrameCompleteOnImplThread(CompletionEvent* completion, TextureUpdateQueue* rawQueue)
{
    scoped_ptr<TextureUpdateQueue> queue(rawQueue);

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

    m_currentTextureUpdateControllerOnImplThread = TextureUpdateController::create(this, Proxy::implThread(), queue.Pass(), m_layerTreeHostImpl->resourceProvider());
    m_currentTextureUpdateControllerOnImplThread->performMoreUpdates(
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
    DCHECK(m_currentTextureUpdateControllerOnImplThread);

    // Complete all remaining texture updates.
    m_currentTextureUpdateControllerOnImplThread->finalize();
    m_currentTextureUpdateControllerOnImplThread.reset();

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
    m_mainThreadProxy->postTask(createThreadTask(this, &ThreadProxy::beginContextRecreation));
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
    double monotonicTime = monotonicallyIncreasingTime();
    double wallClockTime = currentTime();

    if (m_inputHandlerOnImplThread.get())
        m_inputHandlerOnImplThread->animate(monotonicTime);
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
        m_mainThreadProxy->postTask(createThreadTask(this, &ThreadProxy::didCommitAndDrawFrame));
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
    DebugScopedSetMainThreadBlocked mainThreadBlocked;
    CompletionEvent completion;
    Proxy::implThread()->postTask(createThreadTask(this, &ThreadProxy::acquireLayerTexturesForMainThreadOnImplThread, &completion));
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
    if (!m_currentTextureUpdateControllerOnImplThread)
        return;

    m_currentTextureUpdateControllerOnImplThread->performMoreUpdates(time);
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

void ThreadProxy::setAnimationEvents(AnimationEventsVector* passed_events, double wallClockTime)
{
    scoped_ptr<AnimationEventsVector> events(make_scoped_ptr(passed_events));

    TRACE_EVENT0("cc", "ThreadProxy::setAnimationEvents");
    DCHECK(isMainThread());
    if (!m_layerTreeHost)
        return;
    m_layerTreeHost->setAnimationEvents(events.Pass(), wallClockTime);
}

class ThreadProxyContextRecreationTimer : public Timer, TimerClient {
public:
    static scoped_ptr<ThreadProxyContextRecreationTimer> create(ThreadProxy* proxy) { return make_scoped_ptr(new ThreadProxyContextRecreationTimer(proxy)); }

    virtual void onTimerFired() OVERRIDE
    {
        m_proxy->tryToRecreateContext();
    }

private:
    explicit ThreadProxyContextRecreationTimer(ThreadProxy* proxy)
        : Timer(Proxy::mainThread(), this)
        , m_proxy(proxy)
    {
    }

    ThreadProxy* m_proxy;
};

void ThreadProxy::beginContextRecreation()
{
    TRACE_EVENT0("cc", "ThreadProxy::beginContextRecreation");
    DCHECK(isMainThread());
    DCHECK(!m_contextRecreationTimer);
    m_contextRecreationTimer = ThreadProxyContextRecreationTimer::create(this);
    m_layerTreeHost->didLoseContext();
    m_contextRecreationTimer->startOneShot(contextRecreationTickRate);
}

void ThreadProxy::tryToRecreateContext()
{
    DCHECK(isMainThread());
    DCHECK(m_layerTreeHost);
    LayerTreeHost::RecreateResult result = m_layerTreeHost->recreateContext();
    if (result == LayerTreeHost::RecreateFailedButTryAgain)
        m_contextRecreationTimer->startOneShot(contextRecreationTickRate);
    else if (result == LayerTreeHost::RecreateSucceeded)
        m_contextRecreationTimer.reset();
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

    completion->signal();
}

void ThreadProxy::initializeContextOnImplThread(GraphicsContext* context)
{
    TRACE_EVENT0("cc", "ThreadProxy::initializeContextOnImplThread");
    DCHECK(isImplThread());
    m_contextBeforeInitializationOnImplThread = scoped_ptr<GraphicsContext>(context).Pass();
}

void ThreadProxy::initializeRendererOnImplThread(CompletionEvent* completion, bool* initializeSucceeded, RendererCapabilities* capabilities)
{
    TRACE_EVENT0("cc", "ThreadProxy::initializeRendererOnImplThread");
    DCHECK(isImplThread());
    DCHECK(m_contextBeforeInitializationOnImplThread.get());
    *initializeSucceeded = m_layerTreeHostImpl->initializeRenderer(m_contextBeforeInitializationOnImplThread.Pass());
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
    completion->signal();
}

void ThreadProxy::setFullRootLayerDamageOnImplThread()
{
    DCHECK(isImplThread());
    m_layerTreeHostImpl->setFullRootLayerDamage();
}

size_t ThreadProxy::maxPartialTextureUpdates() const
{
    return TextureUpdateController::maxPartialTextureUpdates();
}

void ThreadProxy::recreateContextOnImplThread(CompletionEvent* completion, GraphicsContext* contextPtr, bool* recreateSucceeded, RendererCapabilities* capabilities)
{
    TRACE_EVENT0("cc", "ThreadProxy::recreateContextOnImplThread");
    DCHECK(isImplThread());
    m_layerTreeHost->deleteContentsTexturesOnImplThread(m_layerTreeHostImpl->resourceProvider());
    *recreateSucceeded = m_layerTreeHostImpl->initializeRenderer(scoped_ptr<GraphicsContext>(contextPtr).Pass());
    if (*recreateSucceeded) {
        *capabilities = m_layerTreeHostImpl->rendererCapabilities();
        m_schedulerOnImplThread->didRecreateContext();
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
    : monotonicFrameBeginTime(0)
{
}

ThreadProxy::BeginFrameAndCommitState::~BeginFrameAndCommitState()
{
}

}  // namespace cc
