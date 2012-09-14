// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "BitmapCanvasLayerTextureUpdater.h"

#include "LayerPainterChromium.h"
#include "PlatformColor.h"
#include "skia/ext/platform_canvas.h"

namespace cc {

BitmapCanvasLayerTextureUpdater::Texture::Texture(BitmapCanvasLayerTextureUpdater* textureUpdater, PassOwnPtr<CCPrioritizedTexture> texture)
    : LayerTextureUpdater::Texture(texture)
    , m_textureUpdater(textureUpdater)
{
}

BitmapCanvasLayerTextureUpdater::Texture::~Texture()
{
}

void BitmapCanvasLayerTextureUpdater::Texture::updateRect(CCResourceProvider* resourceProvider, const IntRect& sourceRect, const IntSize& destOffset)
{
    textureUpdater()->updateTextureRect(resourceProvider, texture(), sourceRect, destOffset);
}

PassRefPtr<BitmapCanvasLayerTextureUpdater> BitmapCanvasLayerTextureUpdater::create(PassOwnPtr<LayerPainterChromium> painter)
{
    return adoptRef(new BitmapCanvasLayerTextureUpdater(painter));
}

BitmapCanvasLayerTextureUpdater::BitmapCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium> painter)
    : CanvasLayerTextureUpdater(painter)
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

void BitmapCanvasLayerTextureUpdater::updateTextureRect(CCResourceProvider* resourceProvider, CCPrioritizedTexture* texture, const IntRect& sourceRect, const IntSize& destOffset)
{
    const SkBitmap& bitmap = m_canvas->getDevice()->accessBitmap(false);
    bitmap.lockPixels();

    texture->upload(resourceProvider, static_cast<const uint8_t*>(bitmap.getPixels()), contentRect(), sourceRect, destOffset);
    bitmap.unlockPixels();
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
#endif // USE(ACCELERATED_COMPOSITING)
