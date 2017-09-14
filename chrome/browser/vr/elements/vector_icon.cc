// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/vector_icon.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"

namespace vr {

void DrawVectorIcon(gfx::Canvas* canvas,
                    const gfx::VectorIcon& icon,
                    float size_px,
                    const gfx::PointF& corner,
                    SkColor color) {
  // TODO(cjgrant): Use CreateVectorIcon() when threading issues are resolved.
  canvas->Save();
  canvas->Translate({corner.x(), corner.y()});
  PaintVectorIcon(canvas, icon, size_px, color);
  canvas->Restore();
}

}  // namespace vr
