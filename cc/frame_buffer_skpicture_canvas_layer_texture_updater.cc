// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/frame_buffer_skpicture_canvas_layer_texture_updater.h"

#include "cc/layer_painter.h"

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

scoped_refptr<FrameBufferSkPictureCanvasLayerTextureUpdater> FrameBufferSkPictureCanvasLayerTextureUpdater::create(scoped_ptr<LayerPainterChromium> painter)
{
    return make_scoped_refptr(new FrameBufferSkPictureCanvasLayerTextureUpdater(painter.Pass()));
}

FrameBufferSkPictureCanvasLayerTextureUpdater::FrameBufferSkPictureCanvasLayerTextureUpdater(scoped_ptr<LayerPainterChromium> painter)
    : SkPictureCanvasLayerTextureUpdater(painter.Pass())
{
}

FrameBufferSkPictureCanvasLayerTextureUpdater::~FrameBufferSkPictureCanvasLayerTextureUpdater()
{
}

scoped_ptr<LayerTextureUpdater::Texture> FrameBufferSkPictureCanvasLayerTextureUpdater::createTexture(CCPrioritizedTextureManager* manager)
{
    return scoped_ptr<LayerTextureUpdater::Texture>(new Texture(this, CCPrioritizedTexture::create(manager)));
}

} // namespace cc
