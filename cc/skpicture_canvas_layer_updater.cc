// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/skpicture_canvas_layer_updater.h"

#include "base/debug/trace_event.h"
#include "cc/layer_painter.h"
#include "cc/texture_update_queue.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

SkPictureCanvasLayerUpdater::Texture::Texture(SkPictureCanvasLayerUpdater* updater, scoped_ptr<PrioritizedTexture> texture)
    : LayerUpdater::Texture(texture.Pass())
    , m_updater(updater)
{
}

SkPictureCanvasLayerUpdater::Texture::~Texture()
{
}

void SkPictureCanvasLayerUpdater::Texture::update(TextureUpdateQueue& queue, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats&)
{
    updater()->updateTexture(queue, texture(), sourceRect, destOffset, partialUpdate);
}

SkPictureCanvasLayerUpdater::SkPictureCanvasLayerUpdater(scoped_ptr<LayerPainter> painter)
    : CanvasLayerUpdater(painter.Pass())
    , m_layerIsOpaque(false)
{
}

SkPictureCanvasLayerUpdater::~SkPictureCanvasLayerUpdater()
{
}

scoped_refptr<SkPictureCanvasLayerUpdater> SkPictureCanvasLayerUpdater::create(scoped_ptr<LayerPainter> painter)
{
    return make_scoped_refptr(new SkPictureCanvasLayerUpdater(painter.Pass()));
}

scoped_ptr<LayerUpdater::Texture> SkPictureCanvasLayerUpdater::createTexture(PrioritizedTextureManager* manager)
{
    return scoped_ptr<LayerUpdater::Texture>(new Texture(this, PrioritizedTexture::create(manager)));
}

void SkPictureCanvasLayerUpdater::prepareToUpdate(const IntRect& contentRect, const IntSize&, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, RenderingStats& stats)
{
    SkCanvas* canvas = m_picture.beginRecording(contentRect.width(), contentRect.height());
    paintContents(canvas, contentRect, contentsWidthScale, contentsHeightScale, resultingOpaqueRect, stats);
    m_picture.endRecording();
}

void SkPictureCanvasLayerUpdater::drawPicture(SkCanvas* canvas)
{
    TRACE_EVENT0("cc", "SkPictureCanvasLayerUpdater::drawPicture");
    canvas->drawPicture(m_picture);
}

void SkPictureCanvasLayerUpdater::updateTexture(TextureUpdateQueue& queue, PrioritizedTexture* texture, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate)
{
    ResourceUpdate upload = ResourceUpdate::CreateFromPicture(
        texture, &m_picture, contentRect(), sourceRect, destOffset);
    if (partialUpdate)
        queue.appendPartialUpload(upload);
    else
        queue.appendFullUpload(upload);
}

void SkPictureCanvasLayerUpdater::setOpaque(bool opaque)
{
    m_layerIsOpaque = opaque;
}

} // namespace cc
