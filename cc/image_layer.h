// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IMAGE_LAYER_H_
#define CC_IMAGE_LAYER_H_

#include "cc/cc_export.h"
#include "cc/content_layer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

class ImageLayerUpdater;

// A Layer that contains only an Image element.
class CC_EXPORT ImageLayer : public TiledLayer {
public:
    static scoped_refptr<ImageLayer> create();

    virtual bool drawsContent() const OVERRIDE;
    virtual void setTexturePriorities(const PriorityCalculator&) OVERRIDE;
    virtual void update(ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats*) OVERRIDE;
    virtual void calculateContentsScale(
        float idealContentsScale,
        bool animating_transform_to_screen,
        float* contentsScaleX,
        float* contentsScaleY,
        gfx::Size* contentBounds) OVERRIDE;

    void setBitmap(const SkBitmap& image);

private:
    ImageLayer();
    virtual ~ImageLayer();

    void setTilingOption(TilingOption);

    virtual LayerUpdater* updater() const OVERRIDE;
    virtual void createUpdaterIfNeeded() OVERRIDE;
    float imageContentsScaleX() const;
    float imageContentsScaleY() const;

    SkBitmap m_bitmap;

    scoped_refptr<ImageLayerUpdater> m_updater;
};

}  // namespace cc

#endif  // CC_IMAGE_LAYER_H_
