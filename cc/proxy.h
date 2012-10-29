// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCProxy_h
#define CCProxy_h

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/time.h"
#include <public/WebCompositorOutputSurface.h>

namespace cc {

class Thread;
class IntRect;
class IntSize;
struct RenderingStats;
struct RendererCapabilities;

// Abstract class responsible for proxying commands from the main-thread side of
// the compositor over to the compositor implementation.
class Proxy {
public:
    static void setMainThread(Thread*);
    static Thread* mainThread();

    static bool hasImplThread();
    static void setImplThread(Thread*);
    static Thread* implThread();

    // Returns 0 if the current thread is neither the main thread nor the impl thread.
    static Thread* currentThread();

    virtual ~Proxy();

    virtual bool compositeAndReadback(void *pixels, const IntRect&) = 0;

    virtual void startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, base::TimeDelta duration) = 0;

    virtual void finishAllRendering() = 0;

    virtual bool isStarted() const = 0;

    // Attempts to initialize a context to use for rendering. Returns false if the context could not be created.
    // The context will not be used and no frames may be produced until initializeRenderer() is called.
    virtual bool initializeContext() = 0;

    // Indicates that the compositing surface associated with our context is ready to use.
    virtual void setSurfaceReady() = 0;

    virtual void setVisible(bool) = 0;

    // Attempts to initialize the layer renderer. Returns false if the context isn't usable for compositing.
    virtual bool initializeRenderer() = 0;

    // Attempts to recreate the context and layer renderer after a context lost. Returns false if the renderer couldn't be
    // reinitialized.
    virtual bool recreateContext() = 0;

    virtual void renderingStats(RenderingStats*) = 0;

    virtual const RendererCapabilities& rendererCapabilities() const = 0;

    virtual void setNeedsAnimate() = 0;
    virtual void setNeedsCommit() = 0;
    virtual void setNeedsRedraw() = 0;

    // Defers commits until it is reset. It is only supported when in threaded mode. It's an error to make a sync call
    // like compositeAndReadback while commits are deferred.
    virtual void setDeferCommits(bool) = 0;

    virtual void didAddAnimation() = 0;

    virtual bool commitRequested() const = 0;

    virtual void start() = 0; // Must be called before using the proxy.
    virtual void stop() = 0; // Must be called before deleting the proxy.

    // Forces 3D commands on all contexts to wait for all previous SwapBuffers to finish before executing in the GPU
    // process.
    virtual void forceSerializeOnSwapBuffers() = 0;

    // Maximum number of sub-region texture updates supported for each commit.
    virtual size_t maxPartialTextureUpdates() const = 0;

    virtual void acquireLayerTextures() = 0;

    // Debug hooks
    static bool isMainThread();
    static bool isImplThread();
    static bool isMainThreadBlocked();
#ifndef NDEBUG
    static void setMainThreadBlocked(bool);
#endif

    // Testing hooks
    virtual void loseContext() = 0;

#ifndef NDEBUG
    static void setCurrentThreadIsImplThread(bool);
#endif

protected:
    Proxy();
    friend class DebugScopedSetImplThread;
    friend class DebugScopedSetMainThreadBlocked;

private:
    DISALLOW_COPY_AND_ASSIGN(Proxy);
};

class DebugScopedSetMainThreadBlocked {
public:
    DebugScopedSetMainThreadBlocked()
    {
#ifndef NDEBUG
        DCHECK(!Proxy::isMainThreadBlocked());
        Proxy::setMainThreadBlocked(true);
#endif
    }
    ~DebugScopedSetMainThreadBlocked()
    {
#ifndef NDEBUG
        DCHECK(Proxy::isMainThreadBlocked());
        Proxy::setMainThreadBlocked(false);
#endif
    }
};

}

#endif
