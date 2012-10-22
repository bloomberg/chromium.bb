// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCThreadProxy_h
#define CCThreadProxy_h

#include "CCAnimationEvents.h"
#include "CCCompletionEvent.h"
#include "base/time.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/proxy.h"
#include "cc/scheduler.h"
#include "cc/texture_update_controller.h"

namespace cc {

class InputHandler;
class LayerTreeHost;
class Scheduler;
class ScopedThreadProxy;
class TextureUpdateQueue;
class Thread;
class ThreadProxyContextRecreationTimer;

class ThreadProxy : public Proxy, LayerTreeHostImplClient, SchedulerClient, TextureUpdateControllerClient {
public:
    static scoped_ptr<Proxy> create(LayerTreeHost*);

    virtual ~ThreadProxy();

    // Proxy implementation
    virtual bool compositeAndReadback(void *pixels, const IntRect&) OVERRIDE;
    virtual void startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double duration) OVERRIDE;
    virtual void finishAllRendering() OVERRIDE;
    virtual bool isStarted() const OVERRIDE;
    virtual bool initializeContext() OVERRIDE;
    virtual void setSurfaceReady() OVERRIDE;
    virtual void setVisible(bool) OVERRIDE;
    virtual bool initializeRenderer() OVERRIDE;
    virtual bool recreateContext() OVERRIDE;
    virtual void renderingStats(RenderingStats*) OVERRIDE;
    virtual const RendererCapabilities& rendererCapabilities() const OVERRIDE;
    virtual void loseContext() OVERRIDE;
    virtual void setNeedsAnimate() OVERRIDE;
    virtual void setNeedsCommit() OVERRIDE;
    virtual void setNeedsRedraw() OVERRIDE;
    virtual bool commitRequested() const OVERRIDE;
    virtual void didAddAnimation() OVERRIDE { }
    virtual void start() OVERRIDE;
    virtual void stop() OVERRIDE;
    virtual size_t maxPartialTextureUpdates() const OVERRIDE;
    virtual void acquireLayerTextures() OVERRIDE;
    virtual void forceSerializeOnSwapBuffers() OVERRIDE;

    // LayerTreeHostImplClient implementation
    virtual void didLoseContextOnImplThread() OVERRIDE;
    virtual void onSwapBuffersCompleteOnImplThread() OVERRIDE;
    virtual void onVSyncParametersChanged(double monotonicTimebase, double intervalInSeconds) OVERRIDE;
    virtual void onCanDrawStateChanged(bool canDraw) OVERRIDE;
    virtual void setNeedsRedrawOnImplThread() OVERRIDE;
    virtual void setNeedsCommitOnImplThread() OVERRIDE;
    virtual void postAnimationEventsToMainThreadOnImplThread(scoped_ptr<AnimationEventsVector>, double wallClockTime) OVERRIDE;
    virtual bool reduceContentsTextureMemoryOnImplThread(size_t limitBytes, int priorityCutoff) OVERRIDE;

    // SchedulerClient implementation
    virtual void scheduledActionBeginFrame() OVERRIDE;
    virtual ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapIfPossible() OVERRIDE;
    virtual ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapForced() OVERRIDE;
    virtual void scheduledActionCommit() OVERRIDE;
    virtual void scheduledActionBeginContextRecreation() OVERRIDE;
    virtual void scheduledActionAcquireLayerTexturesForMainThread() OVERRIDE;
    virtual void didAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE;

    // TextureUpdateControllerClient implementation
    virtual void readyToFinalizeTextureUpdates() OVERRIDE;

private:
    explicit ThreadProxy(LayerTreeHost*);
    friend class ThreadProxyContextRecreationTimer;

    // Set on impl thread, read on main thread.
    struct BeginFrameAndCommitState {
        BeginFrameAndCommitState();
        ~BeginFrameAndCommitState();

        double monotonicFrameBeginTime;
        scoped_ptr<ScrollAndScaleSet> scrollInfo;
        WebKit::WebTransformationMatrix implTransform;
        PrioritizedTextureManager::BackingList evictedContentsTexturesBackings;
        size_t memoryAllocationLimitBytes;
    };
    scoped_ptr<BeginFrameAndCommitState> m_pendingBeginFrameRequest;

    // Called on main thread
    void beginFrame();
    void didCommitAndDrawFrame();
    void didCompleteSwapBuffers();
    void setAnimationEvents(AnimationEventsVector*, double wallClockTime);
    void beginContextRecreation();
    void tryToRecreateContext();

    // Called on impl thread
    struct ReadbackRequest {
        CompletionEvent completion;
        bool success;
        void* pixels;
        IntRect rect;
    };
    void forceBeginFrameOnImplThread(CompletionEvent*);
    void beginFrameCompleteOnImplThread(CompletionEvent*, TextureUpdateQueue*);
    void beginFrameAbortedOnImplThread();
    void requestReadbackOnImplThread(ReadbackRequest*);
    void requestStartPageScaleAnimationOnImplThread(IntSize targetPosition, bool useAnchor, float scale, double durationSec);
    void finishAllRenderingOnImplThread(CompletionEvent*);
    void initializeImplOnImplThread(CompletionEvent*, InputHandler*);
    void setSurfaceReadyOnImplThread();
    void setVisibleOnImplThread(CompletionEvent*, bool);
    void initializeContextOnImplThread(GraphicsContext*);
    void initializeRendererOnImplThread(CompletionEvent*, bool* initializeSucceeded, RendererCapabilities*);
    void layerTreeHostClosedOnImplThread(CompletionEvent*);
    void setFullRootLayerDamageOnImplThread();
    void acquireLayerTexturesForMainThreadOnImplThread(CompletionEvent*);
    void recreateContextOnImplThread(CompletionEvent*, GraphicsContext*, bool* recreateSucceeded, RendererCapabilities*);
    void renderingStatsOnImplThread(CompletionEvent*, RenderingStats*);
    ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapInternal(bool forcedDraw);
    void forceSerializeOnSwapBuffersOnImplThread(CompletionEvent*);
    void setNeedsForcedCommitOnImplThread();

    // Accessed on main thread only.
    bool m_animateRequested; // Set only when setNeedsAnimate is called.
    bool m_commitRequested; // Set only when setNeedsCommit is called.
    bool m_commitRequestSentToImplThread; // Set by setNeedsCommit and setNeedsAnimate.
    bool m_forcedCommitRequested;
    scoped_ptr<ThreadProxyContextRecreationTimer> m_contextRecreationTimer;
    LayerTreeHost* m_layerTreeHost;
    bool m_rendererInitialized;
    RendererCapabilities m_RendererCapabilitiesMainThreadCopy;
    bool m_started;
    bool m_texturesAcquired;
    bool m_inCompositeAndReadback;

    scoped_ptr<LayerTreeHostImpl> m_layerTreeHostImpl;

    scoped_ptr<InputHandler> m_inputHandlerOnImplThread;

    scoped_ptr<Scheduler> m_schedulerOnImplThread;

    RefPtr<ScopedThreadProxy> m_mainThreadProxy;

    // Holds on to the context we might use for compositing in between initializeContext()
    // and initializeRenderer() calls.
    scoped_ptr<GraphicsContext> m_contextBeforeInitializationOnImplThread;

    // Set when the main thread is waiting on a scheduledActionBeginFrame to be issued.
    CompletionEvent* m_beginFrameCompletionEventOnImplThread;

    // Set when the main thread is waiting on a readback.
    ReadbackRequest* m_readbackRequestOnImplThread;

    // Set when the main thread is waiting on a commit to complete.
    CompletionEvent* m_commitCompletionEventOnImplThread;

    // Set when the main thread is waiting on layers to be drawn.
    CompletionEvent* m_textureAcquisitionCompletionEventOnImplThread;

    scoped_ptr<TextureUpdateController> m_currentTextureUpdateControllerOnImplThread;

    // Set when the next draw should post didCommitAndDrawFrame to the main thread.
    bool m_nextFrameIsNewlyCommittedFrameOnImplThread;

    bool m_renderVSyncEnabled;

    base::TimeDelta m_totalCommitTime;
    size_t m_totalCommitCount;
};

}

#endif
