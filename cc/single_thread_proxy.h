// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCSingleThreadProxy_h
#define CCSingleThreadProxy_h

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
    virtual void didAddAnimation() OVERRIDE;
    virtual void start() OVERRIDE;
    virtual void stop() OVERRIDE;
    virtual size_t maxPartialTextureUpdates() const OVERRIDE;
    virtual void acquireLayerTextures() OVERRIDE { }
    virtual void forceSerializeOnSwapBuffers() OVERRIDE;

    // LayerTreeHostImplClient implementation
    virtual void didLoseContextOnImplThread() OVERRIDE { }
    virtual void onSwapBuffersCompleteOnImplThread() OVERRIDE;
    virtual void onVSyncParametersChanged(double monotonicTimebase, double intervalInSeconds) OVERRIDE { }
    virtual void onCanDrawStateChanged(bool canDraw) OVERRIDE { }
    virtual void setNeedsRedrawOnImplThread() OVERRIDE;
    virtual void setNeedsCommitOnImplThread() OVERRIDE;
    virtual void postAnimationEventsToMainThreadOnImplThread(scoped_ptr<AnimationEventsVector>, double wallClockTime) OVERRIDE;
    virtual bool reduceContentsTextureMemoryOnImplThread(size_t limitBytes, int priorityCutoff) OVERRIDE;
    virtual void sendManagedMemoryStats() OVERRIDE;

    // Called by the legacy path where RenderWidget does the scheduling.
    void compositeImmediately();

private:
    explicit SingleThreadProxy(LayerTreeHost*);

    bool commitAndComposite();
    void doCommit(scoped_ptr<TextureUpdateQueue>);
    bool doComposite();
    void didSwapFrame();

    // Accessed on main thread only.
    LayerTreeHost* m_layerTreeHost;
    bool m_contextLost;

    // Holds on to the context between initializeContext() and initializeRenderer() calls. Shouldn't
    // be used for anything else.
    scoped_ptr<GraphicsContext> m_contextBeforeInitialization;

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
    DebugScopedSetImplThread()
    {
#ifndef NDEBUG
        Proxy::setCurrentThreadIsImplThread(true);
#endif
    }
    ~DebugScopedSetImplThread()
    {
#ifndef NDEBUG
        Proxy::setCurrentThreadIsImplThread(false);
#endif
    }
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the main thread to satisfy assertion checks.
class DebugScopedSetMainThread {
public:
    DebugScopedSetMainThread()
    {
#ifndef NDEBUG
        Proxy::setCurrentThreadIsImplThread(false);
#endif
    }
    ~DebugScopedSetMainThread()
    {
#ifndef NDEBUG
        Proxy::setCurrentThreadIsImplThread(true);
#endif
    }
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the impl thread and that the main thread is blocked to
// satisfy assertion checks
class DebugScopedSetImplThreadAndMainThreadBlocked {
private:
    DebugScopedSetImplThread m_implThread;
    DebugScopedSetMainThreadBlocked m_mainThreadBlocked;
};

} // namespace cc

#endif
