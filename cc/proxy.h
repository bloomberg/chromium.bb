// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PROXY_H_
#define CC_PROXY_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/time.h"
#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"

namespace gfx {
class Rect;
class Vector2d;
}

namespace cc {

class Thread;
struct RenderingStats;
struct RendererCapabilities;

// Abstract class responsible for proxying commands from the main-thread side of
// the compositor over to the compositor implementation.
class CC_EXPORT Proxy {
public:
    Thread* mainThread() const;
    bool hasImplThread() const;
    Thread* implThread() const;

    // Returns 0 if the current thread is neither the main thread nor the impl thread.
    Thread* currentThread() const;

    // Debug hooks
    bool isMainThread() const;
    bool isImplThread() const;
    bool isMainThreadBlocked() const;
#ifndef NDEBUG
    void setMainThreadBlocked(bool);
    void setCurrentThreadIsImplThread(bool);
#endif

    virtual ~Proxy();

    virtual bool compositeAndReadback(void *pixels, const gfx::Rect&) = 0;

    virtual void startPageScaleAnimation(gfx::Vector2d targetOffset, bool useAnchor, float scale, base::TimeDelta duration) = 0;

    virtual void finishAllRendering() = 0;

    virtual bool isStarted() const = 0;

    // Attempts to initialize a context to use for rendering. Returns false if the context could not be created.
    // The context will not be used and no frames may be produced until initializeRenderer() is called.
    virtual bool initializeOutputSurface() = 0;

    // Indicates that the compositing surface associated with our context is ready to use.
    virtual void setSurfaceReady() = 0;

    virtual void setVisible(bool) = 0;

    // Attempts to initialize the layer renderer. Returns false if the context isn't usable for compositing.
    virtual bool initializeRenderer() = 0;

    // Attempts to recreate the context and layer renderer after a context lost. Returns false if the renderer couldn't be
    // reinitialized.
    virtual bool recreateOutputSurface() = 0;

    virtual void renderingStats(RenderingStats*) = 0;

    virtual const RendererCapabilities& rendererCapabilities() const = 0;

    virtual void setNeedsAnimate() = 0;
    virtual void setNeedsCommit() = 0;
    virtual void setNeedsRedraw() = 0;

    // Defers commits until it is reset. It is only supported when in threaded mode. It's an error to make a sync call
    // like compositeAndReadback while commits are deferred.
    virtual void setDeferCommits(bool) = 0;

    virtual void mainThreadHasStoppedFlinging() = 0;

    virtual bool commitRequested() const = 0;

    virtual void start() = 0; // Must be called before using the proxy.
    virtual void stop() = 0; // Must be called before deleting the proxy.

    // Forces 3D commands on all contexts to wait for all previous SwapBuffers to finish before executing in the GPU
    // process.
    virtual void forceSerializeOnSwapBuffers() = 0;

    // Maximum number of sub-region texture updates supported for each commit.
    virtual size_t maxPartialTextureUpdates() const = 0;

    virtual void acquireLayerTextures() = 0;

    // Testing hooks
    virtual bool commitPendingForTesting() = 0;

protected:
    explicit Proxy(scoped_ptr<Thread> implThread);
    friend class DebugScopedSetImplThread;
    friend class DebugScopedSetMainThread;
    friend class DebugScopedSetMainThreadBlocked;

private:
    DISALLOW_COPY_AND_ASSIGN(Proxy);

    scoped_ptr<Thread> m_mainThread;
    scoped_ptr<Thread> m_implThread;
#ifndef NDEBUG
    bool m_implThreadIsOverridden;
    bool m_isMainThreadBlocked;
#endif
};

#ifndef NDEBUG
class DebugScopedSetMainThreadBlocked {
public:
    explicit DebugScopedSetMainThreadBlocked(Proxy* proxy)
        : m_proxy(proxy)
    {
        DCHECK(!m_proxy->isMainThreadBlocked());
        m_proxy->setMainThreadBlocked(true);
    }
    ~DebugScopedSetMainThreadBlocked()
    {
        DCHECK(m_proxy->isMainThreadBlocked());
        m_proxy->setMainThreadBlocked(false);
    }
private:
    Proxy* m_proxy;
};
#else
class DebugScopedSetMainThreadBlocked {
public:
    explicit DebugScopedSetMainThreadBlocked(Proxy*) { }
    ~DebugScopedSetMainThreadBlocked() { }
};
#endif

}

#endif  // CC_PROXY_H_
