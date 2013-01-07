// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PROXY_H_
#define CC_TEST_FAKE_PROXY_H_

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
    virtual bool initializeOutputSurface() OVERRIDE;
    virtual void setSurfaceReady() OVERRIDE { }
    virtual void setVisible(bool) OVERRIDE { }
    virtual bool initializeRenderer() OVERRIDE;
    virtual bool recreateOutputSurface() OVERRIDE;
    virtual void renderingStats(RenderingStats*) OVERRIDE { }
    virtual const RendererCapabilities& rendererCapabilities() const OVERRIDE;
    virtual void setNeedsAnimate() OVERRIDE { }
    virtual void setNeedsCommit() OVERRIDE { }
    virtual void setNeedsRedraw() OVERRIDE { }
    virtual void setDeferCommits(bool) OVERRIDE { }
    virtual void mainThreadHasStoppedFlinging() OVERRIDE { }
    virtual bool commitRequested() const OVERRIDE;
    virtual void start() OVERRIDE { }
    virtual void stop() OVERRIDE { }
    virtual void forceSerializeOnSwapBuffers() OVERRIDE { }
    virtual size_t maxPartialTextureUpdates() const OVERRIDE;
    virtual void acquireLayerTextures() OVERRIDE { }
    virtual bool commitPendingForTesting() OVERRIDE;

    virtual RendererCapabilities& rendererCapabilities();
    void setMaxPartialTextureUpdates(size_t);

private:
    RendererCapabilities m_capabilities;
    size_t m_maxPartialTextureUpdates;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PROXY_H_
