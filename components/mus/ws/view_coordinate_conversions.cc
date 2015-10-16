// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/view_coordinate_conversions.h"

#include "components/mus/ws/server_view.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace mus {

namespace {

gfx::Vector2dF CalculateOffsetToAncestor(const ServerView* view,
                                         const ServerView* ancestor) {
  DCHECK(ancestor->Contains(view));
  gfx::Vector2d result;
  for (const ServerView* v = view; v != ancestor; v = v->parent())
    result += v->bounds().OffsetFromOrigin();
  return gfx::Vector2dF(result.x(), result.y());
}

}  // namespace

gfx::Point ConvertPointBetweenViews(const ServerView* from,
                                    const ServerView* to,
                                    const gfx::Point& point) {
  return gfx::ToFlooredPoint(
      ConvertPointFBetweenViews(from, to, gfx::PointF(point.x(), point.y())));
}

gfx::PointF ConvertPointFBetweenViews(const ServerView* from,
                                      const ServerView* to,
                                      const gfx::PointF& point) {
  DCHECK(from);
  DCHECK(to);
  if (from == to)
    return point;

  if (from->Contains(to)) {
    const gfx::Vector2dF offset(CalculateOffsetToAncestor(to, from));
    return point - offset;
  }
  DCHECK(to->Contains(from));
  const gfx::Vector2dF offset(CalculateOffsetToAncestor(from, to));
  return point + offset;
}

gfx::Rect ConvertRectBetweenViews(const ServerView* from,
                                  const ServerView* to,
                                  const gfx::Rect& rect) {
  DCHECK(from);
  DCHECK(to);
  if (from == to)
    return rect;

  const gfx::Point top_left(ConvertPointBetweenViews(from, to, rect.origin()));
  const gfx::Point bottom_right(gfx::ToCeiledPoint(ConvertPointFBetweenViews(
      from, to, gfx::PointF(rect.right(), rect.bottom()))));
  return gfx::Rect(top_left.x(), top_left.y(), bottom_right.x() - top_left.x(),
                   bottom_right.y() - top_left.y());
}

}  // namespace mus
