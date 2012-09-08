// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCDirectRenderer_h
#define CCDirectRenderer_h

#include "CCRenderer.h"
#include "CCResourceProvider.h"
#include "CCScopedTexture.h"

namespace WebCore {

class CCResourceProvider;

// This is the base class for code shared between the GL and software
// renderer implementations.  "Direct" refers to the fact that it does not
// delegate rendering to another compositor.
class CCDirectRenderer : public CCRenderer {
    WTF_MAKE_NONCOPYABLE(CCDirectRenderer);
public:
    virtual ~CCDirectRenderer() { }

    CCResourceProvider* resourceProvider() const { return m_resourceProvider; }

    virtual void decideRenderPassAllocationsForFrame(const CCRenderPassList& renderPassesInDrawOrder) OVERRIDE;
    virtual bool haveCachedResourcesForRenderPassId(CCRenderPass::Id) const OVERRIDE;
    virtual void drawFrame(const CCRenderPassList& renderPassesInDrawOrder, const CCRenderPassIdHashMap& renderPassesById) OVERRIDE;

protected:
    CCDirectRenderer(CCRendererClient* client, CCResourceProvider* resourceProvider)
        : CCRenderer(client)
        , m_resourceProvider(resourceProvider)
    {
    }

    struct DrawingFrame {
        const CCRenderPassIdHashMap* renderPassesById;
        const CCRenderPass* rootRenderPass;
        const CCRenderPass* currentRenderPass;
        const CCScopedTexture* currentTexture;

        FloatRect rootDamageRect;

        WebKit::WebTransformationMatrix projectionMatrix;
        WebKit::WebTransformationMatrix windowMatrix;
        bool flippedY;
        FloatRect scissorRectInRenderPassSpace;

        DrawingFrame()
            : rootRenderPass(0)
            , currentRenderPass(0)
            , currentTexture(0)
            , flippedY(false)
        { }
    };

    class CachedTexture : public CCScopedTexture {
        WTF_MAKE_NONCOPYABLE(CachedTexture);
    public:
        static PassOwnPtr<CachedTexture> create(CCResourceProvider* resourceProvider) { return adoptPtr(new CachedTexture(resourceProvider)); }
        virtual ~CachedTexture() { }

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
    };

    static FloatRect quadVertexRect();
    static void quadRectTransform(WebKit::WebTransformationMatrix* quadRectTransform, const WebKit::WebTransformationMatrix& quadTransform, const FloatRect& quadRect);
    static void initializeMatrices(DrawingFrame&, const IntRect& drawRect, bool flipY);
    static IntRect moveScissorToWindowSpace(const DrawingFrame&, FloatRect scissorRect);

    bool haveCachedResources(CCRenderPass::Id) const;
    static IntSize renderPassTextureSize(const CCRenderPass*);
    static GC3Denum renderPassTextureFormat(const CCRenderPass*);

    void drawRenderPass(DrawingFrame&, const CCRenderPass*);
    bool useRenderPass(DrawingFrame&, const CCRenderPass*);

    virtual void bindFramebufferToOutputSurface(DrawingFrame&) = 0;
    virtual bool bindFramebufferToTexture(DrawingFrame&, const CCScopedTexture*, const IntRect& framebufferRect) = 0;
    virtual void setDrawViewportSize(const IntSize&) = 0;
    virtual void enableScissorTestRect(const IntRect& scissorRect) = 0;
    virtual void disableScissorTest() = 0;
    virtual void clearFramebuffer(DrawingFrame&) = 0;
    virtual void drawQuad(DrawingFrame&, const CCDrawQuad*) = 0;
    virtual void beginDrawingFrame(DrawingFrame&) = 0;
    virtual void finishDrawingFrame(DrawingFrame&) = 0;

    HashMap<CCRenderPass::Id, OwnPtr<CachedTexture> > m_renderPassTextures;
    CCResourceProvider* m_resourceProvider;
};

}

#endif // CCDirectRenderer_h
