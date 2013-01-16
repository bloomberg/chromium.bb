// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/content_layer_updater.h"

#include "base/debug/trace_event.h"
#include "base/time.h"
#include "cc/layer_painter.h"
#include "cc/rendering_stats.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/rect_f.h"

namespace cc {

ContentLayerUpdater::ContentLayerUpdater(scoped_ptr<LayerPainter> painter)
    : m_painter(painter.Pass())
{
}

ContentLayerUpdater::~ContentLayerUpdater()
{
}

void ContentLayerUpdater::paintContents(SkCanvas* canvas, const gfx::Rect& contentRect, float contentsWidthScale, float contentsHeightScale, gfx::Rect& resultingOpaqueRect, RenderingStats& stats)
{
    TRACE_EVENT0("cc", "ContentLayerUpdater::paintContents");
    canvas->save();
    canvas->translate(SkFloatToScalar(-contentRect.x()), SkFloatToScalar(-contentRect.y()));

    gfx::Rect layerRect = contentRect;

    if (contentsWidthScale != 1 || contentsHeightScale != 1) {
        canvas->scale(SkFloatToScalar(contentsWidthScale), SkFloatToScalar(contentsHeightScale));

        gfx::RectF rect = gfx::ScaleRect(contentRect, 1 / contentsWidthScale, 1 / contentsHeightScale);
        layerRect = gfx::ToEnclosingRect(rect);
    }

    SkPaint paint;
    paint.setAntiAlias(false);
    paint.setXfermodeMode(SkXfermode::kClear_Mode);
    SkRect layerSkRect = SkRect::MakeXYWH(layerRect.x(), layerRect.y(), layerRect.width(), layerRect.height());
    canvas->drawRect(layerSkRect, paint);
    canvas->clipRect(layerSkRect);

    gfx::RectF opaqueLayerRect;
    base::TimeTicks paintBeginTime = base::TimeTicks::Now();
    m_painter->paint(canvas, layerRect, opaqueLayerRect);
    stats.totalPaintTime += base::TimeTicks::Now() - paintBeginTime;
    canvas->restore();

    stats.totalPixelsPainted += contentRect.width() * contentRect.height();

    gfx::RectF opaqueContentRect = gfx::ScaleRect(opaqueLayerRect, contentsWidthScale, contentsHeightScale);
    resultingOpaqueRect = gfx::ToEnclosedRect(opaqueContentRect);

    m_contentRect = contentRect;
}

}  // namespace cc
