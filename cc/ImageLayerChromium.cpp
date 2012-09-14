// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "ImageLayerChromium.h"

#include "CCLayerTreeHost.h"
#include "LayerTextureUpdater.h"
#include "PlatformColor.h"

namespace cc {

class ImageLayerTextureUpdater : public LayerTextureUpdater {
public:
    class Texture : public LayerTextureUpdater::Texture {
    public:
        Texture(ImageLayerTextureUpdater* textureUpdater, PassOwnPtr<CCPrioritizedTexture> texture)
            : LayerTextureUpdater::Texture(texture)
            , m_textureUpdater(textureUpdater)
        {
        }

        virtual void updateRect(CCResourceProvider* resourceProvider, const IntRect& sourceRect, const IntSize& destOffset) OVERRIDE
        {
            textureUpdater()->updateTextureRect(resourceProvider, texture(), sourceRect, destOffset);
        }

    private:
        ImageLayerTextureUpdater* textureUpdater() { return m_textureUpdater; }

        ImageLayerTextureUpdater* m_textureUpdater;
    };

    static PassRefPtr<ImageLayerTextureUpdater> create()
    {
        return adoptRef(new ImageLayerTextureUpdater());
    }

    virtual ~ImageLayerTextureUpdater() { }

    virtual PassOwnPtr<LayerTextureUpdater::Texture> createTexture(CCPrioritizedTextureManager* manager)
    {
        return adoptPtr(new Texture(this, CCPrioritizedTexture::create(manager)));
    }

    virtual SampledTexelFormat sampledTexelFormat(GC3Denum textureFormat) OVERRIDE
    {
        return PlatformColor::sameComponentOrder(textureFormat) ?
                LayerTextureUpdater::SampledTexelFormatRGBA : LayerTextureUpdater::SampledTexelFormatBGRA;
    }

    void updateTextureRect(CCResourceProvider* resourceProvider, CCPrioritizedTexture* texture, const IntRect& sourceRect, const IntSize& destOffset)
    {
        // Source rect should never go outside the image pixels, even if this
        // is requested because the texture extends outside the image.
        IntRect clippedSourceRect = sourceRect;
        IntRect imageRect = IntRect(0, 0, m_bitmap.width(), m_bitmap.height());
        clippedSourceRect.intersect(imageRect);

        IntSize clippedDestOffset = destOffset + IntSize(clippedSourceRect.location() - sourceRect.location());

        SkAutoLockPixels lock(m_bitmap);
        texture->upload(resourceProvider, static_cast<const uint8_t*>(m_bitmap.getPixels()), imageRect, clippedSourceRect, clippedDestOffset);
    }

    void setBitmap(const SkBitmap& bitmap)
    {
        m_bitmap = bitmap;
    }

private:
    ImageLayerTextureUpdater() { }

    SkBitmap m_bitmap;
};

PassRefPtr<ImageLayerChromium> ImageLayerChromium::create()
{
    return adoptRef(new ImageLayerChromium());
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
    GC3Denum textureFormat = layerTreeHost()->rendererCapabilities().bestTextureFormat;
    setTextureFormat(textureFormat);
    setSampledTexelFormat(textureUpdater()->sampledTexelFormat(textureFormat));
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
#endif // USE(ACCELERATED_COMPOSITING)
