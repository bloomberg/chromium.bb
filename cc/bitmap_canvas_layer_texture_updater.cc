// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"

#include "BitmapCanvasLayerTextureUpdater.h"

#include "CCTextureUpdateQueue.h"
#include "LayerPainterChromium.h"
#include "PlatformColor.h"
#include "skia/ext/platform_canvas.h"

namespace cc {

BitmapCanvasLayerTextureUpdater::Texture::Texture(BitmapCanvasLayerTextureUpdater* textureUpdater, scoped_ptr<CCPrioritizedTexture> texture)
    : LayerTextureUpdater::Texture(texture.Pass())
    , m_textureUpdater(textureUpdater)
{
}

BitmapCanvasLayerTextureUpdater::Texture::~Texture()
{
}

void BitmapCanvasLayerTextureUpdater::Texture::update(CCTextureUpdateQueue& queue, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, CCRenderingStats&)
{
    textureUpdater()->updateTexture(queue, texture(), sourceRect, destOffset, partialUpdate);
}

PassRefPtr<BitmapCanvasLayerTextureUpdater> BitmapCanvasLayerTextureUpdater::create(scoped_ptr<LayerPainterChromium> painter)
{
    return adoptRef(new BitmapCanvasLayerTextureUpdater(painter.Pass()));
}

BitmapCanvasLayerTextureUpdater::BitmapCanvasLayerTextureUpdater(scoped_ptr<LayerPainterChromium> painter)
    : CanvasLayerTextureUpdater(painter.Pass())
    , m_opaque(false)
{
}

BitmapCanvasLayerTextureUpdater::~BitmapCanvasLayerTextureUpdater()
{
}

PassOwnPtr<LayerTextureUpdater::Texture> BitmapCanvasLayerTextureUpdater::createTexture(CCPrioritizedTextureManager* manager)
{
    return adoptPtr(new Texture(this, CCPrioritizedTexture::create(manager)));
}

LayerTextureUpdater::SampledTexelFormat BitmapCanvasLayerTextureUpdater::sampledTexelFormat(GC3Denum textureFormat)
{
    // The component order may be bgra if we uploaded bgra pixels to rgba textures.
    return PlatformColor::sameComponentOrder(textureFormat) ?
            LayerTextureUpdater::SampledTexelFormatRGBA : LayerTextureUpdater::SampledTexelFormatBGRA;
}

void BitmapCanvasLayerTextureUpdater::prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, CCRenderingStats& stats)
{
    if (m_canvasSize != contentRect.size()) {
        m_canvasSize = contentRect.size();
        m_canvas = adoptPtr(skia::CreateBitmapCanvas(m_canvasSize.width(), m_canvasSize.height(), m_opaque));
    }

    paintContents(m_canvas.get(), contentRect, contentsWidthScale, contentsHeightScale, resultingOpaqueRect, stats);
}

void BitmapCanvasLayerTextureUpdater::updateTexture(CCTextureUpdateQueue& queue, CCPrioritizedTexture* texture, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate)
{
    TextureUploader::Parameters upload = { texture, &m_canvas->getDevice()->accessBitmap(false), NULL, { contentRect(), sourceRect, destOffset } };
    if (partialUpdate)
        queue.appendPartialUpload(upload);
    else
        queue.appendFullUpload(upload);
}

void BitmapCanvasLayerTextureUpdater::setOpaque(bool opaque)
{
    if (opaque != m_opaque) {
        m_canvas.clear();
        m_canvasSize = IntSize();
    }
    m_opaque = opaque;
}

} // namespace cc
