// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/win/app_list_win.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
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

AppListWin::AppListWin(app_list::AppListView* view,
                       const base::Closure& on_should_dismiss)
    : view_(view),
      activation_tracker_(view, on_should_dismiss),
      window_icon_updated_(false) {}

AppListWin::~AppListWin() {}

gfx::Point AppListWin::FindAnchorPoint(const gfx::Size& view_size,
                                       const gfx::Display& display,
                                       const gfx::Point& cursor,
                                       const gfx::Rect& taskbar_rect) {
  AppListPositioner positioner(display, view_size, kMinDistanceFromEdge);

  // Subtract the taskbar area since the display's default work_area will not
  // subtract it if the taskbar is set to auto-hide, and the app list should
  // never overlap the taskbar.
  positioner.WorkAreaSubtract(taskbar_rect);

  // The experimental app list is placed in the center of the screen.
  if (app_list::switches::IsExperimentalAppListPositionEnabled())
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
  gfx::Rect taskbar_rect;
  GetTaskbarRect(&taskbar_rect);
  view_->SetAnchorPoint(FindAnchorPoint(view_->GetPreferredSize(), display,
                                        cursor, taskbar_rect));
}

bool AppListWin::IsVisible() {
  return view_->GetWidget()->IsVisible();
}

void AppListWin::Prerender() {
  view_->Prerender();
}

gfx::NativeWindow AppListWin::GetWindow() {
  return view_->GetWidget()->GetNativeWindow();
}

void AppListWin::SetProfile(Profile* profile) {
  view_->SetProfileByPath(profile->GetPath());
}
