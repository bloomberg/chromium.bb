// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_GEOMETRY_H_
#define CC_GEOMETRY_H_

#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/vector2d.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {

inline gfx::Point BottomRight(gfx::Rect rect) {
  return gfx::Point(rect.right(), rect.bottom());
}

inline gfx::PointF BottomRight(gfx::RectF rect) {
  return gfx::PointF(rect.right(), rect.bottom());
}

}  // namespace cc

#endif  // CC_GEOMETRY_H_
