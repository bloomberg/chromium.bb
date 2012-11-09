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

inline gfx::Size ClampSizeFromAbove(gfx::Size s, gfx::Size other) {
  return gfx::Size(s.width() < other.width() ? s.width() : other.width(),
                   s.height() < other.height() ? s.height() : other.height());
}

inline gfx::Vector2dF ClampFromAbove(gfx::Vector2dF s, gfx::Vector2dF max) {
  return gfx::Vector2dF(s.x() < max.x() ? s.x() : max.x(),
                        s.y() < max.y() ? s.y() : max.y());
}

inline gfx::Vector2dF ClampFromBelow(gfx::Vector2dF s, gfx::Vector2dF min) {
  return gfx::Vector2dF(s.x() > min.x() ? s.x() : min.x(),
                        s.y() > min.y() ? s.y() : min.y());
}

inline gfx::Vector2d ClampFromAbove(gfx::Vector2d s, gfx::Vector2d max) {
  return gfx::Vector2d(s.x() < max.x() ? s.x() : max.x(),
                       s.y() < max.y() ? s.y() : max.y());
}

inline gfx::Vector2d ClampFromBelow(gfx::Vector2d s, gfx::Vector2d min) {
  return gfx::Vector2d(s.x() > min.x() ? s.x() : min.x(),
                       s.y() > min.y() ? s.y() : min.y());
}

inline gfx::PointF ClampFromAbove(gfx::PointF s, gfx::PointF max) {
  return gfx::PointF(s.x() < max.x() ? s.x() : max.x(),
                     s.y() < max.y() ? s.y() : max.y());
}

inline gfx::PointF ClampFromBelow(gfx::PointF s, gfx::PointF min) {
  return gfx::PointF(s.x() > min.x() ? s.x() : min.x(),
                     s.y() > min.y() ? s.y() : min.y());
}

inline gfx::Point ClampFromAbove(gfx::Point s, gfx::Point max) {
  return gfx::Point(s.x() < max.x() ? s.x() : max.x(),
                    s.y() < max.y() ? s.y() : max.y());
}

inline gfx::Point ClampFromBelow(gfx::Point s, gfx::Point min) {
  return gfx::Point(s.x() > min.x() ? s.x() : min.x(),
                    s.y() > min.y() ? s.y() : min.y());
}

inline gfx::Point BottomRight(gfx::Rect rect) {
  return gfx::Point(rect.right(), rect.bottom());
}

inline gfx::PointF BottomRight(gfx::RectF rect) {
  return gfx::PointF(rect.right(), rect.bottom());
}

}  // namespace cc

#endif  // CC_GEOMETRY_H_
