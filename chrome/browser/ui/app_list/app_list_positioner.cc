// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_positioner.h"

#include <algorithm>

#include "base/logging.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

AppListPositioner::AppListPositioner(const gfx::Display& display,
                                     const gfx::Size& window_size,
                                     int min_distance_from_edge)
    : display_(display),
      window_size_(window_size),
      min_distance_from_edge_(min_distance_from_edge) {}

void AppListPositioner::WorkAreaSubtract(const gfx::Rect& rect) {
  gfx::Rect work_area = display_.work_area();
  work_area.Subtract(rect);
  display_.set_work_area(work_area);
}

void AppListPositioner::WorkAreaInset(int left,
                                      int top,
                                      int right,
                                      int bottom) {
  gfx::Rect work_area = display_.work_area();
  work_area.Inset(left, top, right, bottom);
  display_.set_work_area(work_area);
}

gfx::Point AppListPositioner::GetAnchorPointForScreenCenter() const {
  return display_.bounds().CenterPoint();
}

gfx::Point AppListPositioner::GetAnchorPointForScreenCorner(
    ScreenCorner corner) const {
  const gfx::Rect& screen_rect = display_.bounds();
  gfx::Point anchor;
  switch (corner) {
    case SCREEN_CORNER_TOP_LEFT:
      anchor = screen_rect.origin();
      break;
    case SCREEN_CORNER_TOP_RIGHT:
      anchor = screen_rect.top_right();
      break;
    case SCREEN_CORNER_BOTTOM_LEFT:
      anchor = screen_rect.bottom_left();
      break;
    case SCREEN_CORNER_BOTTOM_RIGHT:
      anchor = screen_rect.bottom_right();
      break;
    default:
      NOTREACHED();
      anchor = gfx::Point();
  }
  return ClampAnchorPoint(anchor);
}

gfx::Point AppListPositioner::GetAnchorPointForShelfCorner(
    ScreenEdge shelf_edge) const {
  const gfx::Rect& screen_rect = display_.bounds();
  const gfx::Rect& work_area = display_.work_area();
  gfx::Point anchor;
  switch (shelf_edge) {
    case SCREEN_EDGE_LEFT:
      anchor = gfx::Point(work_area.x(), screen_rect.y());
      break;
    case SCREEN_EDGE_RIGHT:
      anchor = gfx::Point(work_area.right(), screen_rect.y());
      break;
    case SCREEN_EDGE_TOP:
      anchor = gfx::Point(screen_rect.x(), work_area.y());
      break;
    case SCREEN_EDGE_BOTTOM:
      anchor = gfx::Point(screen_rect.x(), work_area.bottom());
      break;
    default:
      NOTREACHED();
      anchor = gfx::Point();
  }
  return ClampAnchorPoint(anchor);
}

gfx::Point AppListPositioner::GetAnchorPointForShelfCenter(
    ScreenEdge shelf_edge) const {
  const gfx::Rect& work_area = display_.work_area();
  gfx::Point anchor;
  switch (shelf_edge) {
    case SCREEN_EDGE_LEFT:
      anchor =
          gfx::Point(work_area.x(), work_area.y() + work_area.height() / 2);
      break;
    case SCREEN_EDGE_RIGHT:
      anchor =
          gfx::Point(work_area.right(), work_area.y() + work_area.height() / 2);
      break;
    case SCREEN_EDGE_TOP:
      anchor = gfx::Point(work_area.x() + work_area.width() / 2, work_area.y());
      break;
    case SCREEN_EDGE_BOTTOM:
      anchor =
          gfx::Point(work_area.x() + work_area.width() / 2, work_area.bottom());
      break;
    default:
      NOTREACHED();
      anchor = gfx::Point();
  }
  return ClampAnchorPoint(anchor);
}

gfx::Point AppListPositioner::GetAnchorPointForShelfCursor(
    ScreenEdge shelf_edge,
    const gfx::Point& cursor) const {
  const gfx::Rect& work_area = display_.work_area();
  gfx::Point anchor;
  switch (shelf_edge) {
    case SCREEN_EDGE_LEFT:
      anchor = gfx::Point(work_area.x(), cursor.y());
      break;
    case SCREEN_EDGE_RIGHT:
      anchor = gfx::Point(work_area.right(), cursor.y());
      break;
    case SCREEN_EDGE_TOP:
      anchor = gfx::Point(cursor.x(), work_area.y());
      break;
    case SCREEN_EDGE_BOTTOM:
      anchor = gfx::Point(cursor.x(), work_area.bottom());
      break;
    default:
      NOTREACHED();
      anchor = gfx::Point();
  }
  return ClampAnchorPoint(anchor);
}

AppListPositioner::ScreenEdge AppListPositioner::GetShelfEdge(
    const gfx::Rect& shelf_rect) const {
  const gfx::Rect& screen_rect = display_.bounds();
  const gfx::Rect& work_area = display_.work_area();

  // If we can't find the shelf, return SCREEN_EDGE_UNKNOWN. If the display
  // size is the same as the work area, and does not contain the shelf, either
  // the shelf is hidden or on another monitor.
  if (work_area == screen_rect && !work_area.Contains(shelf_rect))
    return SCREEN_EDGE_UNKNOWN;

  // Note: On Windows 8 the work area won't include split windows on the left or
  // right, and neither will |shelf_rect|.
  if (shelf_rect.x() == work_area.x() &&
      shelf_rect.width() == work_area.width()) {
    // Shelf is horizontal.
    if (shelf_rect.bottom() == screen_rect.bottom())
      return SCREEN_EDGE_BOTTOM;
    else if (shelf_rect.y() == screen_rect.y())
      return SCREEN_EDGE_TOP;
  } else if (shelf_rect.y() == work_area.y() &&
             shelf_rect.height() == work_area.height()) {
    // Shelf is vertical.
    if (shelf_rect.x() == screen_rect.x())
      return SCREEN_EDGE_LEFT;
    else if (shelf_rect.right() == screen_rect.right())
      return SCREEN_EDGE_RIGHT;
  }

  return SCREEN_EDGE_UNKNOWN;
}

int AppListPositioner::GetCursorDistanceFromShelf(
    ScreenEdge shelf_edge,
    const gfx::Point& cursor) const {
  const gfx::Rect& work_area = display_.work_area();
  switch (shelf_edge) {
    case SCREEN_EDGE_UNKNOWN:
      return 0;
    case SCREEN_EDGE_LEFT:
      return std::max(0, cursor.x() - work_area.x());
    case SCREEN_EDGE_RIGHT:
      return std::max(0, work_area.right() - cursor.x());
    case SCREEN_EDGE_TOP:
      return std::max(0, cursor.y() - work_area.y());
    case SCREEN_EDGE_BOTTOM:
      return std::max(0, work_area.bottom() - cursor.y());
    default:
      NOTREACHED();
      return 0;
  }
}

gfx::Point AppListPositioner::ClampAnchorPoint(gfx::Point anchor) const {
  gfx::Rect bounds_rect(display_.work_area());

  // Anchor the center of the window in a region that prevents the window
  // showing outside of the work area.
  bounds_rect.Inset(window_size_.width() / 2 + min_distance_from_edge_,
                    window_size_.height() / 2 + min_distance_from_edge_);

  anchor.SetToMax(bounds_rect.origin());
  anchor.SetToMin(bounds_rect.bottom_right());
  return anchor;
}
