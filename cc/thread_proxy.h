// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_THREAD_PROXY_H_
#define CC_THREAD_PROXY_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "cc/animation_events.h"
#include "cc/completion_event.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/proxy.h"
#include "cc/resource_update_controller.h"
#include "cc/scheduler.h"

namespace cc {

class InputHandler;
class LayerTreeHost;
class ResourceUpdateQueue;
class Scheduler;
class ScopedThreadProxy;
class Thread;

class ThreadProxy : public Proxy, LayerTreeHostImplClient, SchedulerClient, ResourceUpdateControllerClient {
public:
    static scoped_ptr<Proxy> create(LayerTreeHost*, scoped_ptr<Thread> implThread);

    virtual ~ThreadProxy();

    // Proxy implementation
    virtual bool compositeAndReadback(void *pixels, const gfx::Rect&) OVERRIDE;
    virtual void startPageScaleAnimation(gfx::Vector2d targetOffset, bool useAnchor, float scale, base::TimeDelta duration) OVERRIDE;
    virtual void finishAllRendering() OVERRIDE;
    virtual bool isStarted() const OVERRIDE;
    virtual bool initializeOutputSurface() OVERRIDE;
    virtual void setSurfaceReady() OVERRIDE;
    virtual void setVisible(bool) OVERRIDE;
    virtual bool initializeRenderer() OVERRIDE;
    virtual bool recreateOutputSurface() OVERRIDE;
    virtual void renderingStats(RenderingStats*) OVERRIDE;
    virtual const RendererCapabilities& rendererCapabilities() const OVERRIDE;
    virtual void setNeedsAnimate() OVERRIDE;
    virtual void setNeedsCommit() OVERRIDE;
    virtual void setNeedsRedraw() OVERRIDE;
    virtual void setDeferCommits(bool) OVERRIDE;
    virtual bool commitRequested() const OVERRIDE;
    virtual void mainThreadHasStoppedFlinging() OVERRIDE;
    virtual void start() OVERRIDE;
    virtual void stop() OVERRIDE;
    virtual size_t maxPartialTextureUpdates() const OVERRIDE;
    virtual void acquireLayerTextures() OVERRIDE;
    virtual void forceSerializeOnSwapBuffers() OVERRIDE;
    virtual bool commitPendingForTesting() OVERRIDE;
    virtual skia::RefPtr<SkPicture> capturePicture() OVERRIDE;

    // LayerTreeHostImplClient implementation
    virtual void didLoseOutputSurfaceOnImplThread() OVERRIDE;
    virtual void onSwapBuffersCompleteOnImplThread() OVERRIDE;
    virtual void onVSyncParametersChanged(base::TimeTicks timebase, base::TimeDelta interval) OVERRIDE;
    virtual void onCanDrawStateChanged(bool canDraw) OVERRIDE;
    virtual void onHasPendingTreeStateChanged(bool hasPendingTree) OVERRIDE;
    virtual void setNeedsRedrawOnImplThread() OVERRIDE;
    virtual void didSwapUseIncompleteTextureOnImplThread() OVERRIDE;
    virtual void didUploadVisibleHighResolutionTileOnImplTread() OVERRIDE;
    virtual void setNeedsCommitOnImplThread() OVERRIDE;
    virtual void setNeedsManageTilesOnImplThread() OVERRIDE;
    virtual void postAnimationEventsToMainThreadOnImplThread(scoped_ptr<AnimationEventsVector>, base::Time wallClockTime) OVERRIDE;
    virtual bool reduceContentsTextureMemoryOnImplThread(size_t limitBytes, int priorityCutoff) OVERRIDE;
    virtual void sendManagedMemoryStats() OVERRIDE;
    virtual bool isInsideDraw() OVERRIDE;
    virtual void renewTreePriority() OVERRIDE;

    // SchedulerClient implementation
    virtual void scheduledActionBeginFrame() OVERRIDE;
    virtual ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapIfPossible() OVERRIDE;
    virtual ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapForced() OVERRIDE;
    virtual void scheduledActionCommit() OVERRIDE;
    virtual void scheduledActionCheckForCompletedTextures() OVERRIDE;
    virtual void scheduledActionActivatePendingTreeIfNeeded() OVERRIDE;
    virtual void scheduledActionBeginContextRecreation() OVERRIDE;
    virtual void scheduledActionAcquireLayerTexturesForMainThread() OVERRIDE;
    virtual void didAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE;

    // ResourceUpdateControllerClient implementation
    virtual void readyToFinalizeTextureUpdates() OVERRIDE;

    int maxFramesPendingForTesting() const { return m_schedulerOnImplThread->maxFramesPending(); }

private:
    ThreadProxy(LayerTreeHost*, scoped_ptr<Thread> implThread);

    struct BeginFrameAndCommitState {
        BeginFrameAndCommitState();
        ~BeginFrameAndCommitState();

        base::TimeTicks monotonicFrameBeginTime;
        scoped_ptr<ScrollAndScaleSet> scrollInfo;
        gfx::Transform implTransform;
        size_t memoryAllocationLimitBytes;
    };

    // Called on main thread
    void beginFrame(scoped_ptr<BeginFrameAndCommitState> beginFrameState);
    void didCommitAndDrawFrame();
    void didCompleteSwapBuffers();
    void setAnimationEvents(scoped_ptr<AnimationEventsVector>, base::Time wallClockTime);
    void beginContextRecreation();
    void tryToRecreateOutputSurface();

    // Called on impl thread
    struct ReadbackRequest {
        CompletionEvent completion;
        bool success;
        void* pixels;
        gfx::Rect rect;
    };
    struct CommitPendingRequest {
        CompletionEvent completion;
        bool commitPending;
    };
    void forceBeginFrameOnImplThread(CompletionEvent*);
    void beginFrameCompleteOnImplThread(CompletionEvent*, ResourceUpdateQueue*);
    void beginFrameAbortedOnImplThread();
    void requestReadbackOnImplThread(ReadbackRequest*);
    void requestStartPageScaleAnimationOnImplThread(gfx::Vector2d targetOffset, bool useAnchor, float scale, base::TimeDelta duration);
    void finishAllRenderingOnImplThread(CompletionEvent*);
    void initializeImplOnImplThread(CompletionEvent*, InputHandler*);
    void setSurfaceReadyOnImplThread();
    void setVisibleOnImplThread(CompletionEvent*, bool);
    void initializeOutputSurfaceOnImplThread(scoped_ptr<OutputSurface>);
    void initializeRendererOnImplThread(CompletionEvent*, bool* initializeSucceeded, RendererCapabilities*);
    void layerTreeHostClosedOnImplThread(CompletionEvent*);
    void manageTilesOnImplThread();
    void setFullRootLayerDamageOnImplThread();
    void acquireLayerTexturesForMainThreadOnImplThread(CompletionEvent*);
    void recreateOutputSurfaceOnImplThread(CompletionEvent*, scoped_ptr<OutputSurface>, bool* recreateSucceeded, RendererCapabilities*);
    void renderingStatsOnImplThread(CompletionEvent*, RenderingStats*);
    ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapInternal(bool forcedDraw);
    void forceSerializeOnSwapBuffersOnImplThread(CompletionEvent*);
    void setNeedsForcedCommitOnImplThread();
    void commitPendingOnImplThreadForTesting(CommitPendingRequest* request);
    void capturePictureOnImplThread(CompletionEvent*, skia::RefPtr<SkPicture>*);
    void renewTreePriorityOnImplThread();

    // Accessed on main thread only.
    bool m_animateRequested; // Set only when setNeedsAnimate is called.
    bool m_commitRequested; // Set only when setNeedsCommit is called.
    bool m_commitRequestSentToImplThread; // Set by setNeedsCommit and setNeedsAnimate.
    base::CancelableClosure m_outputSurfaceRecreationCallback;
    LayerTreeHost* m_layerTreeHost;
    bool m_rendererInitialized;
    RendererCapabilities m_RendererCapabilitiesMainThreadCopy;
    bool m_started;
    bool m_texturesAcquired;
    bool m_inCompositeAndReadback;
    bool m_manageTilesPending;
    // Weak pointer to use when posting tasks to the impl thread.
    base::WeakPtr<ThreadProxy> m_implThreadWeakPtr;

    base::WeakPtrFactory<ThreadProxy> m_weakFactoryOnImplThread;

    base::WeakPtr<ThreadProxy> m_mainThreadWeakPtr;
    base::WeakPtrFactory<ThreadProxy> m_weakFactory;

    scoped_ptr<LayerTreeHostImpl> m_layerTreeHostImpl;

    scoped_ptr<InputHandler> m_inputHandlerOnImplThread;

    scoped_ptr<Scheduler> m_schedulerOnImplThread;

    // Holds on to the context we might use for compositing in between initializeContext()
    // and initializeRenderer() calls.
    scoped_ptr<OutputSurface> m_outputSurfaceBeforeInitializationOnImplThread;

    // Set when the main thread is waiting on a scheduledActionBeginFrame to be issued.
    CompletionEvent* m_beginFrameCompletionEventOnImplThread;

    // Set when the main thread is waiting on a readback.
    ReadbackRequest* m_readbackRequestOnImplThread;

    // Set when the main thread is waiting on a commit to complete.
    CompletionEvent* m_commitCompletionEventOnImplThread;

    // Set when the main thread is waiting on a pending tree activation.
    CompletionEvent* m_completionEventForCommitHeldOnTreeActivation;

    // Set when the main thread is waiting on layers to be drawn.
    CompletionEvent* m_textureAcquisitionCompletionEventOnImplThread;

    scoped_ptr<ResourceUpdateController> m_currentResourceUpdateControllerOnImplThread;

    // Set when the next draw should post didCommitAndDrawFrame to the main thread.
    bool m_nextFrameIsNewlyCommittedFrameOnImplThread;

    bool m_renderVSyncEnabled;

    bool m_insideDraw;

    base::TimeDelta m_totalCommitTime;
    size_t m_totalCommitCount;

    bool m_deferCommits;
    scoped_ptr<BeginFrameAndCommitState> m_pendingDeferredCommit;
};

}  // namespace cc

#endif  // CC_THREAD_PROXY_H_
