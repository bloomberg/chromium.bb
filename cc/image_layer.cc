// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/image_layer.h"

#include "base/compiler_specific.h"
#include "cc/layer_updater.h"
#include "cc/layer_tree_host.h"
#include "cc/texture_update_queue.h"

namespace cc {

class ImageLayerUpdater : public LayerUpdater {
public:
    class Resource : public LayerUpdater::Resource {
    public:
        Resource(ImageLayerUpdater* updater, scoped_ptr<PrioritizedTexture> texture)
            : LayerUpdater::Resource(texture.Pass())
            , m_updater(updater)
        {
        }

        virtual void update(TextureUpdateQueue& queue, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats&) OVERRIDE
        {
            updater()->updateTexture(queue, texture(), sourceRect, destOffset, partialUpdate);
        }

    private:
        ImageLayerUpdater* updater() { return m_updater; }

        ImageLayerUpdater* m_updater;
    };

    static scoped_refptr<ImageLayerUpdater> create()
    {
        return make_scoped_refptr(new ImageLayerUpdater());
    }

    virtual scoped_ptr<LayerUpdater::Resource> createResource(
        PrioritizedTextureManager* manager) OVERRIDE
    {
        return scoped_ptr<LayerUpdater::Resource>(new Resource(this, PrioritizedTexture::create(manager)));
    }

    void updateTexture(TextureUpdateQueue& queue, PrioritizedTexture* texture, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate)
    {
        // Source rect should never go outside the image pixels, even if this
        // is requested because the texture extends outside the image.
        IntRect clippedSourceRect = sourceRect;
        IntRect imageRect = IntRect(0, 0, m_bitmap.width(), m_bitmap.height());
        clippedSourceRect.intersect(imageRect);

        IntSize clippedDestOffset = destOffset + IntSize(clippedSourceRect.location() - sourceRect.location());

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

    void setBitmap(const SkBitmap& bitmap)
    {
        m_bitmap = bitmap;
    }

private:
    ImageLayerUpdater() { }
    virtual ~ImageLayerUpdater() { }

    SkBitmap m_bitmap;
};

scoped_refptr<ImageLayer> ImageLayer::create()
{
    return make_scoped_refptr(new ImageLayer());
}

ImageLayer::ImageLayer()
    : TiledLayer()
{
}

ImageLayer::~ImageLayer()
{
}

void ImageLayer::setBitmap(const SkBitmap& bitmap)
{
    // setBitmap() currently gets called whenever there is any
    // style change that affects the layer even if that change doesn't
    // affect the actual contents of the image (e.g. a CSS animation).
    // With this check in place we avoid unecessary texture uploads.
    if (bitmap.pixelRef() && bitmap.pixelRef() == m_bitmap.pixelRef())
        return;

    m_bitmap = bitmap;
    setNeedsDisplay();
}

void ImageLayer::setTexturePriorities(const PriorityCalculator& priorityCalc)
{
    // Update the tile data before creating all the layer's tiles.
    updateTileSizeAndTilingOption();

    TiledLayer::setTexturePriorities(priorityCalc);
}

void ImageLayer::update(TextureUpdateQueue& queue, const OcclusionTracker* occlusion, RenderingStats& stats)
{
    createUpdaterIfNeeded();
    if (m_needsDisplay) {
        m_updater->setBitmap(m_bitmap);
        updateTileSizeAndTilingOption();
        invalidateContentRect(IntRect(IntPoint(), contentBounds()));
        m_needsDisplay = false;
    }
    TiledLayer::update(queue, occlusion, stats);
}

void ImageLayer::createUpdaterIfNeeded()
{
    if (m_updater)
        return;

    m_updater = ImageLayerUpdater::create();
    GLenum textureFormat = layerTreeHost()->rendererCapabilities().bestTextureFormat;
    setTextureFormat(textureFormat);
}

LayerUpdater* ImageLayer::updater() const
{
    return m_updater.get();
}

IntSize ImageLayer::contentBounds() const
{
    return IntSize(m_bitmap.width(), m_bitmap.height());
}

bool ImageLayer::drawsContent() const
{
    return !m_bitmap.isNull() && TiledLayer::drawsContent();
}

bool ImageLayer::needsContentsScale() const
{
    // Contents scale is not need for image layer because this can be done in compositor more efficiently.
    return false;
}

}
