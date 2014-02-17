// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_POSITIONER_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_POSITIONER_H_

#include "ui/gfx/display.h"
#include "ui/gfx/size.h"

namespace gfx {
class Point;
class Rect;
}

// Helps anchor the App List, onto the shelf (taskbar, dock or similar) or to
// the corner of a display. This class does not impose any particular policy for
// when and how to anchor the window. The platform-specific code that uses this
// class is free to decide, for example, when the window should be anchored to
// the cursor versus the corner of the shelf. This class just performs the
// calculations necessary to position the App List correctly.
class AppListPositioner {
 public:
  // Represents one of the four edges of the screen.
  enum ScreenEdge {
    SCREEN_EDGE_UNKNOWN,
    SCREEN_EDGE_LEFT,
    SCREEN_EDGE_RIGHT,
    SCREEN_EDGE_TOP,
    SCREEN_EDGE_BOTTOM
  };

  // Represents one of the four corners of the screen.
  enum ScreenCorner {
    SCREEN_CORNER_TOP_LEFT,
    SCREEN_CORNER_TOP_RIGHT,
    SCREEN_CORNER_BOTTOM_LEFT,
    SCREEN_CORNER_BOTTOM_RIGHT
  };

  // The |display| pointer is borrowed, and must outlive this object's lifetime.
  // |window_size| is the size of the App List.
  // |min_distance_from_edge| is the minimum distance, in pixels, to position
  // the app list from the shelf or edge of screen.
  AppListPositioner(const gfx::Display& display,
                    const gfx::Size& window_size,
                    int min_distance_from_edge);

  // Subtracts a rectangle from the display's work area. This can be used to
  // ensure that the app list does not overlap the shelf, even in situations
  // where the shelf is considered part of the work area.
  void WorkAreaSubtract(const gfx::Rect& rect);

  // Shrinks the display's work area by the given amount on each side.
  void WorkAreaInset(int left, int top, int right, int bottom);

  // Finds the position for a window to anchor it to a corner of the screen.
  // |corner| specifies which corner to anchor the window to. Returns the
  // intended coordinates for the center of the window. This should only be used
  // when there is no shelf on the display, because if there is, the returned
  // anchor point will potentially position the window under it.
  gfx::Point GetAnchorPointForScreenCorner(ScreenCorner corner) const;

  // Finds the position for a window to anchor it to the center of the screen.
  // Returns the intended coordinates for the center of the window.
  gfx::Point GetAnchorPointForScreenCenter() const;

  // Finds the position for a window to anchor it to the corner of the shelf.
  // The window will be aligned to the left of the work area for horizontal
  // shelves, or to the top for vertical shelves. |shelf_edge| specifies the
  // location of the shelf. |shelf_edge| must not be SCREEN_EDGE_UNKNOWN.
  // Returns the intended coordinates for the center of the window.
  gfx::Point GetAnchorPointForShelfCorner(ScreenEdge shelf_edge) const;

  // Finds the position for a window to anchor it to the center of the shelf.
  // |shelf_edge| specifies the location of the shelf. It must not be
  // SCREEN_EDGE_UNKNOWN. Returns the intended coordinates for the center of the
  // window.
  gfx::Point GetAnchorPointForShelfCenter(ScreenEdge shelf_edge) const;

  // Finds the position for a window to anchor it to the shelf at a point
  // closest to the user's mouse cursor. |shelf_edge| specifies the location of
  // the shelf; |cursor| specifies the location of the user's mouse cursor.
  // |shelf_edge| must not be SCREEN_EDGE_UNKNOWN. Returns the intended
  // coordinates for the center of the window.
  gfx::Point GetAnchorPointForShelfCursor(ScreenEdge shelf_edge,
                                          const gfx::Point& cursor) const;

  // Determines which edge of the screen the shelf is attached to. Returns
  // SCREEN_EDGE_UNKNOWN if the shelf is unavailable, hidden, or not on the
  // current screen.
  ScreenEdge GetShelfEdge(const gfx::Rect& shelf_rect) const;

  // Gets the lateral distance of the mouse cursor from the edge of the shelf.
  // For horizontal shelves, this is the vertical distance; for vertical
  // shelves, this is the horizontal distance. If the cursor is inside the
  // shelf, returns 0.
  int GetCursorDistanceFromShelf(ScreenEdge shelf_edge,
                                 const gfx::Point& cursor) const;

 private:
  // Ensures that an anchor point will result in a window that is fully within
  // the work area. Returns the updated anchor point.
  gfx::Point ClampAnchorPoint(gfx::Point anchor) const;

  gfx::Display display_;

  // Size of the App List.
  gfx::Size window_size_;

  // The minimum distance, in pixels, to position the app list from the shelf
  // or edge of screen.
  int min_distance_from_edge_;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_POSITIONER_H_
