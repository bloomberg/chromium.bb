// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/paint_aggregator.h"

#include "base/histogram.h"
#include "base/logging.h"

// ----------------------------------------------------------------------------
// ALGORITHM NOTES
//
// We attempt to maintain a scroll rect in the presence of invalidations that
// are contained within the scroll rect.  If an invalidation crosses a scroll
// rect, then we just treat the scroll rect as an invalidation rect.
//
// For invalidations performed prior to scrolling and contained within the
// scroll rect, we offset the invalidation rects to account for the fact that
// the consumer will perform scrolling before painting.
//
// We only support scrolling along one axis at a time.  A diagonal scroll will
// therefore be treated as an invalidation.
// ----------------------------------------------------------------------------

// If the combined area of paint rects contained within the scroll rect grows
// too large, then we might as well just treat the scroll rect as a paint rect.
// This constant sets the max ratio of paint rect area to scroll rect area that
// we will tolerate before downgrading the scroll into a repaint.
static const float kMaxRedundantPaintToScrollArea = 0.8f;

// The maximum number of paint rects.  If we exceed this limit, then we'll
// start combining paint rects (see CombinePaintRects).  This limiting is
// important since the WebKit code associated with deciding what to paint given
// a paint rect can be significant.
static const size_t kMaxPaintRects = 5;

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

gfx::Rect PaintAggregator::PendingUpdate::GetPaintBounds() const {
  gfx::Rect bounds;
  for (size_t i = 0; i < paint_rects.size(); ++i)
    bounds = bounds.Union(paint_rects[i]);
  return bounds;
}

bool PaintAggregator::HasPendingUpdate() const {
  return !update_.scroll_rect.IsEmpty() || !update_.paint_rects.empty();
}

void PaintAggregator::ClearPendingUpdate() {
  update_ = PendingUpdate();
}

void PaintAggregator::InvalidateRect(const gfx::Rect& rect) {
  // Combine overlapping paints using smallest bounding box.
  for (size_t i = 0; i < update_.paint_rects.size(); ++i) {
    const gfx::Rect& existing_rect = update_.paint_rects[i];
    if (existing_rect.Contains(rect))  // Optimize out redundancy.
      return;
    if (rect.Intersects(existing_rect) || rect.SharesEdgeWith(existing_rect)) {
      // Re-invalidate in case the union intersects other paint rects.
      gfx::Rect combined_rect = existing_rect.Union(rect);
      update_.paint_rects.erase(update_.paint_rects.begin() + i);
      InvalidateRect(combined_rect);
      return;
    }
  }

  // Add a non-overlapping paint.
  update_.paint_rects.push_back(rect);

  // If the new paint overlaps with a scroll, then it forces an invalidation of
  // the scroll.  If the new paint is contained by a scroll, then trim off the
  // scroll damage to avoid redundant painting.
  if (!update_.scroll_rect.IsEmpty()) {
    if (ShouldInvalidateScrollRect(rect)) {
      InvalidateScrollRect();
    } else if (update_.scroll_rect.Contains(rect)) {
      update_.paint_rects[update_.paint_rects.size() - 1] =
          rect.Subtract(update_.GetScrollDamage());
      if (update_.paint_rects[update_.paint_rects.size() - 1].IsEmpty())
        update_.paint_rects.erase(update_.paint_rects.end() - 1);
    }
  }

  if (update_.paint_rects.size() > kMaxPaintRects)
    CombinePaintRects();

  // Track how large the paint_rects vector grows during an invalidation
  // sequence.  Note: A subsequent invalidation may end up being combined
  // with all existing paints, which means that tracking the size of
  // paint_rects at the time when GetPendingUpdate() is called may mask
  // certain performance problems.
  HISTOGRAM_COUNTS_100("MPArch.RW_IntermediatePaintRectCount",
                       update_.paint_rects.size());
}

void PaintAggregator::ScrollRect(int dx, int dy, const gfx::Rect& clip_rect) {
  // We only support scrolling along one axis at a time.
  if (dx != 0 && dy != 0) {
    InvalidateRect(clip_rect);
    return;
  }

  // We can only scroll one rect at a time.
  if (!update_.scroll_rect.IsEmpty() &&
      !update_.scroll_rect.Equals(clip_rect)) {
    InvalidateRect(clip_rect);
    return;
  }

  // Again, we only support scrolling along one axis at a time.  Make sure this
  // update doesn't scroll on a different axis than any existing one.
  if ((dx && update_.scroll_delta.y()) || (dy && update_.scroll_delta.x())) {
    InvalidateRect(clip_rect);
    return;
  }

  // The scroll rect is new or isn't changing (though the scroll amount may
  // be changing).
  update_.scroll_rect = clip_rect;
  update_.scroll_delta.Offset(dx, dy);

  // We might have just wiped out a pre-existing scroll.
  if (update_.scroll_delta == gfx::Point()) {
    update_.scroll_rect = gfx::Rect();
    return;
  }

  // Adjust any contained paint rects and check for any overlapping paints.
  for (size_t i = 0; i < update_.paint_rects.size(); ++i) {
    if (update_.scroll_rect.Contains(update_.paint_rects[i])) {
      update_.paint_rects[i] = ScrollPaintRect(update_.paint_rects[i], dx, dy);
      // The rect may have been scrolled out of view.
      if (update_.paint_rects[i].IsEmpty()) {
        update_.paint_rects.erase(update_.paint_rects.begin() + i);
        i--;
      }
    } else if (update_.scroll_rect.Intersects(update_.paint_rects[i])) {
      InvalidateScrollRect();
      return;
    }
  }

  // If the new scroll overlaps too much with contained paint rects, then force
  // an invalidation of the scroll.
  if (ShouldInvalidateScrollRect(gfx::Rect()))
    InvalidateScrollRect();
}

gfx::Rect PaintAggregator::ScrollPaintRect(const gfx::Rect& paint_rect,
                                           int dx, int dy) const {
  gfx::Rect result = paint_rect;

  result.Offset(dx, dy);
  result = update_.scroll_rect.Intersect(result);

  // Subtract out the scroll damage rect to avoid redundant painting.
  return result.Subtract(update_.GetScrollDamage());
}

bool PaintAggregator::ShouldInvalidateScrollRect(const gfx::Rect& rect) const {
  if (!rect.IsEmpty()) {
    if (!update_.scroll_rect.Intersects(rect))
      return false;

    if (!update_.scroll_rect.Contains(rect))
      return true;
  }

  // Check if the combined area of all contained paint rects plus this new
  // rect comes too close to the area of the scroll_rect.  If so, then we
  // might as well invalidate the scroll rect.

  int paint_area = rect.size().GetArea();
  for (size_t i = 0; i < update_.paint_rects.size(); ++i) {
    const gfx::Rect& existing_rect = update_.paint_rects[i];
    if (update_.scroll_rect.Contains(existing_rect))
      paint_area += existing_rect.size().GetArea();
  }
  int scroll_area = update_.scroll_rect.size().GetArea();
  if (float(paint_area) / float(scroll_area) > kMaxRedundantPaintToScrollArea)
    return true;

  return false;
}

void PaintAggregator::InvalidateScrollRect() {
  gfx::Rect scroll_rect = update_.scroll_rect;
  update_.scroll_rect = gfx::Rect();
  update_.scroll_delta = gfx::Point();
  InvalidateRect(scroll_rect);
}

void PaintAggregator::CombinePaintRects() {
  // Combine paint rects down to at most two rects: one inside the scroll_rect
  // and one outside the scroll_rect.  If there is no scroll_rect, then just
  // use the smallest bounding box for all paint rects.
  //
  // NOTE: This is a fairly simple algorithm.  We could get fancier by only
  // combining two rects to get us under the kMaxPaintRects limit, but if we
  // reach this method then it means we're hitting a rare case, so there's no
  // need to over-optimize it.
  //
  if (update_.scroll_rect.IsEmpty()) {
    gfx::Rect bounds = update_.GetPaintBounds();
    update_.paint_rects.clear();
    update_.paint_rects.push_back(bounds);
  } else {
    gfx::Rect inner, outer;
    for (size_t i = 0; i < update_.paint_rects.size(); ++i) {
      const gfx::Rect& existing_rect = update_.paint_rects[i];
      if (update_.scroll_rect.Contains(existing_rect)) {
        inner = inner.Union(existing_rect);
      } else {
        outer = outer.Union(existing_rect);
      }
    }
    update_.paint_rects.clear();
    update_.paint_rects.push_back(inner);
    update_.paint_rects.push_back(outer);
  }
}
