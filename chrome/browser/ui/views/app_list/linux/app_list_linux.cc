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
  // Find which edge of the screen the shelf is attached to. For now, just
  // assume Ubuntu Unity (fixed to left edge).
  // TODO(mgiuca): Support other window manager configurations, and multiple
  // monitors (where the current display may not have an edge).
  AppListPositioner::ScreenEdge edge = AppListPositioner::SCREEN_EDGE_LEFT;
  view_->SetAnchorPoint(FindAnchorPoint(view_->GetPreferredSize(), display,
                                        cursor, edge));
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
