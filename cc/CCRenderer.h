// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCRenderer_h
#define CCRenderer_h

#include "CCLayerTreeHost.h"
#include "CCRenderPass.h"
#include "FloatQuad.h"
#include "IntRect.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>

namespace cc {

class CCScopedTexture;
class TextureCopier;
class TextureUploader;

enum TextureUploaderOption { ThrottledUploader, UnthrottledUploader };

class CCRendererClient {
public:
    virtual const IntSize& deviceViewportSize() const = 0;
    virtual const CCLayerTreeSettings& settings() const = 0;
    virtual void didLoseContext() = 0;
    virtual void onSwapBuffersComplete() = 0;
    virtual void releaseContentsTextures() = 0;
    virtual void setFullRootLayerDamage() = 0;
    virtual void setMemoryAllocationLimitBytes(size_t) = 0;
protected:
    virtual ~CCRendererClient() { }
};

class CCRenderer {
    WTF_MAKE_NONCOPYABLE(CCRenderer);
public:
    // This enum defines the various resource pools for the CCResourceProvider
    // where textures get allocated.
    enum ResourcePool {
      ImplPool = 1, // This pool is for textures that get allocated on the impl thread (e.g. RenderSurfaces).
      ContentPool // This pool is for textures that get allocated on the main thread (e.g. tiles).
    };

    virtual ~CCRenderer() { }

    virtual const RendererCapabilities& capabilities() const = 0;

    const CCLayerTreeSettings& settings() const { return m_client->settings(); }

    const IntSize& viewportSize() { return m_client->deviceViewportSize(); }
    int viewportWidth() { return viewportSize().width(); }
    int viewportHeight() { return viewportSize().height(); }

    virtual void viewportChanged() { }

    virtual void decideRenderPassAllocationsForFrame(const CCRenderPassList&) { }
    virtual bool haveCachedResourcesForRenderPassId(CCRenderPass::Id) const { return false; }

    virtual void drawFrame(const CCRenderPassList&, const CCRenderPassIdHashMap&) = 0;

    // waits for rendering to finish
    virtual void finish() = 0;

    virtual void doNoOp() { }
    // puts backbuffer onscreen
    virtual bool swapBuffers() = 0;

    virtual void getFramebufferPixels(void *pixels, const IntRect&) = 0;

    virtual TextureCopier* textureCopier() const = 0;
    virtual TextureUploader* textureUploader() const = 0;

    virtual bool isContextLost() { return false; }

    virtual void setVisible(bool) = 0;

protected:
    explicit CCRenderer(CCRendererClient* client)
        : m_client(client)
    {
    }

    CCRendererClient* m_client;
};

}

#endif // CCRenderer_h
