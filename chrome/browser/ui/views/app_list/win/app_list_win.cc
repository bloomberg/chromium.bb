// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/win/app_list_win.h"

#include "chrome/browser/profiles/profile.h"
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
static const int kSnapOffset = 3;

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

// Represents one of the four edges of the screen.
enum ScreenEdge {
  SCREEN_EDGE_UNKNOWN,
  SCREEN_EDGE_LEFT,
  SCREEN_EDGE_RIGHT,
  SCREEN_EDGE_TOP,
  SCREEN_EDGE_BOTTOM
};

// Determines which edge of the screen the taskbar is attached to. Returns
// SCREEN_EDGE_UNKNOWN if the taskbar is unavailable, hidden, or not on the
// current screen.
ScreenEdge FindTaskbarEdge(const gfx::Display& display,
                           const gfx::Rect& taskbar_rect) {
  const gfx::Rect& screen_rect = display.bounds();

  // If we can't find the taskbar, return SCREEN_EDGE_UNKNOWN. If the display
  // size is the same as the work area, and does not contain the taskbar, either
  // the taskbar is hidden or on another monitor.
  if (display.work_area() == screen_rect &&
      !display.work_area().Contains(taskbar_rect)) {
    return SCREEN_EDGE_UNKNOWN;
  }

  // Note: On Windows 8 the work area won't include split windows on the left or
  // right, and neither will |taskbar_rect|.
  if (taskbar_rect.width() == display.work_area().width()) {
    // Taskbar is horizontal.
    if (taskbar_rect.bottom() == screen_rect.bottom())
      return SCREEN_EDGE_BOTTOM;
    else if (taskbar_rect.y() == screen_rect.y())
      return SCREEN_EDGE_TOP;
  } else if (taskbar_rect.height() == display.work_area().height()) {
    // Taskbar is vertical.
    if (taskbar_rect.x() == screen_rect.x())
      return SCREEN_EDGE_LEFT;
    else if (taskbar_rect.right() == screen_rect.right())
      return SCREEN_EDGE_RIGHT;
  }

  return SCREEN_EDGE_UNKNOWN;
}

gfx::Point FindReferencePoint(const gfx::Display& display,
                              ScreenEdge taskbar_location,
                              const gfx::Rect& taskbar_rect,
                              const gfx::Point& cursor) {
  // Snap to the taskbar edge. If the cursor is greater than kSnapDistance away,
  // also move to the left (for horizontal taskbars) or top (for vertical).
  const gfx::Rect& screen_rect = display.bounds();
  switch (taskbar_location) {
    case SCREEN_EDGE_UNKNOWN:
      // If we can't find the taskbar, snap to the bottom left.
      return display.work_area().bottom_left();
    case SCREEN_EDGE_LEFT:
      if (cursor.x() - taskbar_rect.right() > kSnapDistance)
        return gfx::Point(taskbar_rect.right(), screen_rect.y());

      return gfx::Point(taskbar_rect.right(), cursor.y());
    case SCREEN_EDGE_RIGHT:
      if (taskbar_rect.x() - cursor.x() > kSnapDistance)
        return gfx::Point(taskbar_rect.x(), screen_rect.y());

      return gfx::Point(taskbar_rect.x(), cursor.y());
    case SCREEN_EDGE_TOP:
      if (cursor.y() - taskbar_rect.bottom() > kSnapDistance)
        return gfx::Point(screen_rect.x(), taskbar_rect.bottom());

      return gfx::Point(cursor.x(), taskbar_rect.bottom());
    case SCREEN_EDGE_BOTTOM:
      if (taskbar_rect.y() - cursor.y() > kSnapDistance)
        return gfx::Point(screen_rect.x(), taskbar_rect.y());

      return gfx::Point(cursor.x(), taskbar_rect.y());
    default:
      NOTREACHED();
      return gfx::Point();
  }
}

gfx::Point FindAnchorPoint(
    const gfx::Size view_size,
    const gfx::Display& display,
    const gfx::Point& cursor) {
  gfx::Rect bounds_rect(display.work_area());
  // Always subtract the taskbar area since work_area() will not subtract it
  // if the taskbar is set to auto-hide, and the app list should never overlap
  // the taskbar.
  gfx::Rect taskbar_rect;
  ScreenEdge taskbar_edge = SCREEN_EDGE_UNKNOWN;
  if (GetTaskbarRect(&taskbar_rect)) {
    bounds_rect.Subtract(taskbar_rect);
    // Find which edge of the screen the taskbar is attached to.
    taskbar_edge = FindTaskbarEdge(display, taskbar_rect);
  }

  bounds_rect.Inset(view_size.width() / 2 + kSnapOffset,
                    view_size.height() / 2 + kSnapOffset);

  gfx::Point anchor = FindReferencePoint(display, taskbar_edge, taskbar_rect,
                                         cursor);
  anchor.SetToMax(bounds_rect.origin());
  anchor.SetToMin(bounds_rect.bottom_right());
  return anchor;
}

}  // namespace

AppListWin::AppListWin(app_list::AppListView* view,
                       const base::Closure& on_should_dismiss)
  : view_(view),
    activation_tracker_(view, on_should_dismiss),
    window_icon_updated_(false) {
}

AppListWin::~AppListWin() {
}

void AppListWin::Show() {
  view_->GetWidget()->Show();
  if (!window_icon_updated_) {
    view_->GetWidget()->GetTopLevelWidget()->UpdateWindowIcon();
    window_icon_updated_ = true;
  }
  view_->GetWidget()->Activate();
}

void AppListWin::Hide() {
  view_->GetWidget()->Hide();
  activation_tracker_.OnViewHidden();
}

void AppListWin::MoveNearCursor() {
  gfx::Point cursor = gfx::Screen::GetNativeScreen()->GetCursorScreenPoint();
  gfx::Screen* screen =
      gfx::Screen::GetScreenFor(view_->GetWidget()->GetNativeView());
  gfx::Display display = screen->GetDisplayNearestPoint(cursor);

  view_->SetBubbleArrow(views::BubbleBorder::FLOAT);
  view_->SetAnchorPoint(FindAnchorPoint(
      view_->GetPreferredSize(), display, cursor));
}

bool AppListWin::IsVisible() {
  return view_->GetWidget()->IsVisible();
}

void AppListWin::Prerender() {
  view_->Prerender();
}

void AppListWin::ReactivateOnNextFocusLoss() {
  activation_tracker_.ReactivateOnNextFocusLoss();
}

gfx::NativeWindow AppListWin::GetWindow() {
  return view_->GetWidget()->GetNativeWindow();
}

void AppListWin::SetProfile(Profile* profile) {
  view_->SetProfileByPath(profile->GetPath());
}
