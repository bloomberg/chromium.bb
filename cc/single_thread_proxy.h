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

class LayerTreeHost;

class SingleThreadProxy : public Proxy, LayerTreeHostImplClient {
public:
    static scoped_ptr<Proxy> create(LayerTreeHost*);
    virtual ~SingleThreadProxy();

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
    virtual void mainThreadHasStoppedFlinging() OVERRIDE { }
    virtual void start() OVERRIDE;
    virtual void stop() OVERRIDE;
    virtual size_t maxPartialTextureUpdates() const OVERRIDE;
    virtual void acquireLayerTextures() OVERRIDE { }
    virtual void forceSerializeOnSwapBuffers() OVERRIDE;
    virtual bool commitPendingForTesting() OVERRIDE;
    virtual skia::RefPtr<SkPicture> capturePicture() OVERRIDE;

    // LayerTreeHostImplClient implementation
    virtual void didLoseOutputSurfaceOnImplThread() OVERRIDE { }
    virtual void onSwapBuffersCompleteOnImplThread() OVERRIDE;
    virtual void onVSyncParametersChanged(base::TimeTicks timebase, base::TimeDelta interval) OVERRIDE { }
    virtual void onCanDrawStateChanged(bool canDraw) OVERRIDE { }
    virtual void onHasPendingTreeStateChanged(bool havePendingTree) OVERRIDE;
    virtual void setNeedsRedrawOnImplThread() OVERRIDE;
    virtual void setNeedsCommitOnImplThread() OVERRIDE;
    virtual void setNeedsManageTilesOnImplThread() OVERRIDE;
    virtual void postAnimationEventsToMainThreadOnImplThread(scoped_ptr<AnimationEventsVector>, base::Time wallClockTime) OVERRIDE;
    virtual bool reduceContentsTextureMemoryOnImplThread(size_t limitBytes, int priorityCutoff) OVERRIDE;
    virtual void sendManagedMemoryStats() OVERRIDE;

    // Called by the legacy path where RenderWidget does the scheduling.
    void compositeImmediately();

private:
    explicit SingleThreadProxy(LayerTreeHost*);

    bool commitAndComposite();
    void doCommit(scoped_ptr<ResourceUpdateQueue>);
    bool doComposite();
    void didSwapFrame();

    // Accessed on main thread only.
    LayerTreeHost* m_layerTreeHost;
    bool m_outputSurfaceLost;

    // Holds on to the context between initializeContext() and initializeRenderer() calls. Shouldn't
    // be used for anything else.
    scoped_ptr<OutputSurface> m_outputSurfaceBeforeInitialization;

    // Used on the Thread, but checked on main thread during initialization/shutdown.
    scoped_ptr<LayerTreeHostImpl> m_layerTreeHostImpl;
    bool m_rendererInitialized;
    RendererCapabilities m_RendererCapabilitiesForMainThread;

    bool m_nextFrameIsNewlyCommittedFrame;

    base::TimeDelta m_totalCommitTime;
    size_t m_totalCommitCount;
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the impl thread to satisfy assertion checks.
class DebugScopedSetImplThread {
public:
    explicit DebugScopedSetImplThread(Proxy* proxy)
        : m_proxy(proxy)
    {
#ifndef NDEBUG
        m_previousValue = m_proxy->m_implThreadIsOverridden;
        m_proxy->setCurrentThreadIsImplThread(true);
#endif
    }
    ~DebugScopedSetImplThread()
    {
#ifndef NDEBUG
        m_proxy->setCurrentThreadIsImplThread(m_previousValue);
#endif
    }
private:
    bool m_previousValue;
    Proxy* m_proxy;
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the main thread to satisfy assertion checks.
class DebugScopedSetMainThread {
public:
    explicit DebugScopedSetMainThread(Proxy* proxy)
        : m_proxy(proxy)
    {
#ifndef NDEBUG
        m_previousValue = m_proxy->m_implThreadIsOverridden;
        m_proxy->setCurrentThreadIsImplThread(false);
#endif
    }
    ~DebugScopedSetMainThread()
    {
#ifndef NDEBUG
        m_proxy->setCurrentThreadIsImplThread(m_previousValue);
#endif
    }
private:
    bool m_previousValue;
    Proxy* m_proxy;
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the impl thread and that the main thread is blocked to
// satisfy assertion checks
class DebugScopedSetImplThreadAndMainThreadBlocked {
public:
    explicit DebugScopedSetImplThreadAndMainThreadBlocked(Proxy* proxy)
        : m_implThread(proxy)
        , m_mainThreadBlocked(proxy)
    {
    }
private:
    DebugScopedSetImplThread m_implThread;
    DebugScopedSetMainThreadBlocked m_mainThreadBlocked;
};

} // namespace cc

#endif  // CC_SINGLE_THREAD_PROXY_H_
