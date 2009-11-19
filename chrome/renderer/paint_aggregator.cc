// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/paint_aggregator.h"

#include "base/logging.h"

// We implement a very simple algorithm:
//
// - Multiple repaints are unioned to form the smallest bounding box.
// - If a scroll intersects a repaint, then the scroll is downgraded
//   to a repaint and then unioned with the existing repaint.
//
// This allows for a scroll to exist in parallel to a repaint provided the two
// do not intersect.

gfx::Rect PaintAggregator::PendingUpdate::GetScrollDamage() const {
  // Should only be scrolling in one direction at a time.
  DCHECK(!(scroll_delta.x() && scroll_delta.y()));

  gfx::Rect damaged_rect;

  // Compute the region we will expose by scrolling, and paint that into a
  // shared memory section.
  if (scroll_delta.x()) {
    int dx = scroll_delta.x();
    damaged_rect.set_y(scroll_rect.y());
    damaged_rect.set_height(scroll_rect.height());
    if (dx > 0) {
      damaged_rect.set_x(scroll_rect.x());
      damaged_rect.set_width(dx);
    } else {
      damaged_rect.set_x(scroll_rect.right() + dx);
      damaged_rect.set_width(-dx);
    }
  } else {
    int dy = scroll_delta.y();
    damaged_rect.set_x(scroll_rect.x());
    damaged_rect.set_width(scroll_rect.width());
    if (dy > 0) {
      damaged_rect.set_y(scroll_rect.y());
      damaged_rect.set_height(dy);
    } else {
      damaged_rect.set_y(scroll_rect.bottom() + dy);
      damaged_rect.set_height(-dy);
    }
  }

  // In case the scroll offset exceeds the width/height of the scroll rect
  return scroll_rect.Intersect(damaged_rect);
}

bool PaintAggregator::HasPendingUpdate() const {
  return !update_.scroll_rect.IsEmpty() || !update_.paint_rect.IsEmpty();
}

void PaintAggregator::ClearPendingUpdate() {
  update_ = PendingUpdate();
}

void PaintAggregator::InvalidateRect(const gfx::Rect& rect) {
  // If this invalidate overlaps with a pending scroll, then we have to
  // downgrade to invalidating the scroll rect.
  if (rect.Intersects(update_.scroll_rect)) {
    update_.paint_rect = update_.paint_rect.Union(update_.scroll_rect);
    update_.scroll_rect = gfx::Rect();
    update_.scroll_delta = gfx::Point();
  }

  update_.paint_rect = update_.paint_rect.Union(rect);
}

void PaintAggregator::ScrollRect(int dx, int dy, const gfx::Rect& clip_rect) {
  if (dx != 0 && dy != 0) {
    // We only support scrolling along one axis at a time.
    ScrollRect(0, dy, clip_rect);
    dy = 0;
  }

  bool intersects_with_painting = update_.paint_rect.Intersects(clip_rect);

  // If we already have a pending scroll operation or if this scroll operation
  // intersects the existing paint region, then just failover to invalidating.
  if (!update_.scroll_rect.IsEmpty() || intersects_with_painting) {
    if (!intersects_with_painting && update_.scroll_rect == clip_rect) {
      // OK, we can just update the scroll delta (requires same scrolling axis)
      if (!dx && !update_.scroll_delta.x()) {
        update_.scroll_delta.set_y(update_.scroll_delta.y() + dy);
        return;
      }
      if (!dy && !update_.scroll_delta.y()) {
        update_.scroll_delta.set_x(update_.scroll_delta.x() + dx);
        return;
      }
    }
    InvalidateRect(update_.scroll_rect);
    DCHECK(update_.scroll_rect.IsEmpty());
    InvalidateRect(clip_rect);
    return;
  }

  update_.scroll_rect = clip_rect;
  update_.scroll_delta = gfx::Point(dx, dy);
}
