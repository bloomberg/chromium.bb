// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef BitmapCanvasLayerTextureUpdater_h
#define BitmapCanvasLayerTextureUpdater_h

#if USE(ACCELERATED_COMPOSITING)

#include "CanvasLayerTextureUpdater.h"

class SkCanvas;

namespace cc {

class LayerPainterChromium;

// This class rasterizes the contentRect into a skia bitmap canvas. It then updates
// textures by copying from the canvas into the texture, using MapSubImage if
// possible.
class BitmapCanvasLayerTextureUpdater : public CanvasLayerTextureUpdater {
public:
    class Texture : public LayerTextureUpdater::Texture {
    public:
        Texture(BitmapCanvasLayerTextureUpdater*, PassOwnPtr<CCPrioritizedTexture>);
        virtual ~Texture();

        virtual void updateRect(CCResourceProvider*, const IntRect& sourceRect, const IntSize& destOffset) OVERRIDE;

    private:
        BitmapCanvasLayerTextureUpdater* textureUpdater() { return m_textureUpdater; }

        BitmapCanvasLayerTextureUpdater* m_textureUpdater;
    };

    static PassRefPtr<BitmapCanvasLayerTextureUpdater> create(PassOwnPtr<LayerPainterChromium>);
    virtual ~BitmapCanvasLayerTextureUpdater();

    virtual PassOwnPtr<LayerTextureUpdater::Texture> createTexture(CCPrioritizedTextureManager*) OVERRIDE;
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum textureFormat) OVERRIDE;
    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, CCRenderingStats&) OVERRIDE;
    void updateTextureRect(CCResourceProvider*, CCPrioritizedTexture*, const IntRect& sourceRect, const IntSize& destOffset);

    virtual void setOpaque(bool) OVERRIDE;

private:
    explicit BitmapCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium>);

    OwnPtr<SkCanvas> m_canvas;
    IntSize m_canvasSize;
    bool m_opaque;
};

} // namespace cc
#endif // USE(ACCELERATED_COMPOSITING)
#endif // BitmapCanvasLayerTextureUpdater_h
