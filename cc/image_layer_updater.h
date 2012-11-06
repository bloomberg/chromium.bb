// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageLayerUpdater_h
#define ImageLayerUpdater_h

#include "config.h"

#include "cc/layer_updater.h"

namespace cc {

class ResourceUpdateQueue;

class ImageLayerUpdater : public LayerUpdater {
public:
    class Resource : public LayerUpdater::Resource {
    public:
        Resource(ImageLayerUpdater* updater, scoped_ptr<PrioritizedTexture> texture)
            : LayerUpdater::Resource(texture.Pass())
            , m_updater(updater)
        {
        }

        virtual void update(ResourceUpdateQueue&, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate, RenderingStats&) OVERRIDE;

    private:
        ImageLayerUpdater* m_updater;
    };

    static scoped_refptr<ImageLayerUpdater> create();

    virtual scoped_ptr<LayerUpdater::Resource> createResource(
        PrioritizedTextureManager*) OVERRIDE;

    void updateTexture(ResourceUpdateQueue&, PrioritizedTexture*, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate);

    void setBitmap(const SkBitmap&);

private:
    ImageLayerUpdater() { }
    virtual ~ImageLayerUpdater() { }

    SkBitmap m_bitmap;
};

}

#endif
