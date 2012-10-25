// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BitmapSkPictureCanvasLayerUpdater_h
#define BitmapSkPictureCanvasLayerUpdater_h

#include "cc/skpicture_canvas_layer_updater.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

// This class records the contentRect into an SkPicture, then software rasterizes
// the SkPicture into bitmaps for each tile. This implements Settings::perTilePainting.
class BitmapSkPictureCanvasLayerUpdater : public SkPictureCanvasLayerUpdater {
public:
    class Resource : public CanvasLayerUpdater::Resource {
    public:
        Resource(BitmapSkPictureCanvasLayerUpdater*, scoped_ptr<PrioritizedTexture>);

        virtual void update(TextureUpdateQueue&, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats&) OVERRIDE;

    private:
        BitmapSkPictureCanvasLayerUpdater* updater() { return m_updater; }

        SkBitmap m_bitmap;
        BitmapSkPictureCanvasLayerUpdater* m_updater;
    };

    static scoped_refptr<BitmapSkPictureCanvasLayerUpdater> create(scoped_ptr<LayerPainter>);

    virtual scoped_ptr<LayerUpdater::Resource> createResource(PrioritizedTextureManager*) OVERRIDE;
    void paintContentsRect(SkCanvas*, const IntRect& sourceRect, RenderingStats&);

private:
    explicit BitmapSkPictureCanvasLayerUpdater(scoped_ptr<LayerPainter>);
    virtual ~BitmapSkPictureCanvasLayerUpdater();
};

}  // namespace cc

#endif  // BitmapSkPictureCanvasLayerUpdater_h
