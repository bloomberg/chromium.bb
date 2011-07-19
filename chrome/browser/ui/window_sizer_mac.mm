// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/window_sizer.h"


// How much horizontal and vertical offset there is between newly
// opened windows.
const int WindowSizer::kWindowTilePixels = 22;

namespace {

class DefaultMonitorInfoProvider : public WindowSizer::MonitorInfoProvider {
 public:
  DefaultMonitorInfoProvider() { }

  // Overridden from WindowSizer::MonitorInfoProvider:
  virtual gfx::Rect GetPrimaryMonitorWorkArea() const  {
    // Primary monitor is defined as the monitor with the menubar,
    // which is always at index 0.
    NSScreen* primary = [[NSScreen screens] objectAtIndex:0];
    NSRect frame = [primary frame];
    NSRect visible_frame = [primary visibleFrame];

    // Convert coordinate systems.
    gfx::Rect rect = gfx::Rect(NSRectToCGRect(visible_frame));
    rect.set_y(frame.size.height -
               visible_frame.origin.y - visible_frame.size.height);
    return rect;
  }

  virtual gfx::Rect GetPrimaryMonitorBounds() const  {
    // Primary monitor is defined as the monitor with the menubar,
    // which is always at index 0.
    NSScreen* primary = [[NSScreen screens] objectAtIndex:0];
    return gfx::Rect(NSRectToCGRect([primary frame]));
  }

  virtual gfx::Rect GetMonitorWorkAreaMatching(
      const gfx::Rect& match_rect) const {
    NSScreen* match_screen = GetMatchingScreen(match_rect);
    return ConvertCoordinateSystem([match_screen visibleFrame]);
  }

  virtual gfx::Point GetBoundsOffsetMatching(
      const gfx::Rect& match_rect) const {
    NSScreen* match_screen = GetMatchingScreen(match_rect);
    gfx::Rect bounds = ConvertCoordinateSystem([match_screen frame]);
    gfx::Rect work_area = ConvertCoordinateSystem([match_screen visibleFrame]);
    return gfx::Point(work_area.x() - bounds.x(), work_area.y() - bounds.y());
  }

  virtual void UpdateWorkAreas();

 private:
  // Returns a pointer to the screen that most closely matches the
  // given |match_rect|.  This function currently returns the screen
  // that contains the largest amount of |match_rect| by area.
  NSScreen* GetMatchingScreen(const gfx::Rect& match_rect) const {
    // Default to the monitor with the current keyboard focus, in case
    // |match_rect| is not on any screen at all.
    NSScreen* max_screen = [NSScreen mainScreen];
    int max_area = 0;

    for (NSScreen* screen in [NSScreen screens]) {
      gfx::Rect monitor_area = ConvertCoordinateSystem([screen frame]);
      gfx::Rect intersection = monitor_area.Intersect(match_rect);
      int area = intersection.width() * intersection.height();
      if (area > max_area) {
        max_area = area;
        max_screen = screen;
      }
    }

    return max_screen;
  }

  // The lower left corner of screen 0 is always at (0, 0).  The
  // cross-platform code expects the origin to be in the upper left,
  // so we have to translate |ns_rect| to the new coordinate
  // system.
  gfx::Rect ConvertCoordinateSystem(NSRect ns_rect) const;

  DISALLOW_COPY_AND_ASSIGN(DefaultMonitorInfoProvider);
};

void DefaultMonitorInfoProvider::UpdateWorkAreas() {
  work_areas_.clear();

  for (NSScreen* screen in [NSScreen screens]) {
    gfx::Rect rect = ConvertCoordinateSystem([screen visibleFrame]);
    work_areas_.push_back(rect);
  }
}

gfx::Rect DefaultMonitorInfoProvider::ConvertCoordinateSystem(
    NSRect ns_rect) const {
  // Primary monitor is defined as the monitor with the menubar,
  // which is always at index 0.
  NSScreen* primary_screen = [[NSScreen screens] objectAtIndex:0];
  float primary_screen_height = [primary_screen frame].size.height;
  gfx::Rect rect(NSRectToCGRect(ns_rect));
  rect.set_y(primary_screen_height - rect.y() - rect.height());
  return rect;
}

}  // namespace

// static
WindowSizer::MonitorInfoProvider*
WindowSizer::CreateDefaultMonitorInfoProvider() {
  return new DefaultMonitorInfoProvider();
}

// static
gfx::Point WindowSizer::GetDefaultPopupOrigin(const gfx::Size& size) {
  NSRect work_area = [[NSScreen mainScreen] visibleFrame];
  NSRect main_area = [[[NSScreen screens] objectAtIndex:0] frame];
  NSPoint corner = NSMakePoint(NSMinX(work_area), NSMaxY(work_area));

  if (Browser* b = BrowserList::GetLastActive()) {
    NSWindow* window = b->window()->GetNativeHandle();
    NSRect window_frame = [window frame];

    // Limit to not overflow the work area right and bottom edges.
    NSPoint limit = NSMakePoint(
        std::min(NSMinX(window_frame) + kWindowTilePixels,
                 NSMaxX(work_area) - size.width()),
        std::max(NSMaxY(window_frame) - kWindowTilePixels,
                 NSMinY(work_area) + size.height()));

    // Adjust corner to now overflow the work area left and top edges, so
    // that if a popup does not fit the title-bar is remains visible.
    corner = NSMakePoint(std::max(corner.x, limit.x),
                         std::min(corner.y, limit.y));
  }

  return gfx::Point(corner.x, NSHeight(main_area) - corner.y);
}
