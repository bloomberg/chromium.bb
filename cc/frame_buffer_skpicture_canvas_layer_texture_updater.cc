// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"

#include "FrameBufferSkPictureCanvasLayerTextureUpdater.h"

#include "LayerPainterChromium.h"

namespace cc {

FrameBufferSkPictureCanvasLayerTextureUpdater::Texture::Texture(FrameBufferSkPictureCanvasLayerTextureUpdater* textureUpdater, scoped_ptr<CCPrioritizedTexture> texture)
  : LayerTextureUpdater::Texture(texture.Pass())
    , m_textureUpdater(textureUpdater)
{
}

FrameBufferSkPictureCanvasLayerTextureUpdater::Texture::~Texture()
{
}

void FrameBufferSkPictureCanvasLayerTextureUpdater::Texture::update(CCTextureUpdateQueue& queue, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, CCRenderingStats&)
{
    textureUpdater()->updateTexture(queue, texture(), sourceRect, destOffset, partialUpdate);
}

PassRefPtr<FrameBufferSkPictureCanvasLayerTextureUpdater> FrameBufferSkPictureCanvasLayerTextureUpdater::create(PassOwnPtr<LayerPainterChromium> painter)
{
    return adoptRef(new FrameBufferSkPictureCanvasLayerTextureUpdater(painter));
}

FrameBufferSkPictureCanvasLayerTextureUpdater::FrameBufferSkPictureCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium> painter)
    : SkPictureCanvasLayerTextureUpdater(painter)
{
}

FrameBufferSkPictureCanvasLayerTextureUpdater::~FrameBufferSkPictureCanvasLayerTextureUpdater()
{
}

PassOwnPtr<LayerTextureUpdater::Texture> FrameBufferSkPictureCanvasLayerTextureUpdater::createTexture(CCPrioritizedTextureManager* manager)
{
    return adoptPtr(new Texture(this, CCPrioritizedTexture::create(manager)));
}

LayerTextureUpdater::SampledTexelFormat FrameBufferSkPictureCanvasLayerTextureUpdater::sampledTexelFormat(GC3Denum textureFormat)
{
    // Here we directly render to the texture, so the component order is always correct.
    return LayerTextureUpdater::SampledTexelFormatRGBA;
}

} // namespace cc
