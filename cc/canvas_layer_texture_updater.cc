// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"

#include "CanvasLayerTextureUpdater.h"

#include "CCRenderingStats.h"
#include "FloatRect.h"
#include "LayerPainterChromium.h"
#include "SkiaUtils.h"
#include "TraceEvent.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include <wtf/CurrentTime.h>

namespace cc {

CanvasLayerTextureUpdater::CanvasLayerTextureUpdater(scoped_ptr<LayerPainterChromium> painter)
    : m_painter(painter.Pass())
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
