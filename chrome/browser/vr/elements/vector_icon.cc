// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/vector_icon.h"

#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

void DrawVectorIcon(gfx::Canvas* canvas,
                    const gfx::VectorIcon& icon,
                    float size_px,
                    const gfx::PointF& corner,
                    SkColor color) {
  gfx::ScopedCanvas scoped(canvas);
  canvas->Translate(
      {static_cast<int>(corner.x()), static_cast<int>(corner.y())});

  // Explicitly cut out the 1x version of the icon, as PaintVectorIcon draws the
  // 1x version if device scale factor isn't set. See crbug.com/749146.
  gfx::VectorIcon icon_no_1x{icon.path, nullptr};
  PaintVectorIcon(canvas, icon_no_1x, size_px, color);
}

}  // namespace vr
