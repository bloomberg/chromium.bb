// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/win/app_list_win.h"

#include "chrome/browser/ui/app_list/app_list_positioner.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace {

static const wchar_t kTrayClassName[] = L"Shell_TrayWnd";

// If the mouse cursor is less than this distance, in pixels, away from the
// taskbar, it is considered to be in the taskbar for the purpose of anchoring.
static const int kSnapDistance = 50;

// The minimum distance, in pixels, to position the app list from the taskbar or
// edge of screen.
static const int kMinDistanceFromEdge = 3;

// Utility methods for showing the app list.
// Attempts to find the bounds of the Windows taskbar. Returns true on success.
// |rect| is in screen coordinates. If the taskbar is in autohide mode and is
// not visible, |rect| will be outside the current monitor's bounds, except for
// one pixel of overlap where the edge of the taskbar is shown.
bool GetTaskbarRect(gfx::Rect* rect) {
  HWND taskbar_hwnd = FindWindow(kTrayClassName, NULL);
  if (!taskbar_hwnd)
    return false;

  RECT win_rect;
  if (!GetWindowRect(taskbar_hwnd, &win_rect))
    return false;

  *rect = gfx::Rect(win_rect);
  return true;
}

}  // namespace

// static
gfx::Point AppListWin::FindAnchorPoint(const gfx::Size& view_size,
                                       const gfx::Display& display,
                                       const gfx::Point& cursor,
                                       const gfx::Rect& taskbar_rect,
                                       bool center_window) {
  AppListPositioner positioner(display, view_size, kMinDistanceFromEdge);

  // Subtract the taskbar area since the display's default work_area will not
  // subtract it if the taskbar is set to auto-hide, and the app list should
  // never overlap the taskbar.
  positioner.WorkAreaSubtract(taskbar_rect);

  // Special case for app list in the center of the screen.
  if (center_window)
    return positioner.GetAnchorPointForScreenCenter();

  // Find which edge of the screen the taskbar is attached to.
  AppListPositioner::ScreenEdge edge = positioner.GetShelfEdge(taskbar_rect);

  // Snap to the taskbar edge. If the cursor is greater than kSnapDistance away,
  // anchor to the corner. Otherwise, anchor to the cursor position.
  gfx::Point anchor;
  if (edge == AppListPositioner::SCREEN_EDGE_UNKNOWN) {
    // If we can't find the taskbar, snap to the bottom left.
    return positioner.GetAnchorPointForScreenCorner(
        AppListPositioner::SCREEN_CORNER_BOTTOM_LEFT);
  }

  if (positioner.GetCursorDistanceFromShelf(edge, cursor) > kSnapDistance)
    return positioner.GetAnchorPointForShelfCorner(edge);

  return positioner.GetAnchorPointForShelfCursor(edge, cursor);
}

// static
void AppListWin::MoveNearCursor(app_list::AppListView* view) {
  gfx::Screen* screen = gfx::Screen::GetScreen();
  gfx::Point cursor = screen->GetCursorScreenPoint();
  gfx::Display display = screen->GetDisplayNearestPoint(cursor);

  view->SetBubbleArrow(views::BubbleBorder::FLOAT);
  gfx::Rect taskbar_rect;
  GetTaskbarRect(&taskbar_rect);
  view->SetAnchorPoint(FindAnchorPoint(view->GetPreferredSize(),
                                       display,
                                       cursor,
                                       taskbar_rect,
                                       view->ShouldCenterWindow()));
}
