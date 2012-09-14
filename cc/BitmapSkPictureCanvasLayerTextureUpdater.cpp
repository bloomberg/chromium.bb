// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "BitmapSkPictureCanvasLayerTextureUpdater.h"

#include "CCRenderingStats.h"
#include "LayerPainterChromium.h"
#include "PlatformColor.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include <wtf/CurrentTime.h>

namespace cc {

BitmapSkPictureCanvasLayerTextureUpdater::Texture::Texture(BitmapSkPictureCanvasLayerTextureUpdater* textureUpdater, PassOwnPtr<CCPrioritizedTexture> texture)
    : CanvasLayerTextureUpdater::Texture(texture)
    , m_textureUpdater(textureUpdater)
{
}

void BitmapSkPictureCanvasLayerTextureUpdater::Texture::prepareRect(const IntRect& sourceRect, CCRenderingStats& stats)
{
    m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, sourceRect.width(), sourceRect.height());
    m_bitmap.allocPixels();
    m_bitmap.setIsOpaque(m_textureUpdater->layerIsOpaque());
    SkDevice device(m_bitmap);
    SkCanvas canvas(&device);
    double paintBeginTime = monotonicallyIncreasingTime();
    textureUpdater()->paintContentsRect(&canvas, sourceRect, stats);
    stats.totalPaintTimeInSeconds += monotonicallyIncreasingTime() - paintBeginTime;
}

void BitmapSkPictureCanvasLayerTextureUpdater::Texture::updateRect(CCResourceProvider* resourceProvider, const IntRect& sourceRect, const IntSize& destOffset)
{
    m_bitmap.lockPixels();
    texture()->upload(resourceProvider, static_cast<uint8_t*>(m_bitmap.getPixels()), sourceRect, sourceRect, destOffset);
    m_bitmap.unlockPixels();
    m_bitmap.reset();
}

PassRefPtr<BitmapSkPictureCanvasLayerTextureUpdater> BitmapSkPictureCanvasLayerTextureUpdater::create(PassOwnPtr<LayerPainterChromium> painter)
{
    return adoptRef(new BitmapSkPictureCanvasLayerTextureUpdater(painter));
}

BitmapSkPictureCanvasLayerTextureUpdater::BitmapSkPictureCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium> painter)
    : SkPictureCanvasLayerTextureUpdater(painter)
{
}

BitmapSkPictureCanvasLayerTextureUpdater::~BitmapSkPictureCanvasLayerTextureUpdater()
{
}

PassOwnPtr<LayerTextureUpdater::Texture> BitmapSkPictureCanvasLayerTextureUpdater::createTexture(CCPrioritizedTextureManager* manager)
{
    return adoptPtr(new Texture(this, CCPrioritizedTexture::create(manager)));
}

LayerTextureUpdater::SampledTexelFormat BitmapSkPictureCanvasLayerTextureUpdater::sampledTexelFormat(GC3Denum textureFormat)
{
    // The component order may be bgra if we uploaded bgra pixels to rgba textures.
    return PlatformColor::sameComponentOrder(textureFormat) ?
            LayerTextureUpdater::SampledTexelFormatRGBA : LayerTextureUpdater::SampledTexelFormatBGRA;
}

void BitmapSkPictureCanvasLayerTextureUpdater::paintContentsRect(SkCanvas* canvas, const IntRect& sourceRect, CCRenderingStats& stats)
{
    // Translate the origin of contentRect to that of sourceRect.
    canvas->translate(contentRect().x() - sourceRect.x(),
                      contentRect().y() - sourceRect.y());
    double rasterizeBeginTime = monotonicallyIncreasingTime();
    drawPicture(canvas);
    stats.totalRasterizeTimeInSeconds += monotonicallyIncreasingTime() - rasterizeBeginTime;
}

} // namespace cc
#endif // USE(ACCELERATED_COMPOSITING)
