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

class ContextProvider;
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
    virtual bool CompositeAndReadback(void* pixels, gfx::Rect rect) OVERRIDE;
    virtual void StartPageScaleAnimation(gfx::Vector2d targetOffset, bool useAnchor, float scale, base::TimeDelta duration) OVERRIDE;
    virtual void FinishAllRendering() OVERRIDE;
    virtual bool IsStarted() const OVERRIDE;
    virtual bool InitializeOutputSurface() OVERRIDE;
    virtual void SetSurfaceReady() OVERRIDE;
    virtual void SetVisible(bool) OVERRIDE;
    virtual bool InitializeRenderer() OVERRIDE;
    virtual bool RecreateOutputSurface() OVERRIDE;
    virtual void CollectRenderingStats(RenderingStats* stats) OVERRIDE;
    virtual const RendererCapabilities& GetRendererCapabilities() const OVERRIDE;
    virtual void SetNeedsAnimate() OVERRIDE;
    virtual void SetNeedsCommit() OVERRIDE;
    virtual void SetNeedsRedraw() OVERRIDE;
    virtual void SetDeferCommits(bool) OVERRIDE;
    virtual bool CommitRequested() const OVERRIDE;
    virtual void MainThreadHasStoppedFlinging() OVERRIDE;
    virtual void Start() OVERRIDE;
    virtual void Stop() OVERRIDE;
    virtual size_t MaxPartialTextureUpdates() const OVERRIDE;
    virtual void AcquireLayerTextures() OVERRIDE;
    virtual void ForceSerializeOnSwapBuffers() OVERRIDE;
    virtual skia::RefPtr<SkPicture> CapturePicture() OVERRIDE;
    virtual scoped_ptr<base::Value> AsValue() const OVERRIDE;
    virtual bool CommitPendingForTesting() OVERRIDE;

    // LayerTreeHostImplClient implementation
    virtual void DidLoseOutputSurfaceOnImplThread() OVERRIDE;
    virtual void OnSwapBuffersCompleteOnImplThread() OVERRIDE;
    virtual void OnVSyncParametersChanged(base::TimeTicks timebase, base::TimeDelta interval) OVERRIDE;
    virtual void OnCanDrawStateChanged(bool canDraw) OVERRIDE;
    virtual void OnHasPendingTreeStateChanged(bool hasPendingTree) OVERRIDE;
    virtual void SetNeedsRedrawOnImplThread() OVERRIDE;
    virtual void DidUploadVisibleHighResolutionTileOnImplThread() OVERRIDE;
    virtual void SetNeedsCommitOnImplThread() OVERRIDE;
    virtual void SetNeedsManageTilesOnImplThread() OVERRIDE;
    virtual void PostAnimationEventsToMainThreadOnImplThread(scoped_ptr<AnimationEventsVector>, base::Time wallClockTime) OVERRIDE;
    virtual bool ReduceContentsTextureMemoryOnImplThread(size_t limitBytes, int priorityCutoff) OVERRIDE;
    virtual void ReduceWastedContentsTextureMemoryOnImplThread() OVERRIDE;
    virtual void SendManagedMemoryStats() OVERRIDE;
    virtual bool IsInsideDraw() OVERRIDE;
    virtual void RenewTreePriority() OVERRIDE;

    // SchedulerClient implementation
    virtual void scheduledActionBeginFrame() OVERRIDE;
    virtual ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapIfPossible() OVERRIDE;
    virtual ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapForced() OVERRIDE;
    virtual void scheduledActionCommit() OVERRIDE;
    virtual void scheduledActionCheckForCompletedTileUploads() OVERRIDE;
    virtual void scheduledActionActivatePendingTreeIfNeeded() OVERRIDE;
    virtual void scheduledActionBeginContextRecreation() OVERRIDE;
    virtual void scheduledActionAcquireLayerTexturesForMainThread() OVERRIDE;
    virtual void didAnticipatedDrawTimeChange(base::TimeTicks) OVERRIDE;

    // ResourceUpdateControllerClient implementation
    virtual void ReadyToFinalizeTextureUpdates() OVERRIDE;

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
    void beginFrameCompleteOnImplThread(CompletionEvent*, ResourceUpdateQueue*, scoped_refptr<cc::ContextProvider> offscreenContextProvider);
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
    void recreateOutputSurfaceOnImplThread(CompletionEvent*, scoped_ptr<OutputSurface>, scoped_refptr<cc::ContextProvider> offscreenContextProvider, bool* recreateSucceeded, RendererCapabilities*);
    void renderingStatsOnImplThread(CompletionEvent*, RenderingStats*);
    ScheduledActionDrawAndSwapResult scheduledActionDrawAndSwapInternal(bool forcedDraw);
    void forceSerializeOnSwapBuffersOnImplThread(CompletionEvent*);
    void setNeedsForcedCommitOnImplThread();
    void checkOutputSurfaceStatusOnImplThread();
    void commitPendingOnImplThreadForTesting(CommitPendingRequest* request);
    void capturePictureOnImplThread(CompletionEvent*, skia::RefPtr<SkPicture>*);
    void asValueOnImplThread(CompletionEvent*, base::DictionaryValue*) const;
    void renewTreePriorityOnImplThread();
    void didSwapUseIncompleteTileOnImplThread();

    // Accessed on main thread only.
    bool m_animateRequested; // Set only when setNeedsAnimate is called.
    bool m_commitRequested; // Set only when setNeedsCommit is called.
    bool m_commitRequestSentToImplThread; // Set by setNeedsCommit and setNeedsAnimate.
    bool m_createdOffscreenContextProvider; // Set by beginFrame.
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
    // and InitializeRenderer() calls.
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

    base::TimeTicks m_smoothnessTakesPriorityExpirationTime;
    bool m_renewTreePriorityOnImplThreadPending;
};

}  // namespace cc

#endif  // CC_THREAD_PROXY_H_
