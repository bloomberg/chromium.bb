// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/canvas_layer_texture_updater.h"

#include "CCRenderingStats.h"
#include "FloatRect.h"
#include "SkiaUtils.h"
#include "base/debug/trace_event.h"
#include "base/time.h"
#include "cc/layer_painter.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"

namespace cc {

CanvasLayerTextureUpdater::CanvasLayerTextureUpdater(scoped_ptr<LayerPainter> painter)
    : m_painter(painter.Pass())
{
}

CanvasLayerTextureUpdater::~CanvasLayerTextureUpdater()
{
}

void CanvasLayerTextureUpdater::paintContents(SkCanvas* canvas, const IntRect& contentRect, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, RenderingStats& stats)
{
    TRACE_EVENT0("cc", "CanvasLayerTextureUpdater::paintContents");
    canvas->save();
    canvas->translate(FloatToSkScalar(-contentRect.x()), FloatToSkScalar(-contentRect.y()));

    IntRect layerRect = contentRect;

    if (contentsWidthScale != 1 || contentsHeightScale != 1) {
        canvas->scale(FloatToSkScalar(contentsWidthScale), FloatToSkScalar(contentsHeightScale));

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
    base::TimeTicks paintBeginTime = base::TimeTicks::Now();
    m_painter->paint(canvas, layerRect, opaqueLayerRect);
    stats.totalPaintTimeInSeconds += (base::TimeTicks::Now() - paintBeginTime).InSecondsF();
    canvas->restore();

    FloatRect opaqueContentRect = opaqueLayerRect;
    opaqueContentRect.scale(contentsWidthScale, contentsHeightScale);
    resultingOpaqueRect = enclosedIntRect(opaqueContentRect);

    m_contentRect = contentRect;
}

} // namespace cc
