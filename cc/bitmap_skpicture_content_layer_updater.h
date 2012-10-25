// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BitmapSkPictureContentLayerUpdater_h
#define BitmapSkPictureContentLayerUpdater_h

#include "cc/skpicture_content_layer_updater.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

// This class records the contentRect into an SkPicture, then software rasterizes
// the SkPicture into bitmaps for each tile. This implements Settings::perTilePainting.
class BitmapSkPictureContentLayerUpdater : public SkPictureContentLayerUpdater {
public:
    class Resource : public ContentLayerUpdater::Resource {
    public:
        Resource(BitmapSkPictureContentLayerUpdater*, scoped_ptr<PrioritizedTexture>);

        virtual void update(TextureUpdateQueue&, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats&) OVERRIDE;

    private:
        BitmapSkPictureContentLayerUpdater* updater() { return m_updater; }

        SkBitmap m_bitmap;
        BitmapSkPictureContentLayerUpdater* m_updater;
    };

    static scoped_refptr<BitmapSkPictureContentLayerUpdater> create(scoped_ptr<LayerPainter>);

    virtual scoped_ptr<LayerUpdater::Resource> createResource(PrioritizedTextureManager*) OVERRIDE;
    void paintContentsRect(SkCanvas*, const IntRect& sourceRect, RenderingStats&);

private:
    explicit BitmapSkPictureContentLayerUpdater(scoped_ptr<LayerPainter>);
    virtual ~BitmapSkPictureContentLayerUpdater();
};

}  // namespace cc

#endif  // BitmapSkPictureContentLayerUpdater_h
