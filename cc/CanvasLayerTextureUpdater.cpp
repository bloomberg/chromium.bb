// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "CanvasLayerTextureUpdater.h"

#include "CCRenderingStats.h"
#include "FloatRect.h"
#include "LayerPainterChromium.h"
#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkRect.h"
#include "SkiaUtils.h"
#include "TraceEvent.h"
#include <wtf/CurrentTime.h>

namespace cc {

CanvasLayerTextureUpdater::CanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium> painter)
    : m_painter(painter)
{
}

CanvasLayerTextureUpdater::~CanvasLayerTextureUpdater()
{
}

void CanvasLayerTextureUpdater::paintContents(SkCanvas* canvas, const IntRect& contentRect, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, CCRenderingStats& stats)
{
    TRACE_EVENT0("cc", "CanvasLayerTextureUpdater::paintContents");
    canvas->save();
    canvas->translate(CCFloatToSkScalar(-contentRect.x()), CCFloatToSkScalar(-contentRect.y()));

    IntRect layerRect = contentRect;

    if (contentsWidthScale != 1 || contentsHeightScale != 1) {
        canvas->scale(CCFloatToSkScalar(contentsWidthScale), CCFloatToSkScalar(contentsHeightScale));

        FloatRect rect = contentRect;
        rect.scale(1 / contentsWidthScale, 1 / contentsHeightScale);
        layerRect = enclosingIntRect(rect);
    }

    SkPaint paint;
    paint.setAntiAlias(false);
    paint.setXfermodeMode(SkXfermode::kClear_Mode);
    SkRect layerSkRect = SkRect::MakeXYWH(layerRect.x(), layerRect.y(), layerRect.width(), layerRect.height());
    canvas->drawRect(layerSkRect, paint);
    canvas->clipRect(layerSkRect);

    FloatRect opaqueLayerRect;
    double paintBeginTime = monotonicallyIncreasingTime();
    m_painter->paint(canvas, layerRect, opaqueLayerRect);
    stats.totalPaintTimeInSeconds += monotonicallyIncreasingTime() - paintBeginTime;
    canvas->restore();

    FloatRect opaqueContentRect = opaqueLayerRect;
    opaqueContentRect.scale(contentsWidthScale, contentsHeightScale);
    resultingOpaqueRect = enclosedIntRect(opaqueContentRect);

    m_contentRect = contentRect;
}

} // namespace cc
#endif // USE(ACCELERATED_COMPOSITING)
