// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/image_layer.h"

#include "CCLayerTreeHost.h"
#include "base/compiler_specific.h"
#include "cc/layer_texture_updater.h"
#include "cc/texture_update_queue.h"

namespace cc {

class ImageLayerTextureUpdater : public LayerTextureUpdater {
public:
    class Texture : public LayerTextureUpdater::Texture {
    public:
        Texture(ImageLayerTextureUpdater* textureUpdater, scoped_ptr<CCPrioritizedTexture> texture)
            : LayerTextureUpdater::Texture(texture.Pass())
            , m_textureUpdater(textureUpdater)
        {
        }

        virtual void update(CCTextureUpdateQueue& queue, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, CCRenderingStats&) OVERRIDE
        {
            textureUpdater()->updateTexture(queue, texture(), sourceRect, destOffset, partialUpdate);
        }

    private:
        ImageLayerTextureUpdater* textureUpdater() { return m_textureUpdater; }

        ImageLayerTextureUpdater* m_textureUpdater;
    };

    static scoped_refptr<ImageLayerTextureUpdater> create()
    {
        return make_scoped_refptr(new ImageLayerTextureUpdater());
    }

    virtual scoped_ptr<LayerTextureUpdater::Texture> createTexture(
        CCPrioritizedTextureManager* manager) OVERRIDE
    {
        return scoped_ptr<LayerTextureUpdater::Texture>(new Texture(this, CCPrioritizedTexture::create(manager)));
    }

    void updateTexture(CCTextureUpdateQueue& queue, CCPrioritizedTexture* texture, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate)
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
    ImageLayerTextureUpdater() { }
    virtual ~ImageLayerTextureUpdater() { }

    SkBitmap m_bitmap;
};

scoped_refptr<ImageLayerChromium> ImageLayerChromium::create()
{
    return make_scoped_refptr(new ImageLayerChromium());
}

ImageLayerChromium::ImageLayerChromium()
    : TiledLayerChromium()
{
}

ImageLayerChromium::~ImageLayerChromium()
{
}

void ImageLayerChromium::setBitmap(const SkBitmap& bitmap)
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

void ImageLayerChromium::setTexturePriorities(const CCPriorityCalculator& priorityCalc)
{
    // Update the tile data before creating all the layer's tiles.
    updateTileSizeAndTilingOption();

    TiledLayerChromium::setTexturePriorities(priorityCalc);
}

void ImageLayerChromium::update(CCTextureUpdateQueue& queue, const CCOcclusionTracker* occlusion, CCRenderingStats& stats)
{
    createTextureUpdaterIfNeeded();
    if (m_needsDisplay) {
        m_textureUpdater->setBitmap(m_bitmap);
        updateTileSizeAndTilingOption();
        invalidateContentRect(IntRect(IntPoint(), contentBounds()));
        m_needsDisplay = false;
    }
    TiledLayerChromium::update(queue, occlusion, stats);
}

void ImageLayerChromium::createTextureUpdaterIfNeeded()
{
    if (m_textureUpdater)
        return;

    m_textureUpdater = ImageLayerTextureUpdater::create();
    GLenum textureFormat = layerTreeHost()->rendererCapabilities().bestTextureFormat;
    setTextureFormat(textureFormat);
}

LayerTextureUpdater* ImageLayerChromium::textureUpdater() const
{
    return m_textureUpdater.get();
}

IntSize ImageLayerChromium::contentBounds() const
{
    return IntSize(m_bitmap.width(), m_bitmap.height());
}

bool ImageLayerChromium::drawsContent() const
{
    return !m_bitmap.isNull() && TiledLayerChromium::drawsContent();
}

bool ImageLayerChromium::needsContentsScale() const
{
    // Contents scale is not need for image layer because this can be done in compositor more efficiently.
    return false;
}

}
