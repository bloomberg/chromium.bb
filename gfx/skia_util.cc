// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/skia_util.h"

#include "gfx/rect.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

namespace gfx {

SkRect RectToSkRect(const gfx::Rect& rect) {
  SkRect r;
  r.set(SkIntToScalar(rect.x()), SkIntToScalar(rect.y()),
        SkIntToScalar(rect.right()), SkIntToScalar(rect.bottom()));
  return r;
}

gfx::Rect SkRectToRect(const SkRect& rect) {
  return gfx::Rect(SkScalarToFixed(rect.fLeft),
                   SkScalarToFixed(rect.fTop),
                   SkScalarToFixed(rect.width()),
                   SkScalarToFixed(rect.height()));
}

SkShader* CreateGradientShader(int start_point,
                               int end_point,
                               SkColor start_color,
                               SkColor end_color) {
  SkColor grad_colors[2] = { start_color, end_color};
  SkPoint grad_points[2];
  grad_points[0].set(SkIntToScalar(0), SkIntToScalar(start_point));
  grad_points[1].set(SkIntToScalar(0), SkIntToScalar(end_point));

  return SkGradientShader::CreateLinear(
      grad_points, grad_colors, NULL, 2, SkShader::kRepeat_TileMode);
}

}  // namespace gfx

