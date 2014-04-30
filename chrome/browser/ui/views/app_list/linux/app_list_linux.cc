// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_list/linux/app_list_linux.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_positioner.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/gfx/screen.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/widget/widget.h"

AppListLinux::AppListLinux(app_list::AppListView* view,
                           const base::Closure& on_should_dismiss)
    : view_(view),
      window_icon_updated_(false),
      on_should_dismiss_(on_should_dismiss) {
  view_->AddObserver(this);
}

AppListLinux::~AppListLinux() {
  view_->RemoveObserver(this);
}

// static
AppListPositioner::ScreenEdge AppListLinux::ShelfLocationInDisplay(
    const gfx::Display& display) {
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
                                         const gfx::Display& display,
                                         const gfx::Point& cursor,
                                         AppListPositioner::ScreenEdge edge) {
  AppListPositioner positioner(display, view_size, 0);

  // The experimental app list is placed in the center of the screen.
  if (app_list::switches::IsExperimentalAppListPositionEnabled())
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

void AppListLinux::Show() {
  view_->GetWidget()->Show();
  if (!window_icon_updated_) {
    view_->GetWidget()->GetTopLevelWidget()->UpdateWindowIcon();
    window_icon_updated_ = true;
  }
  view_->GetWidget()->Activate();
}

void AppListLinux::Hide() {
  view_->GetWidget()->Hide();
}

void AppListLinux::MoveNearCursor() {
  gfx::Point cursor = gfx::Screen::GetNativeScreen()->GetCursorScreenPoint();
  gfx::Screen* screen =
      gfx::Screen::GetScreenFor(view_->GetWidget()->GetNativeView());
  gfx::Display display = screen->GetDisplayNearestPoint(cursor);

  view_->SetBubbleArrow(views::BubbleBorder::FLOAT);

  // In the Unity desktop environment, special case SCREEN_EDGE_LEFT. It is
  // always on the left side in Unity, but ShelfLocationInDisplay will not
  // detect this if the shelf is hidden.
  // TODO(mgiuca): Apply this special case in Gnome Shell also. The same logic
  // applies, but we currently have no way to detect whether Gnome Shell is
  // running.
  views::LinuxUI* ui = views::LinuxUI::instance();
  AppListPositioner::ScreenEdge edge;
  if (ui && ui->UnityIsRunning())
    edge = AppListPositioner::SCREEN_EDGE_LEFT;
  else
    edge = ShelfLocationInDisplay(display);
  view_->SetAnchorPoint(
      FindAnchorPoint(view_->GetPreferredSize(), display, cursor, edge));
}

bool AppListLinux::IsVisible() {
  return view_->GetWidget()->IsVisible();
}

void AppListLinux::Prerender() {
  view_->Prerender();
}

gfx::NativeWindow AppListLinux::GetWindow() {
  return view_->GetWidget()->GetNativeWindow();
}

void AppListLinux::SetProfile(Profile* profile) {
  view_->SetProfileByPath(profile->GetPath());
}

void AppListLinux::OnActivationChanged(
    views::Widget* /*widget*/, bool active) {
  if (active)
    return;

  // Call |on_should_dismiss_| asynchronously. This must be done asynchronously
  // or our caller will crash, as it expects the app list to remain alive.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, on_should_dismiss_);
}
