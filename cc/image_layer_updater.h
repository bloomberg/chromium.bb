// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IMAGE_LAYER_UPDATER_H_
#define CC_IMAGE_LAYER_UPDATER_H_

#include "cc/layer_updater.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

class ResourceUpdateQueue;

class ImageLayerUpdater : public LayerUpdater {
public:
    class Resource : public LayerUpdater::Resource {
    public:
        Resource(ImageLayerUpdater* updater, scoped_ptr<PrioritizedResource> texture);
        virtual ~Resource();

        virtual void update(ResourceUpdateQueue&, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate, RenderingStats&) OVERRIDE;

    private:
        ImageLayerUpdater* m_updater;
    };

    static scoped_refptr<ImageLayerUpdater> create();

    virtual scoped_ptr<LayerUpdater::Resource> createResource(
        PrioritizedResourceManager*) OVERRIDE;

    void updateTexture(ResourceUpdateQueue&, PrioritizedResource*, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate);

    void setBitmap(const SkBitmap&);

private:
    ImageLayerUpdater() { }
    virtual ~ImageLayerUpdater() { }

    SkBitmap m_bitmap;
};

}  // namespace cc

#endif  // CC_IMAGE_LAYER_UPDATER_H_
