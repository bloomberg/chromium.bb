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

class CCResourceProvider;

// This is the base class for code shared between the GL and software
// renderer implementations.  "Direct" refers to the fact that it does not
// delegate rendering to another compositor.
class CCDirectRenderer : public CCRenderer {
public:
    virtual ~CCDirectRenderer();

    CCResourceProvider* resourceProvider() const { return m_resourceProvider; }

    virtual void decideRenderPassAllocationsForFrame(const CCRenderPassList& renderPassesInDrawOrder) OVERRIDE;
    virtual bool haveCachedResourcesForRenderPassId(CCRenderPass::Id) const OVERRIDE;
    virtual void drawFrame(const CCRenderPassList& renderPassesInDrawOrder, const CCRenderPassIdHashMap& renderPassesById) OVERRIDE;

protected:
    CCDirectRenderer(CCRendererClient* client, CCResourceProvider* resourceProvider);

    struct DrawingFrame {
        DrawingFrame();
        ~DrawingFrame();

        const CCRenderPassIdHashMap* renderPassesById;
        const CCRenderPass* rootRenderPass;
        const CCRenderPass* currentRenderPass;
        const CCScopedTexture* currentTexture;

        gfx::RectF rootDamageRect;

        WebKit::WebTransformationMatrix projectionMatrix;
        WebKit::WebTransformationMatrix windowMatrix;
        bool flippedY;
        gfx::RectF scissorRectInRenderPassSpace;
    };

    class CachedTexture : public CCScopedTexture {
    public:
        static scoped_ptr<CachedTexture> create(CCResourceProvider* resourceProvider) {
          return make_scoped_ptr(new CachedTexture(resourceProvider));
        }
        virtual ~CachedTexture() {}

        bool isComplete() const { return m_isComplete; }
        void setIsComplete(bool isComplete) { m_isComplete = isComplete; }

    protected:
        explicit CachedTexture(CCResourceProvider* resourceProvider)
            : CCScopedTexture(resourceProvider)
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

    bool haveCachedResources(CCRenderPass::Id) const;
    static IntSize renderPassTextureSize(const CCRenderPass*);
    static GLenum renderPassTextureFormat(const CCRenderPass*);

    void drawRenderPass(DrawingFrame&, const CCRenderPass*);
    bool useRenderPass(DrawingFrame&, const CCRenderPass*);

    virtual void bindFramebufferToOutputSurface(DrawingFrame&) = 0;
    virtual bool bindFramebufferToTexture(DrawingFrame&, const CCScopedTexture*, const gfx::Rect& framebufferRect) = 0;
    virtual void setDrawViewportSize(const gfx::Size&) = 0;
    virtual void enableScissorTestRect(const gfx::Rect& scissorRect) = 0;
    virtual void disableScissorTest() = 0;
    virtual void clearFramebuffer(DrawingFrame&) = 0;
    virtual void drawQuad(DrawingFrame&, const CCDrawQuad*) = 0;
    virtual void beginDrawingFrame(DrawingFrame&) = 0;
    virtual void finishDrawingFrame(DrawingFrame&) = 0;
    virtual bool flippedFramebuffer() const = 0;

    ScopedPtrHashMap<CCRenderPass::Id, CachedTexture> m_renderPassTextures;
    CCResourceProvider* m_resourceProvider;

private:
    DISALLOW_COPY_AND_ASSIGN(CCDirectRenderer);
};

}  // namespace cc

#endif  // CCDirectRenderer_h
