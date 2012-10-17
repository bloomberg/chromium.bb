// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/skpicture_canvas_layer_texture_updater.h"

#include "CCTextureUpdateQueue.h"
#include "TraceEvent.h"
#include "cc/layer_painter.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

SkPictureCanvasLayerTextureUpdater::SkPictureCanvasLayerTextureUpdater(scoped_ptr<LayerPainterChromium> painter)
    : CanvasLayerTextureUpdater(painter.Pass())
    , m_layerIsOpaque(false)
{
}

SkPictureCanvasLayerTextureUpdater::~SkPictureCanvasLayerTextureUpdater()
{
}

void SkPictureCanvasLayerTextureUpdater::prepareToUpdate(const IntRect& contentRect, const IntSize&, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, CCRenderingStats& stats)
{
    SkCanvas* canvas = m_picture.beginRecording(contentRect.width(), contentRect.height());
    paintContents(canvas, contentRect, contentsWidthScale, contentsHeightScale, resultingOpaqueRect, stats);
    m_picture.endRecording();
}

void SkPictureCanvasLayerTextureUpdater::drawPicture(SkCanvas* canvas)
{
    TRACE_EVENT0("cc", "SkPictureCanvasLayerTextureUpdater::drawPicture");
    canvas->drawPicture(m_picture);
}

void SkPictureCanvasLayerTextureUpdater::updateTexture(CCTextureUpdateQueue& queue, CCPrioritizedTexture* texture, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate)
{
    TextureUploader::Parameters upload = { texture, NULL, &m_picture, { contentRect(), sourceRect, destOffset } };
    if (partialUpdate)
        queue.appendPartialUpload(upload);
    else
        queue.appendFullUpload(upload);
}

void SkPictureCanvasLayerTextureUpdater::setOpaque(bool opaque)
{
    m_layerIsOpaque = opaque;
}

} // namespace cc
