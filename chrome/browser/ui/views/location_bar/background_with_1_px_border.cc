// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/background_with_1_px_border.h"

#include "chrome/browser/ui/views/layout_constants.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

BackgroundWith1PxBorder::BackgroundWith1PxBorder(SkColor background,
                                                 SkColor border,
                                                 bool is_popup_mode)
    : border_color_(border), is_popup_mode_(is_popup_mode) {
  SetNativeControlColor(background);
}

void BackgroundWith1PxBorder::Paint(gfx::Canvas* canvas,
                                    views::View* view) const {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();
  const float kPreScaleOffset =
      GetLayoutConstant(LOCATION_BAR_BORDER_THICKNESS);
  const float kPostScaleOffset = -0.5f;
  gfx::RectF border_rect_f(view->GetContentsBounds());
  SkPath path;
  if (is_popup_mode_) {
    // Maximized popup windows don't draw the horizontal edges.  We implement
    // this by not insetting the edge before scaling. The subsequent post
    // scaling inset expands the paint area outside of the canvas.
    border_rect_f.Inset(view->GetWidget()->IsMaximized() ? 0 : kPreScaleOffset,
                        kPreScaleOffset);
    border_rect_f.Scale(scale);
    border_rect_f.Inset(kPostScaleOffset, kPostScaleOffset);
    path.addRect(gfx::RectFToSkRect(border_rect_f));
  } else {
    border_rect_f.Inset(kPreScaleOffset, kPreScaleOffset);
    border_rect_f.Scale(scale);
    border_rect_f.Inset(kPostScaleOffset, kPostScaleOffset);
    const SkScalar kCornerRadius = SkDoubleToScalar(2.5f * scale);
    path.addRoundRect(gfx::RectFToSkRect(border_rect_f), kCornerRadius,
                      kCornerRadius);
  }

  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(1);
  paint.setAntiAlias(true);

  SkPath stroke_path;
  paint.getFillPath(path, &stroke_path);

  SkPath fill_path;
  Op(path, stroke_path, kDifference_SkPathOp, &fill_path);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(get_color());
  canvas->sk_canvas()->drawPath(fill_path, paint);

  paint.setColor(border_color_);
  canvas->sk_canvas()->drawPath(stroke_path, paint);
}
