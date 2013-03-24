// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/content_layer_updater.h"

#include "base/debug/trace_event.h"
#include "base/time.h"
#include "cc/debug/rendering_stats.h"
#include "cc/resources/layer_painter.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/rect_f.h"

namespace cc {

ContentLayerUpdater::ContentLayerUpdater(scoped_ptr<LayerPainter> painter)
    : painter_(painter.Pass()) {}

ContentLayerUpdater::~ContentLayerUpdater() {}

void ContentLayerUpdater::PaintContents(SkCanvas* canvas,
                                        gfx::Rect content_rect,
                                        float contents_width_scale,
                                        float contents_height_scale,
                                        gfx::Rect* resulting_opaque_rect,
                                        RenderingStats* stats) {
  TRACE_EVENT0("cc", "ContentLayerUpdater::paintContents");
  canvas->save();
  canvas->translate(SkFloatToScalar(-content_rect.x()),
                    SkFloatToScalar(-content_rect.y()));

  gfx::Rect layer_rect = content_rect;

  if (contents_width_scale != 1.f || contents_height_scale != 1.f) {
    canvas->scale(SkFloatToScalar(contents_width_scale),
                  SkFloatToScalar(contents_height_scale));

    gfx::RectF rect = gfx::ScaleRect(
        content_rect, 1.f / contents_width_scale, 1.f / contents_height_scale);
    layer_rect = gfx::ToEnclosingRect(rect);
  }

  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setXfermodeMode(SkXfermode::kClear_Mode);
  SkRect layer_sk_rect = SkRect::MakeXYWH(
      layer_rect.x(), layer_rect.y(), layer_rect.width(), layer_rect.height());
  canvas->drawRect(layer_sk_rect, paint);
  canvas->clipRect(layer_sk_rect);

  gfx::RectF opaque_layer_rect;
  base::TimeTicks paint_begin_time;
  if (stats)
    paint_begin_time = base::TimeTicks::Now();
  painter_->Paint(canvas, layer_rect, &opaque_layer_rect);
  if (stats) {
    stats->total_paint_time += base::TimeTicks::Now() - paint_begin_time;
    stats->total_pixels_painted += content_rect.width() * content_rect.height();
  }
  canvas->restore();

  gfx::RectF opaque_content_rect = gfx::ScaleRect(
      opaque_layer_rect, contents_width_scale, contents_height_scale);
  *resulting_opaque_rect = gfx::ToEnclosedRect(opaque_content_rect);

  content_rect_ = content_rect;
}

}  // namespace cc
