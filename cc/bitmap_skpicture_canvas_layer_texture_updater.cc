// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"

#include "BitmapSkPictureCanvasLayerTextureUpdater.h"

#include "CCRenderingStats.h"
#include "CCTextureUpdateQueue.h"
#include "LayerPainterChromium.h"
#include "PlatformColor.h"
#include "SkCanvas.h"
#include "SkDevice.h"
#include <wtf/CurrentTime.h>

namespace cc {

BitmapSkPictureCanvasLayerTextureUpdater::Texture::Texture(BitmapSkPictureCanvasLayerTextureUpdater* textureUpdater, scoped_ptr<CCPrioritizedTexture> texture)
    : CanvasLayerTextureUpdater::Texture(texture.Pass())
    , m_textureUpdater(textureUpdater)
{
}

void BitmapSkPictureCanvasLayerTextureUpdater::Texture::update(CCTextureUpdateQueue& queue, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, CCRenderingStats& stats)
{
    m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, sourceRect.width(), sourceRect.height());
    m_bitmap.allocPixels();
    m_bitmap.setIsOpaque(m_textureUpdater->layerIsOpaque());
    SkDevice device(m_bitmap);
    SkCanvas canvas(&device);
    double paintBeginTime = monotonicallyIncreasingTime();
    textureUpdater()->paintContentsRect(&canvas, sourceRect, stats);
    stats.totalPaintTimeInSeconds += monotonicallyIncreasingTime() - paintBeginTime;

    TextureUploader::Parameters upload = { texture(), &m_bitmap, NULL, { sourceRect, sourceRect, destOffset } };
    if (partialUpdate)
        queue.appendPartialUpload(upload);
    else
        queue.appendFullUpload(upload);
}

PassRefPtr<BitmapSkPictureCanvasLayerTextureUpdater> BitmapSkPictureCanvasLayerTextureUpdater::create(scoped_ptr<LayerPainterChromium> painter)
{
    return adoptRef(new BitmapSkPictureCanvasLayerTextureUpdater(painter.Pass()));
}

BitmapSkPictureCanvasLayerTextureUpdater::BitmapSkPictureCanvasLayerTextureUpdater(scoped_ptr<LayerPainterChromium> painter)
    : SkPictureCanvasLayerTextureUpdater(painter.Pass())
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
