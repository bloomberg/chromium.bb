// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCSingleThreadProxy_h
#define CCSingleThreadProxy_h

#include "CCAnimationEvents.h"
#include "CCLayerTreeHostImpl.h"
#include "CCProxy.h"
#include <limits>
#include <wtf/OwnPtr.h>

namespace cc {

class CCLayerTreeHost;

class CCSingleThreadProxy : public CCProxy, CCLayerTreeHostImplClient {
public:
    static PassOwnPtr<CCProxy> create(CCLayerTreeHost*);
    virtual ~CCSingleThreadProxy();

    // CCProxy implementation
    virtual bool compositeAndReadback(void *pixels, const IntRect&) OVERRIDE;
    virtual void startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double duration) OVERRIDE;
    virtual void finishAllRendering() OVERRIDE;
    virtual bool isStarted() const OVERRIDE;
    virtual bool initializeContext() OVERRIDE;
    virtual void setSurfaceReady() OVERRIDE;
    virtual void setVisible(bool) OVERRIDE;
    virtual bool initializeRenderer() OVERRIDE;
    virtual bool recreateContext() OVERRIDE;
    virtual void implSideRenderingStats(CCRenderingStats&) OVERRIDE;
    virtual const RendererCapabilities& rendererCapabilities() const OVERRIDE;
    virtual void loseContext() OVERRIDE;
    virtual void setNeedsAnimate() OVERRIDE;
    virtual void setNeedsCommit() OVERRIDE;
    virtual void setNeedsRedraw() OVERRIDE;
    virtual bool commitRequested() const OVERRIDE;
    virtual void didAddAnimation() OVERRIDE;
    virtual void start() OVERRIDE;
    virtual void stop() OVERRIDE;
    virtual size_t maxPartialTextureUpdates() const OVERRIDE { return std::numeric_limits<size_t>::max(); }
    virtual void acquireLayerTextures() OVERRIDE { }
    virtual void forceSerializeOnSwapBuffers() OVERRIDE;

    // CCLayerTreeHostImplClient implementation
    virtual void didLoseContextOnImplThread() OVERRIDE { }
    virtual void onSwapBuffersCompleteOnImplThread() OVERRIDE { ASSERT_NOT_REACHED(); }
    virtual void onVSyncParametersChanged(double monotonicTimebase, double intervalInSeconds) OVERRIDE { }
    virtual void onCanDrawStateChanged(bool canDraw) OVERRIDE { }
    virtual void setNeedsRedrawOnImplThread() OVERRIDE { m_layerTreeHost->scheduleComposite(); }
    virtual void setNeedsCommitOnImplThread() OVERRIDE { m_layerTreeHost->scheduleComposite(); }
    virtual void postAnimationEventsToMainThreadOnImplThread(PassOwnPtr<CCAnimationEventsVector>, double wallClockTime) OVERRIDE;

    // Called by the legacy path where RenderWidget does the scheduling.
    void compositeImmediately();

private:
    explicit CCSingleThreadProxy(CCLayerTreeHost*);

    bool commitAndComposite();
    void doCommit(CCTextureUpdateQueue&);
    bool doComposite();
    void didSwapFrame();

    // Accessed on main thread only.
    CCLayerTreeHost* m_layerTreeHost;
    bool m_contextLost;

    // Holds on to the context between initializeContext() and initializeRenderer() calls. Shouldn't
    // be used for anything else.
    OwnPtr<CCGraphicsContext> m_contextBeforeInitialization;

    // Used on the CCThread, but checked on main thread during initialization/shutdown.
    OwnPtr<CCLayerTreeHostImpl> m_layerTreeHostImpl;
    bool m_rendererInitialized;
    RendererCapabilities m_RendererCapabilitiesForMainThread;

    bool m_nextFrameIsNewlyCommittedFrame;
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the impl thread to satisfy assertion checks.
class DebugScopedSetImplThread {
public:
    DebugScopedSetImplThread()
    {
#if !ASSERT_DISABLED
        CCProxy::setCurrentThreadIsImplThread(true);
#endif
    }
    ~DebugScopedSetImplThread()
    {
#if !ASSERT_DISABLED
        CCProxy::setCurrentThreadIsImplThread(false);
#endif
    }
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the main thread to satisfy assertion checks.
class DebugScopedSetMainThread {
public:
    DebugScopedSetMainThread()
    {
#if !ASSERT_DISABLED
        CCProxy::setCurrentThreadIsImplThread(false);
#endif
    }
    ~DebugScopedSetMainThread()
    {
#if !ASSERT_DISABLED
        CCProxy::setCurrentThreadIsImplThread(true);
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
