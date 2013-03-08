// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SINGLE_THREAD_PROXY_H_
#define CC_SINGLE_THREAD_PROXY_H_

#include <limits>

#include "base/time.h"
#include "cc/animation_events.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/proxy.h"

namespace cc {

class ContextProvider;
class LayerTreeHost;

class SingleThreadProxy : public Proxy, LayerTreeHostImplClient {
public:
    static scoped_ptr<Proxy> create(LayerTreeHost*);
    virtual ~SingleThreadProxy();

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
    virtual void GetRenderingStats(RenderingStats*) OVERRIDE;
    virtual const RendererCapabilities& GetRendererCapabilities() const OVERRIDE;
    virtual void SetNeedsAnimate() OVERRIDE;
    virtual void SetNeedsCommit() OVERRIDE;
    virtual void SetNeedsRedraw() OVERRIDE;
    virtual void SetDeferCommits(bool) OVERRIDE;
    virtual bool CommitRequested() const OVERRIDE;
    virtual void MainThreadHasStoppedFlinging() OVERRIDE { }
    virtual void Start() OVERRIDE;
    virtual void Stop() OVERRIDE;
    virtual size_t MaxPartialTextureUpdates() const OVERRIDE;
    virtual void AcquireLayerTextures() OVERRIDE { }
    virtual void ForceSerializeOnSwapBuffers() OVERRIDE;
    virtual skia::RefPtr<SkPicture> CapturePicture() OVERRIDE;
    virtual scoped_ptr<base::Value> AsValue() const OVERRIDE;
    virtual bool CommitPendingForTesting() OVERRIDE;

    // LayerTreeHostImplClient implementation
    virtual void didLoseOutputSurfaceOnImplThread() OVERRIDE;
    virtual void onSwapBuffersCompleteOnImplThread() OVERRIDE;
    virtual void onVSyncParametersChanged(base::TimeTicks timebase, base::TimeDelta interval) OVERRIDE { }
    virtual void onCanDrawStateChanged(bool canDraw) OVERRIDE { }
    virtual void onHasPendingTreeStateChanged(bool havePendingTree) OVERRIDE;
    virtual void setNeedsRedrawOnImplThread() OVERRIDE;
    virtual void didUploadVisibleHighResolutionTileOnImplThread() OVERRIDE;
    virtual void setNeedsCommitOnImplThread() OVERRIDE;
    virtual void setNeedsManageTilesOnImplThread() OVERRIDE;
    virtual void postAnimationEventsToMainThreadOnImplThread(scoped_ptr<AnimationEventsVector>, base::Time wallClockTime) OVERRIDE;
    virtual bool reduceContentsTextureMemoryOnImplThread(size_t limitBytes, int priorityCutoff) OVERRIDE;
    virtual void reduceWastedContentsTextureMemoryOnImplThread() OVERRIDE;
    virtual void sendManagedMemoryStats() OVERRIDE;
    virtual bool isInsideDraw() OVERRIDE;
    virtual void renewTreePriority() OVERRIDE { }

    // Called by the legacy path where RenderWidget does the scheduling.
    void compositeImmediately();

private:
    explicit SingleThreadProxy(LayerTreeHost*);

    bool commitAndComposite();
    void doCommit(scoped_ptr<ResourceUpdateQueue>);
    bool doComposite(scoped_refptr<cc::ContextProvider> offscreenContextProvider);
    void didSwapFrame();

    // Accessed on main thread only.
    LayerTreeHost* m_layerTreeHost;
    bool m_outputSurfaceLost;
    bool m_createdOffscreenContextProvider;

    // Holds on to the context between initializeContext() and InitializeRenderer() calls. Shouldn't
    // be used for anything else.
    scoped_ptr<OutputSurface> m_outputSurfaceBeforeInitialization;

    // Used on the Thread, but checked on main thread during initialization/shutdown.
    scoped_ptr<LayerTreeHostImpl> m_layerTreeHostImpl;
    bool m_rendererInitialized;
    RendererCapabilities m_RendererCapabilitiesForMainThread;

    bool m_nextFrameIsNewlyCommittedFrame;

    bool m_insideDraw;

    base::TimeDelta m_totalCommitTime;
    size_t m_totalCommitCount;
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the impl thread to satisfy assertion checks.
class DebugScopedSetImplThread {
public:
    explicit DebugScopedSetImplThread(Proxy* proxy)
        : proxy_(proxy)
    {
#ifndef NDEBUG
        m_previousValue = proxy_->impl_thread_is_overridden_;
        proxy_->SetCurrentThreadIsImplThread(true);
#endif
    }
    ~DebugScopedSetImplThread()
    {
#ifndef NDEBUG
        proxy_->SetCurrentThreadIsImplThread(m_previousValue);
#endif
    }
private:
    bool m_previousValue;
    Proxy* proxy_;
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the main thread to satisfy assertion checks.
class DebugScopedSetMainThread {
public:
    explicit DebugScopedSetMainThread(Proxy* proxy)
        : proxy_(proxy)
    {
#ifndef NDEBUG
        m_previousValue = proxy_->impl_thread_is_overridden_;
        proxy_->SetCurrentThreadIsImplThread(false);
#endif
    }
    ~DebugScopedSetMainThread()
    {
#ifndef NDEBUG
        proxy_->SetCurrentThreadIsImplThread(m_previousValue);
#endif
    }
private:
    bool m_previousValue;
    Proxy* proxy_;
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the impl thread and that the main thread is blocked to
// satisfy assertion checks
class DebugScopedSetImplThreadAndMainThreadBlocked {
public:
    explicit DebugScopedSetImplThreadAndMainThreadBlocked(Proxy* proxy)
        : impl_thread_(proxy)
        , m_mainThreadBlocked(proxy)
    {
    }
private:
    DebugScopedSetImplThread impl_thread_;
    DebugScopedSetMainThreadBlocked m_mainThreadBlocked;
};

} // namespace cc

#endif  // CC_SINGLE_THREAD_PROXY_H_
