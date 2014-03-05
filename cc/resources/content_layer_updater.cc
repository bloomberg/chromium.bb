// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/content_layer_updater.h"

#include "base/debug/trace_event.h"
#include "base/time/time.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/layer_painter.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/rect_f.h"

namespace cc {

ContentLayerUpdater::ContentLayerUpdater(
    scoped_ptr<LayerPainter> painter,
    RenderingStatsInstrumentation* stats_instrumentation,
    int layer_id)
    : rendering_stats_instrumentation_(stats_instrumentation),
      layer_id_(layer_id),
      layer_is_opaque_(false),
      painter_(painter.Pass()) {}

ContentLayerUpdater::~ContentLayerUpdater() {}

void ContentLayerUpdater::set_rendering_stats_instrumentation(
    RenderingStatsInstrumentation* rsi) {
  rendering_stats_instrumentation_ = rsi;
}

void ContentLayerUpdater::PaintContents(SkCanvas* canvas,
                                        const gfx::Point& origin,
                                        float contents_width_scale,
                                        float contents_height_scale,
                                        gfx::Rect* resulting_opaque_rect) {
  TRACE_EVENT0("cc", "ContentLayerUpdater::PaintContents");
  canvas->save();
  canvas->translate(SkFloatToScalar(-origin.x()),
                    SkFloatToScalar(-origin.y()));

  SkISize size = canvas->getDeviceSize();
  gfx::Rect content_rect(origin, gfx::Size(size.width(), size.height()));

  gfx::Rect layer_rect = content_rect;

  if (contents_width_scale != 1.f || contents_height_scale != 1.f) {
    canvas->scale(SkFloatToScalar(contents_width_scale),
                  SkFloatToScalar(contents_height_scale));

    layer_rect = gfx::ScaleToEnclosingRect(
        content_rect, 1.f / contents_width_scale, 1.f / contents_height_scale);
  }

  SkRect layer_sk_rect = SkRect::MakeXYWH(
      layer_rect.x(), layer_rect.y(), layer_rect.width(), layer_rect.height());

  canvas->clipRect(layer_sk_rect);

  // If the layer has opaque contents then there is no need to
  // clear the canvas before painting.
  if (!layer_is_opaque_) {
    TRACE_EVENT0("cc", "Clear");
    canvas->drawColor(SK_ColorTRANSPARENT, SkXfermode::kSrc_Mode);
  }

  gfx::RectF opaque_layer_rect;
  painter_->Paint(canvas, layer_rect, &opaque_layer_rect);
  canvas->restore();

  gfx::Rect opaque_content_rect = gfx::ToEnclosedRect(gfx::ScaleRect(
      opaque_layer_rect, contents_width_scale, contents_height_scale));
  *resulting_opaque_rect = opaque_content_rect;

  content_rect_ = content_rect;
}

void ContentLayerUpdater::SetOpaque(bool opaque) {
  layer_is_opaque_ = opaque;
}

}  // namespace cc
