// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "SkPictureCanvasLayerTextureUpdater.h"

#include "LayerPainterChromium.h"
#include "SkCanvas.h"
#include "TraceEvent.h"

namespace cc {

SkPictureCanvasLayerTextureUpdater::SkPictureCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium> painter)
    : CanvasLayerTextureUpdater(painter)
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

void SkPictureCanvasLayerTextureUpdater::setOpaque(bool opaque)
{
    m_layerIsOpaque = opaque;
}

} // namespace cc
#endif // USE(ACCELERATED_COMPOSITING)
