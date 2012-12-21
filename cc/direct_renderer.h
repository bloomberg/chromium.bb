// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DIRECT_RENDERER_H_
#define CC_DIRECT_RENDERER_H_

#include "base/basictypes.h"
#include "cc/cc_export.h"
#include "cc/renderer.h"
#include "cc/resource_provider.h"
#include "cc/scoped_resource.h"

namespace cc {

class ResourceProvider;

// This is the base class for code shared between the GL and software
// renderer implementations.  "Direct" refers to the fact that it does not
// delegate rendering to another compositor.
class CC_EXPORT DirectRenderer : public Renderer {
public:
    virtual ~DirectRenderer();

    ResourceProvider* resourceProvider() const { return m_resourceProvider; }

    virtual void decideRenderPassAllocationsForFrame(const RenderPassList& renderPassesInDrawOrder) OVERRIDE;
    virtual bool haveCachedResourcesForRenderPassId(RenderPass::Id) const OVERRIDE;
    virtual void drawFrame(RenderPassList& renderPassesInDrawOrder) OVERRIDE;

    struct CC_EXPORT DrawingFrame {
        DrawingFrame();
        ~DrawingFrame();

        const RenderPass* rootRenderPass;
        const RenderPass* currentRenderPass;
        const ScopedResource* currentTexture;

        gfx::RectF rootDamageRect;

        gfx::Transform projectionMatrix;
        gfx::Transform windowMatrix;
        bool flippedY;
    };

    void setEnlargePassTextureAmountForTesting(gfx::Vector2d amount);

protected:
    DirectRenderer(RendererClient* client, ResourceProvider* resourceProvider);

    class CachedResource : public ScopedResource {
    public:
        static scoped_ptr<CachedResource> create(ResourceProvider* resourceProvider) {
          return make_scoped_ptr(new CachedResource(resourceProvider));
        }
        virtual ~CachedResource() {}

        bool isComplete() const { return m_isComplete; }
        void setIsComplete(bool isComplete) { m_isComplete = isComplete; }

    protected:
        explicit CachedResource(ResourceProvider* resourceProvider)
            : ScopedResource(resourceProvider)
            , m_isComplete(false)
        {
        }

    private:
        bool m_isComplete;

        DISALLOW_COPY_AND_ASSIGN(CachedResource);
    };

    static gfx::RectF quadVertexRect();
    static void quadRectTransform(gfx::Transform* quadRectTransform, const gfx::Transform& quadTransform, const gfx::RectF& quadRect);
    static void initializeMatrices(DrawingFrame&, const gfx::Rect& drawRect, bool flipY);
    static gfx::Rect moveScissorToWindowSpace(const DrawingFrame&, gfx::RectF scissorRect);
    static gfx::RectF computeScissorRectForRenderPass(const DrawingFrame& frame);
    void setScissorStateForQuad(const DrawingFrame& frame, const DrawQuad& quad);
    void setScissorStateForQuadWithRenderPassScissor(const DrawingFrame& frame, const DrawQuad& quad, const gfx::RectF& renderPassScissor, bool* shouldSkipQuad);

    bool haveCachedResources(RenderPass::Id) const;
    static gfx::Size renderPassTextureSize(const RenderPass*);
    static GLenum renderPassTextureFormat(const RenderPass*);

    void drawRenderPass(DrawingFrame&, const RenderPass*);
    bool useRenderPass(DrawingFrame&, const RenderPass*);

    virtual void bindFramebufferToOutputSurface(DrawingFrame&) = 0;
    virtual bool bindFramebufferToTexture(DrawingFrame&, const ScopedResource*, const gfx::Rect& framebufferRect) = 0;
    virtual void setDrawViewportSize(const gfx::Size&) = 0;
    virtual void setScissorTestRect(const gfx::Rect& scissorRect) = 0;
    virtual void clearFramebuffer(DrawingFrame&) = 0;
    virtual void drawQuad(DrawingFrame&, const DrawQuad*) = 0;
    virtual void beginDrawingFrame(DrawingFrame&) = 0;
    virtual void finishDrawingFrame(DrawingFrame&) = 0;
    virtual void finishDrawingQuadList();
    virtual bool flippedFramebuffer() const = 0;
    virtual void ensureScissorTestEnabled() = 0;
    virtual void ensureScissorTestDisabled() = 0;

    ScopedPtrHashMap<RenderPass::Id, CachedResource> m_renderPassTextures;
    ResourceProvider* m_resourceProvider;

private:

    gfx::Vector2d m_enlargePassTextureAmount;

    DISALLOW_COPY_AND_ASSIGN(DirectRenderer);
};

}  // namespace cc

#endif  // CC_DIRECT_RENDERER_H_
