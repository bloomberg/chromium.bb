// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/vector_icon.h"

#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/paint_vector_icon.h"

namespace vr {

void DrawVectorIcon(gfx::Canvas* canvas,
                    const gfx::VectorIcon& icon,
                    float size_px,
                    const gfx::PointF& corner,
                    SkColor color) {
  gfx::ImageSkia image = CreateVectorIcon(
      gfx::IconDescription(icon, 0, color, base::TimeDelta(), gfx::kNoneIcon));

  // Determine how much we need to scale the icon to fit the target region.
  float scale = size_px / image.width();
  const gfx::ImageSkiaRep& image_rep = image.GetRepresentation(scale);

  // Blit the icon based on its desired position on the canvas.
  cc::PaintFlags flags;
  gfx::Point point(corner.x(), corner.y());
  canvas->DrawImageIntInPixel(image_rep, point.x(), point.y(),
                              image_rep.pixel_width(), image_rep.pixel_height(),
                              false, flags);
}

}  // namespace vr
