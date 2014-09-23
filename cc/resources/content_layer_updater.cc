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
#include "ui/gfx/skia_util.h"

namespace cc {

ContentLayerUpdater::ContentLayerUpdater(
    scoped_ptr<LayerPainter> painter,
    RenderingStatsInstrumentation* stats_instrumentation,
    int layer_id)
    : rendering_stats_instrumentation_(stats_instrumentation),
      layer_id_(layer_id),
      layer_is_opaque_(false),
      layer_fills_bounds_completely_(false),
      painter_(painter.Pass()),
      background_color_(SK_ColorTRANSPARENT) {
}

ContentLayerUpdater::~ContentLayerUpdater() {}

void ContentLayerUpdater::set_rendering_stats_instrumentation(
    RenderingStatsInstrumentation* rsi) {
  rendering_stats_instrumentation_ = rsi;
}

void ContentLayerUpdater::PaintContents(SkCanvas* canvas,
                                        const gfx::Size& layer_content_size,
                                        const gfx::Rect& paint_rect,
                                        float contents_width_scale,
                                        float contents_height_scale) {
  TRACE_EVENT0("cc", "ContentLayerUpdater::PaintContents");
  if (!canvas)
    return;
  canvas->save();
  canvas->translate(SkIntToScalar(-paint_rect.x()),
                    SkIntToScalar(-paint_rect.y()));

  // The |canvas| backing should be sized to hold the |paint_rect|.
  DCHECK_EQ(paint_rect.width(), canvas->getBaseLayerSize().width());
  DCHECK_EQ(paint_rect.height(), canvas->getBaseLayerSize().height());

  const bool is_scaled =
      contents_width_scale != 1.f || contents_height_scale != 1.f;

  if (is_scaled && (layer_is_opaque_ || layer_fills_bounds_completely_)) {
    // Even if completely covered, for rasterizations that touch the edge of the
    // layer, we also need to raster the background color underneath the last
    // texel (since the paint won't cover it).
    //
    // The final texel of content may only be partially covered by a
    // rasterization; this rect represents the content rect that is fully
    // covered by content.
    const gfx::Rect layer_content_rect = gfx::Rect(layer_content_size);
    gfx::Rect deflated_layer_content_rect = layer_content_rect;
    deflated_layer_content_rect.Inset(0, 0, 1, 1);

    if (!layer_content_rect.Contains(deflated_layer_content_rect)) {
      // Drawing at most 1 x 1 x (canvas width + canvas height) texels is 2-3X
      // faster than clearing, so special case this.
      DCHECK_LE(paint_rect.right(), layer_content_rect.right());
      DCHECK_LE(paint_rect.bottom(), layer_content_rect.bottom());
      canvas->save();
      canvas->clipRect(gfx::RectToSkRect(layer_content_rect),
                       SkRegion::kReplace_Op);
      canvas->clipRect(gfx::RectToSkRect(deflated_layer_content_rect),
                       SkRegion::kDifference_Op);
      canvas->drawColor(background_color_, SkXfermode::kSrc_Mode);
      canvas->restore();
    }
  }

  gfx::Rect layer_rect;
  if (is_scaled) {
    canvas->scale(SkFloatToScalar(contents_width_scale),
                  SkFloatToScalar(contents_height_scale));

    // NOTE: this may go beyond the bounds of the layer, but that shouldn't
    // cause problems (anything beyond the layer is clipped out).
    layer_rect = gfx::ScaleToEnclosingRect(
        paint_rect, 1.f / contents_width_scale, 1.f / contents_height_scale);
  } else {
    layer_rect = paint_rect;
  }

  SkRect layer_sk_rect = SkRect::MakeXYWH(
      layer_rect.x(), layer_rect.y(), layer_rect.width(), layer_rect.height());

  canvas->clipRect(layer_sk_rect);

  // If the layer has opaque contents or will fill the bounds completely there
  // is no need to clear the canvas before painting.
  if (!layer_is_opaque_ && !layer_fills_bounds_completely_) {
    TRACE_EVENT0("cc", "Clear");
    canvas->drawColor(SK_ColorTRANSPARENT, SkXfermode::kSrc_Mode);
  }

  painter_->Paint(canvas, layer_rect);
  canvas->restore();

  paint_rect_ = paint_rect;
}

void ContentLayerUpdater::SetOpaque(bool opaque) {
  layer_is_opaque_ = opaque;
}

void ContentLayerUpdater::SetFillsBoundsCompletely(bool fills_bounds) {
  layer_fills_bounds_completely_ = fills_bounds;
}

void ContentLayerUpdater::SetBackgroundColor(SkColor background_color) {
  background_color_ = background_color;
}

}  // namespace cc
