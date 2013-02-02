// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_NINE_PATCH_LAYER_H_
#define CC_NINE_PATCH_LAYER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/layer.h"
#include "cc/image_layer_updater.h"
#include "ui/gfx/rect.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

class ResourceUpdateQueue;

class CC_EXPORT NinePatchLayer : public Layer {
public:
    static scoped_refptr<NinePatchLayer> create();

    virtual bool drawsContent() const OVERRIDE;
    virtual void setTexturePriorities(const PriorityCalculator&) OVERRIDE;
    virtual void update(ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats*) OVERRIDE;
    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;

    // aperture is in the pixel space of the bitmap resource and refers to
    // the center patch of the ninepatch (which is unused in this
    // implementation). We split off eight rects surrounding it and stick them
    // on the edges of the layer. The corners are unscaled, the top and bottom
    // rects are x-stretched to fit, and the left and right rects are
    // y-stretched to fit.
    void setBitmap(const SkBitmap& bitmap, const gfx::Rect& aperture);

private:
    NinePatchLayer();
    virtual ~NinePatchLayer();
    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* treeImpl) OVERRIDE;

    void createUpdaterIfNeeded();
    void createResource();

    scoped_refptr<ImageLayerUpdater> m_updater;
    scoped_ptr<LayerUpdater::Resource> m_resource;

    SkBitmap m_bitmap;
    bool m_bitmapDirty;

    // The transparent center region that shows the parent layer's contents in image space.
    gfx::Rect m_imageAperture;
};

}  // namespace cc

#endif  // CC_NINE_PATCH_LAYER_H_
