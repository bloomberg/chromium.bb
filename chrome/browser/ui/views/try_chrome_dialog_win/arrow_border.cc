// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/try_chrome_dialog_win/arrow_border.h"

#include <algorithm>
#include <utility>

#include "cc/paint/paint_canvas.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

ArrowBorder::ArrowBorder(int thickness,
                         SkColor color,
                         SkColor background_color,
                         const gfx::VectorIcon& arrow_icon,
                         const Properties* properties)
    : views::Border(color),
      insets_(gfx::Insets(thickness) + properties->insets),
      arrow_border_insets_(properties->arrow_border_insets),
      arrow_(gfx::CreateVectorIcon(arrow_icon, background_color)) {
  switch (properties->arrow_rotation) {
    case ArrowRotation::kNone:
      break;
    case ArrowRotation::k90Degrees:
      arrow_ = gfx::ImageSkiaOperations::CreateRotatedImage(
          arrow_, SkBitmapOperations::ROTATION_90_CW);
      break;
    case ArrowRotation::k180Degrees:
      arrow_ = gfx::ImageSkiaOperations::CreateRotatedImage(
          arrow_, SkBitmapOperations::ROTATION_180_CW);
      break;
    case ArrowRotation::k270Degrees:
      arrow_ = gfx::ImageSkiaOperations::CreateRotatedImage(
          arrow_, SkBitmapOperations::ROTATION_270_CW);
      break;
  }
}

void ArrowBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  // Undo DSF to be sure to draw an integral number of pixels for the border.
  // This ensures sharp lines even for fractional scale factors.
  gfx::ScopedCanvas scoped(canvas);
  float dsf = canvas->UndoDeviceScaleFactor();

  // Compute the bounds of the contents within the border (this scaling
  // operation floors the inset values).
  gfx::Rect content_bounds = gfx::Rect(
      view.GetWidget()->GetNativeView()->GetHost()->GetBoundsInPixels().size());
  content_bounds.Inset(insets_.Scale(dsf));

  // Clip out the contents, leaving behind the border.
  canvas->sk_canvas()->clipRect(gfx::RectToSkRect(content_bounds),
                                SkClipOp::kDifference, true);

  // Paint the rectangular border, less the region occupied by the arrow.
  {
    gfx::ScopedCanvas content_clip(canvas);

    // Clip out the arrow, less its insets.
    gfx::Rect arrow_bounds(arrow_bounds_);
    arrow_bounds.Inset(arrow_border_insets_.Scale(dsf));
    canvas->sk_canvas()->clipRect(gfx::RectToSkRect(arrow_bounds),
                                  SkClipOp::kDifference, true);
    canvas->DrawColor(color());
  }

  // Paint the arrow.

  // The arrow is a square icon and must be painted as such.
  gfx::Rect arrow_bounds(arrow_bounds_);
  int size = std::max(arrow_bounds.width(), arrow_bounds.height());
  gfx::Size arrow_size(size, size);

  // The arrow image is square with only the top 14 points (kArrowHeight)
  // containing the relevant drawing. When drawn "toward" the origin (i.e., for
  // a top or left taskbar), the arrow must be offset this extra amount.
  if (arrow_bounds.origin().x() < content_bounds.origin().x())
    arrow_bounds.Offset(arrow_bounds.width() - arrow_size.width(), 0);
  else if (arrow_bounds.origin().y() < content_bounds.origin().y())
    arrow_bounds.Offset(0, arrow_bounds.height() - arrow_size.height());

  canvas->DrawImageIntInPixel(arrow_.GetRepresentation(dsf), arrow_bounds.x(),
                              arrow_bounds.y(), arrow_size.width(),
                              arrow_size.height(), false, cc::PaintFlags());
}

gfx::Insets ArrowBorder::GetInsets() const {
  return insets_;
}

gfx::Size ArrowBorder::GetMinimumSize() const {
  return {insets_.width(), insets_.height()};
}
