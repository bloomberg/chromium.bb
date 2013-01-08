// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/image_layer_updater.h"
#include "cc/prioritized_resource.h"
#include "cc/resource_update_queue.h"

namespace cc {

ImageLayerUpdater::Resource::Resource(ImageLayerUpdater* updater, scoped_ptr<PrioritizedResource> texture)
    : LayerUpdater::Resource(texture.Pass())
    , m_updater(updater)
{
}

ImageLayerUpdater::Resource::~Resource()
{
}

void ImageLayerUpdater::Resource::update(ResourceUpdateQueue& queue, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate, RenderingStats&)
{
    m_updater->updateTexture(queue, texture(), sourceRect, destOffset, partialUpdate);
}

// static
scoped_refptr<ImageLayerUpdater> ImageLayerUpdater::create()
{
    return make_scoped_refptr(new ImageLayerUpdater());
}

scoped_ptr<LayerUpdater::Resource> ImageLayerUpdater::createResource(
    PrioritizedResourceManager* manager)
{
    return scoped_ptr<LayerUpdater::Resource>(new Resource(this, PrioritizedResource::create(manager)));
}

void ImageLayerUpdater::updateTexture(ResourceUpdateQueue& queue, PrioritizedResource* texture, const gfx::Rect& sourceRect, const gfx::Vector2d& destOffset, bool partialUpdate)
{
    // Source rect should never go outside the image pixels, even if this
    // is requested because the texture extends outside the image.
    gfx::Rect clippedSourceRect = sourceRect;
    gfx::Rect imageRect = gfx::Rect(0, 0, m_bitmap.width(), m_bitmap.height());
    clippedSourceRect.Intersect(imageRect);

    gfx::Vector2d clippedDestOffset = destOffset + gfx::Vector2d(clippedSourceRect.origin() - sourceRect.origin());

    ResourceUpdate upload = ResourceUpdate::Create(texture,
                                                   &m_bitmap,
                                                   imageRect,
                                                   clippedSourceRect,
                                                   clippedDestOffset);
    if (partialUpdate)
        queue.appendPartialUpload(upload);
    else
        queue.appendFullUpload(upload);
}

void ImageLayerUpdater::setBitmap(const SkBitmap& bitmap)
{
    m_bitmap = bitmap;
}

}
