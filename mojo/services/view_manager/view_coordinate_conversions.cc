// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_coordinate_conversions.h"

#include "mojo/services/view_manager/server_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace view_manager {

namespace {

gfx::Vector2d CalculateOffsetToAncestor(const ServerView* view,
                                        const ServerView* ancestor) {
  DCHECK(ancestor->Contains(view));
  gfx::Vector2d result;
  for (const ServerView* v = view; v != ancestor; v = v->parent())
    result += v->bounds().OffsetFromOrigin();
  return result;
}

}  // namespace

gfx::Rect ConvertRectBetweenViews(const ServerView* from,
                                  const ServerView* to,
                                  const gfx::Rect& rect) {
  DCHECK(from);
  if (from == to)
    return rect;

  if (from->Contains(to)) {
    const gfx::Vector2d offset(CalculateOffsetToAncestor(to, from));
    return gfx::Rect(rect.origin() - offset, rect.size());
  }
  DCHECK(to->Contains(from));
  const gfx::Vector2d offset(CalculateOffsetToAncestor(from, to));
  return gfx::Rect(rect.origin() + offset, rect.size());
}

}  // namespace view_manager
