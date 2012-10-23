// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef BitmapCanvasLayerUpdater_h
#define BitmapCanvasLayerUpdater_h

#include "cc/canvas_layer_updater.h"

class SkCanvas;

namespace cc {

class LayerPainter;

// This class rasterizes the contentRect into a skia bitmap canvas. It then updates
// textures by copying from the canvas into the texture, using MapSubImage if
// possible.
class BitmapCanvasLayerUpdater : public CanvasLayerUpdater {
public:
    class Texture : public LayerUpdater::Texture {
    public:
        Texture(BitmapCanvasLayerUpdater*, scoped_ptr<PrioritizedTexture>);
        virtual ~Texture();

        virtual void update(TextureUpdateQueue&, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats&) OVERRIDE;

    private:
        BitmapCanvasLayerUpdater* updater() { return m_updater; }

        BitmapCanvasLayerUpdater* m_updater;
    };

    static scoped_refptr<BitmapCanvasLayerUpdater> create(scoped_ptr<LayerPainter>);

    virtual scoped_ptr<LayerUpdater::Texture> createTexture(PrioritizedTextureManager*) OVERRIDE;
    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, RenderingStats&) OVERRIDE;
    void updateTexture(TextureUpdateQueue&, PrioritizedTexture*, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate);

    virtual void setOpaque(bool) OVERRIDE;

protected:
    explicit BitmapCanvasLayerUpdater(scoped_ptr<LayerPainter>);
    virtual ~BitmapCanvasLayerUpdater();

    scoped_ptr<SkCanvas> m_canvas;
    IntSize m_canvasSize;
    bool m_opaque;
};

}  // namespace cc

#endif  // BitmapCanvasLayerUpdater_h
