// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/linux/app_list_linux.h"

#include "build/build_config.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/display/screen.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/widget/widget.h"

// static
AppListPositioner::ScreenEdge AppListLinux::ShelfLocationInDisplay(
    const display::Display& display) {
  // On Linux, it is difficult to find the shelf (due to the large variety of
  // desktop environments). The shelf can usually be found on the edge where the
  // display edge and work area do not match up, but there can be more than one
  // such edge. The shelf is assumed to be on the side of the screen with the
  // largest delta between the display edge and the work area edge. Ties are
  // broken in the order: top, left, right, bottom.
  const gfx::Rect work_area = display.work_area();
  const gfx::Rect display_bounds = display.bounds();

  int winning_margin = 0;
  AppListPositioner::ScreenEdge winning_edge =
      AppListPositioner::SCREEN_EDGE_UNKNOWN;

  if (work_area.y() - display_bounds.y() > winning_margin) {
    winning_margin = work_area.y() - display_bounds.y();
    winning_edge = AppListPositioner::SCREEN_EDGE_TOP;
  }

  if (work_area.x() - display_bounds.x() > winning_margin) {
    winning_margin = work_area.x() - display_bounds.x();
    winning_edge = AppListPositioner::SCREEN_EDGE_LEFT;
  }

  if (display_bounds.right() - work_area.right() > winning_margin) {
    winning_margin = display_bounds.right() - work_area.right();
    winning_edge = AppListPositioner::SCREEN_EDGE_RIGHT;
  }

  if (display_bounds.bottom() - work_area.bottom() > winning_margin) {
    winning_margin = display_bounds.bottom() - work_area.bottom();
    winning_edge = AppListPositioner::SCREEN_EDGE_BOTTOM;
  }

  return winning_edge;
}

// static
gfx::Point AppListLinux::FindAnchorPoint(const gfx::Size& view_size,
                                         const display::Display& display,
                                         const gfx::Point& cursor,
                                         AppListPositioner::ScreenEdge edge,
                                         bool center_window) {
  AppListPositioner positioner(display, view_size, 0);

  // Special case for app list in the center of the screen.
  if (center_window)
    return positioner.GetAnchorPointForScreenCenter();

  gfx::Point anchor;
  // Snap to the shelf edge. If the cursor is greater than the window
  // width/height away, anchor to the corner. Otherwise, anchor to the cursor
  // position.
  if (edge == AppListPositioner::SCREEN_EDGE_UNKNOWN) {
    // If we can't find the shelf, snap to the top left.
    return positioner.GetAnchorPointForScreenCorner(
        AppListPositioner::SCREEN_CORNER_TOP_LEFT);
  }

  int snap_distance = edge == AppListPositioner::SCREEN_EDGE_BOTTOM ||
                              edge == AppListPositioner::SCREEN_EDGE_TOP
                          ? view_size.height()
                          : view_size.width();
  if (positioner.GetCursorDistanceFromShelf(edge, cursor) > snap_distance)
    return positioner.GetAnchorPointForShelfCorner(edge);

  return positioner.GetAnchorPointForShelfCursor(edge, cursor);
}

// static
void AppListLinux::MoveNearCursor(app_list::AppListView* view) {
  display::Screen* screen = display::Screen::GetScreen();
  gfx::Point cursor = screen->GetCursorScreenPoint();
  display::Display display = screen->GetDisplayNearestPoint(cursor);

  view->SetBubbleArrow(views::BubbleBorder::FLOAT);

  AppListPositioner::ScreenEdge edge;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // In the Unity desktop environment, special case SCREEN_EDGE_LEFT. It is
  // always on the left side in Unity, but ShelfLocationInDisplay will not
  // detect this if the shelf is hidden.
  // TODO(mgiuca): Apply this special case in Gnome Shell also. The same logic
  // applies, but we currently have no way to detect whether Gnome Shell is
  // running.
  views::LinuxUI* ui = views::LinuxUI::instance();
  if (ui && ui->UnityIsRunning())
    edge = AppListPositioner::SCREEN_EDGE_LEFT;
  else
#endif
    edge = ShelfLocationInDisplay(display);
  view->SetAnchorPoint(FindAnchorPoint(view->GetPreferredSize(),
                                       display,
                                       cursor,
                                       edge,
                                       view->ShouldCenterWindow()));
}
