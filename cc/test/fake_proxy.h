// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeCCProxy_h
#define FakeCCProxy_h

#include "config.h"

#include "cc/layer_tree_host.h"
#include "cc/proxy.h"
#include "cc/thread.h"

namespace cc {

class FakeProxy : public Proxy {
public:
    explicit FakeProxy(scoped_ptr<Thread> implThread) : Proxy(implThread.Pass()) { }

    virtual bool compositeAndReadback(void *pixels, const gfx::Rect&) OVERRIDE;
    virtual void startPageScaleAnimation(gfx::Vector2d targetPosition, bool useAnchor, float scale, base::TimeDelta duration) OVERRIDE { }
    virtual void finishAllRendering() OVERRIDE { }
    virtual bool isStarted() const OVERRIDE;
    virtual bool initializeContext() OVERRIDE;
    virtual void setSurfaceReady() OVERRIDE { }
    virtual void setVisible(bool) OVERRIDE { }
    virtual bool initializeRenderer() OVERRIDE;
    virtual bool recreateContext() OVERRIDE;
    virtual void renderingStats(RenderingStats*) OVERRIDE { }
    virtual const RendererCapabilities& rendererCapabilities() const OVERRIDE;
    virtual void setNeedsAnimate() OVERRIDE { }
    virtual void setNeedsCommit() OVERRIDE { }
    virtual void setNeedsRedraw() OVERRIDE { }
    virtual void setDeferCommits(bool) OVERRIDE { }
    virtual void didAddAnimation() OVERRIDE { }
    virtual bool commitRequested() const OVERRIDE;
    virtual void start() OVERRIDE { }
    virtual void stop() OVERRIDE { }
    virtual void forceSerializeOnSwapBuffers() OVERRIDE { }
    virtual size_t maxPartialTextureUpdates() const OVERRIDE;
    virtual void acquireLayerTextures() OVERRIDE { }
    virtual void loseContext() OVERRIDE { }

private:
    RendererCapabilities m_capabilities;
};

}

#endif
