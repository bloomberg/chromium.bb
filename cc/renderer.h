// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RENDERER_H_
#define CC_RENDERER_H_

#include "base/basictypes.h"
#include "cc/cc_export.h"
#include "cc/layer_tree_host.h"
#include "cc/managed_memory_policy.h"
#include "cc/render_pass.h"

namespace cc {

class CompositorFrameAck;
class CompositorFrameMetadata;
class ScopedResource;

class CC_EXPORT RendererClient {
public:
    virtual const gfx::Size& deviceViewportSize() const = 0;
    virtual const LayerTreeSettings& settings() const = 0;
    virtual void didLoseOutputSurface() = 0;
    virtual void onSwapBuffersComplete() = 0;
    virtual void setFullRootLayerDamage() = 0;
    virtual void setManagedMemoryPolicy(const ManagedMemoryPolicy& policy) = 0;
    virtual void enforceManagedMemoryPolicy(const ManagedMemoryPolicy& policy) = 0;
    virtual bool hasImplThread() const = 0;
    virtual bool shouldClearRootRenderPass() const = 0;
    virtual CompositorFrameMetadata makeCompositorFrameMetadata() const = 0;
protected:
    virtual ~RendererClient() { }
};

class CC_EXPORT Renderer {
public:
    virtual ~Renderer() { }

    virtual const RendererCapabilities& capabilities() const = 0;

    const LayerTreeSettings& settings() const { return m_client->settings(); }

    gfx::Size viewportSize() const { return m_client->deviceViewportSize(); }
    int viewportWidth() const { return viewportSize().width(); }
    int viewportHeight() const { return viewportSize().height(); }

    virtual void viewportChanged() { }
    virtual void receiveCompositorFrameAck(const CompositorFrameAck&) { }

    virtual void decideRenderPassAllocationsForFrame(const RenderPassList&) { }
    virtual bool haveCachedResourcesForRenderPassId(RenderPass::Id) const;

    // This passes ownership of the render passes to the renderer. It should
    // consume them, and empty the list.
    virtual void drawFrame(RenderPassList&) = 0;

    // waits for rendering to finish
    virtual void finish() = 0;

    virtual void doNoOp() { }
    // puts backbuffer onscreen
    virtual bool swapBuffers() = 0;

    virtual void getFramebufferPixels(void *pixels, const gfx::Rect&) = 0;

    virtual bool isContextLost();

    virtual void setVisible(bool) = 0;

    virtual void sendManagedMemoryStats(size_t bytesVisible, size_t bytesVisibleAndNearby, size_t bytesAllocated) = 0;

protected:
    explicit Renderer(RendererClient* client)
        : m_client(client)
    {
    }

    RendererClient* m_client;

    DISALLOW_COPY_AND_ASSIGN(Renderer);
};

}

#endif  // CC_RENDERER_H_
