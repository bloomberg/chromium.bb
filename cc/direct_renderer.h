// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCDirectRenderer_h
#define CCDirectRenderer_h

#include "base/basictypes.h"
#include "cc/renderer.h"
#include "cc/resource_provider.h"
#include "cc/scoped_texture.h"

namespace cc {

class ResourceProvider;

// This is the base class for code shared between the GL and software
// renderer implementations.  "Direct" refers to the fact that it does not
// delegate rendering to another compositor.
class DirectRenderer : public Renderer {
public:
    virtual ~DirectRenderer();

    ResourceProvider* resourceProvider() const { return m_resourceProvider; }

    virtual void decideRenderPassAllocationsForFrame(const RenderPassList& renderPassesInDrawOrder) OVERRIDE;
    virtual bool haveCachedResourcesForRenderPassId(RenderPass::Id) const OVERRIDE;
    virtual void drawFrame(const RenderPassList& renderPassesInDrawOrder, const RenderPassIdHashMap& renderPassesById) OVERRIDE;

protected:
    DirectRenderer(RendererClient* client, ResourceProvider* resourceProvider);

    struct DrawingFrame {
        DrawingFrame();
        ~DrawingFrame();

        const RenderPassIdHashMap* renderPassesById;
        const RenderPass* rootRenderPass;
        const RenderPass* currentRenderPass;
        const ScopedTexture* currentTexture;

        gfx::RectF rootDamageRect;

        WebKit::WebTransformationMatrix projectionMatrix;
        WebKit::WebTransformationMatrix windowMatrix;
        bool flippedY;
        gfx::RectF scissorRectInRenderPassSpace;
    };

    class CachedTexture : public ScopedTexture {
    public:
        static scoped_ptr<CachedTexture> create(ResourceProvider* resourceProvider) {
          return make_scoped_ptr(new CachedTexture(resourceProvider));
        }
        virtual ~CachedTexture() {}

        bool isComplete() const { return m_isComplete; }
        void setIsComplete(bool isComplete) { m_isComplete = isComplete; }

    protected:
        explicit CachedTexture(ResourceProvider* resourceProvider)
            : ScopedTexture(resourceProvider)
            , m_isComplete(false)
        {
        }

    private:
        bool m_isComplete;

        DISALLOW_COPY_AND_ASSIGN(CachedTexture);
    };

    static FloatRect quadVertexRect();
    static void quadRectTransform(WebKit::WebTransformationMatrix* quadRectTransform, const WebKit::WebTransformationMatrix& quadTransform, const gfx::RectF& quadRect);
    static void initializeMatrices(DrawingFrame&, const gfx::Rect& drawRect, bool flipY);
    static gfx::Rect moveScissorToWindowSpace(const DrawingFrame&, gfx::RectF scissorRect);

    bool haveCachedResources(RenderPass::Id) const;
    static IntSize renderPassTextureSize(const RenderPass*);
    static GLenum renderPassTextureFormat(const RenderPass*);

    void drawRenderPass(DrawingFrame&, const RenderPass*);
    bool useRenderPass(DrawingFrame&, const RenderPass*);

    virtual void bindFramebufferToOutputSurface(DrawingFrame&) = 0;
    virtual bool bindFramebufferToTexture(DrawingFrame&, const ScopedTexture*, const gfx::Rect& framebufferRect) = 0;
    virtual void setDrawViewportSize(const gfx::Size&) = 0;
    virtual void enableScissorTestRect(const gfx::Rect& scissorRect) = 0;
    virtual void disableScissorTest() = 0;
    virtual void clearFramebuffer(DrawingFrame&) = 0;
    virtual void drawQuad(DrawingFrame&, const DrawQuad*) = 0;
    virtual void beginDrawingFrame(DrawingFrame&) = 0;
    virtual void finishDrawingFrame(DrawingFrame&) = 0;
    virtual bool flippedFramebuffer() const = 0;

    ScopedPtrHashMap<RenderPass::Id, CachedTexture> m_renderPassTextures;
    ResourceProvider* m_resourceProvider;

private:
    DISALLOW_COPY_AND_ASSIGN(DirectRenderer);
};

}  // namespace cc

#endif  // CCDirectRenderer_h
