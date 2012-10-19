// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BitmapSkPictureCanvasLayerTextureUpdater_h
#define BitmapSkPictureCanvasLayerTextureUpdater_h

#include "cc/skpicture_canvas_layer_texture_updater.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

// This class records the contentRect into an SkPicture, then software rasterizes
// the SkPicture into bitmaps for each tile. This implements Settings::perTilePainting.
class BitmapSkPictureCanvasLayerTextureUpdater : public SkPictureCanvasLayerTextureUpdater {
public:
    class Texture : public CanvasLayerTextureUpdater::Texture {
    public:
        Texture(BitmapSkPictureCanvasLayerTextureUpdater*, scoped_ptr<PrioritizedTexture>);

        virtual void update(TextureUpdateQueue&, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats&) OVERRIDE;

    private:
        BitmapSkPictureCanvasLayerTextureUpdater* textureUpdater() { return m_textureUpdater; }

        SkBitmap m_bitmap;
        BitmapSkPictureCanvasLayerTextureUpdater* m_textureUpdater;
    };

    static scoped_refptr<BitmapSkPictureCanvasLayerTextureUpdater> create(scoped_ptr<LayerPainter>);

    virtual scoped_ptr<LayerTextureUpdater::Texture> createTexture(PrioritizedTextureManager*) OVERRIDE;
    virtual SampledTexelFormat sampledTexelFormat(GLenum textureFormat) OVERRIDE;
    void paintContentsRect(SkCanvas*, const IntRect& sourceRect, RenderingStats&);

private:
    explicit BitmapSkPictureCanvasLayerTextureUpdater(scoped_ptr<LayerPainter>);
    virtual ~BitmapSkPictureCanvasLayerTextureUpdater();
};

}  // namespace cc

#endif  // BitmapSkPictureCanvasLayerTextureUpdater_h
