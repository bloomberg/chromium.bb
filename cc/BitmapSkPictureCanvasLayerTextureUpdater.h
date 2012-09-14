// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef BitmapSkPictureCanvasLayerTextureUpdater_h
#define BitmapSkPictureCanvasLayerTextureUpdater_h

#if USE(ACCELERATED_COMPOSITING)
#include "SkBitmap.h"
#include "SkPictureCanvasLayerTextureUpdater.h"

namespace cc {

// This class records the contentRect into an SkPicture, then software rasterizes
// the SkPicture into bitmaps for each tile. This implements CCSettings::perTilePainting.
class BitmapSkPictureCanvasLayerTextureUpdater : public SkPictureCanvasLayerTextureUpdater {
public:
    class Texture : public CanvasLayerTextureUpdater::Texture {
    public:
        Texture(BitmapSkPictureCanvasLayerTextureUpdater*, PassOwnPtr<CCPrioritizedTexture>);

        virtual void prepareRect(const IntRect& sourceRect, CCRenderingStats&) OVERRIDE;
        virtual void updateRect(CCResourceProvider*, const IntRect& sourceRect, const IntSize& destOffset) OVERRIDE;

    private:
        BitmapSkPictureCanvasLayerTextureUpdater* textureUpdater() { return m_textureUpdater; }

        SkBitmap m_bitmap;
        BitmapSkPictureCanvasLayerTextureUpdater* m_textureUpdater;
    };

    static PassRefPtr<BitmapSkPictureCanvasLayerTextureUpdater> create(PassOwnPtr<LayerPainterChromium>);
    virtual ~BitmapSkPictureCanvasLayerTextureUpdater();

    virtual PassOwnPtr<LayerTextureUpdater::Texture> createTexture(CCPrioritizedTextureManager*) OVERRIDE;
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum textureFormat) OVERRIDE;
    void paintContentsRect(SkCanvas*, const IntRect& sourceRect, CCRenderingStats&);

private:
    explicit BitmapSkPictureCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium>);
};
} // namespace cc
#endif // USE(ACCELERATED_COMPOSITING)
#endif // BitmapSkPictureCanvasLayerTextureUpdater_h
