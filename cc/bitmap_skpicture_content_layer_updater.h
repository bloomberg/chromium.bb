// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BITMAP_SKPICTURE_CONTENT_LAYER_UPDATER_H_
#define CC_BITMAP_SKPICTURE_CONTENT_LAYER_UPDATER_H_

#include "cc/skpicture_content_layer_updater.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

// This class records the contentRect into an SkPicture, then software rasterizes
// the SkPicture into bitmaps for each tile. This implements Settings::perTilePainting.
class BitmapSkPictureContentLayerUpdater : public SkPictureContentLayerUpdater {
public:
    class Resource : public ContentLayerUpdater::Resource {
    public:
        Resource(BitmapSkPictureContentLayerUpdater*, scoped_ptr<PrioritizedResource>);

        virtual void Update(ResourceUpdateQueue* queue, gfx::Rect sourceRect, gfx::Vector2d destOffset, bool partialUpdate, RenderingStats* stats) OVERRIDE;

    private:
        BitmapSkPictureContentLayerUpdater* updater() { return m_updater; }

        SkBitmap m_bitmap;
        BitmapSkPictureContentLayerUpdater* m_updater;
    };

    static scoped_refptr<BitmapSkPictureContentLayerUpdater> create(scoped_ptr<LayerPainter>);

    virtual scoped_ptr<LayerUpdater::Resource> CreateResource(PrioritizedResourceManager*) OVERRIDE;
    void paintContentsRect(SkCanvas*, const gfx::Rect& sourceRect, RenderingStats*);

private:
    explicit BitmapSkPictureContentLayerUpdater(scoped_ptr<LayerPainter>);
    virtual ~BitmapSkPictureContentLayerUpdater();
};

}  // namespace cc

#endif  // CC_BITMAP_SKPICTURE_CONTENT_LAYER_UPDATER_H_
