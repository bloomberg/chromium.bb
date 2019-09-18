// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_grid_event_handler.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_utils.h"
#include "ui/compositor/compositor.h"
#include "ui/events/event.h"
#include "ui/events/gestures/fling_curve.h"

namespace ash {

namespace {

WallpaperView* GetWallpaperViewForRoot(const aura::Window* root_window) {
  auto* wallpaper_widget_controller =
      RootWindowController::ForWindow(root_window)
          ->wallpaper_widget_controller();
  if (!wallpaper_widget_controller)
    return nullptr;
  return wallpaper_widget_controller->wallpaper_view();
}

}  // namespace

OverviewGridEventHandler::OverviewGridEventHandler(OverviewGrid* grid)
    : grid_(grid) {
  auto* wallpaper_view = GetWallpaperViewForRoot(grid_->root_window());
  if (wallpaper_view)
    wallpaper_view->AddPreTargetHandler(this);
}

OverviewGridEventHandler::~OverviewGridEventHandler() {
  EndFling();
  grid_->EndScroll();

  auto* wallpaper_view = GetWallpaperViewForRoot(grid_->root_window());
  if (wallpaper_view)
    wallpaper_view->RemovePreTargetHandler(this);
}

void OverviewGridEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_RELEASED)
    HandleClickOrTap(event);
}

void OverviewGridEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP: {
      HandleClickOrTap(event);
      break;
    }
    case ui::ET_SCROLL_FLING_START: {
      if (!ShouldUseTabletModeGridLayout())
        return;

      HandleFlingScroll(event);
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      if (!ShouldUseTabletModeGridLayout())
        return;

      EndFling();
      grid_->StartScroll();
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      if (!ShouldUseTabletModeGridLayout())
        return;

      grid_->UpdateScrollOffset(event->details().scroll_x());
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_SCROLL_END: {
      if (!ShouldUseTabletModeGridLayout())
        return;

      grid_->EndScroll();
      event->SetHandled();
      break;
    }
    default:
      break;
  }
}

void OverviewGridEventHandler::OnAnimationStep(base::TimeTicks timestamp) {
  // Updates |grid_| based on |offset| when |observed_compositor_| begins a new
  // frame.
  DCHECK(observed_compositor_);

  // As a fling progresses, the velocity degenerates, and the difference in
  // offset is passed into |grid_| as an updated scroll value. Stop flinging if
  // the API for fling says to finish, or we reach one of the edges of the
  // overview grid. Update the grid even if the API says to stop flinging as it
  // still produces a usable |offset|, but end the fling afterwards.
  gfx::Vector2dF offset;
  bool continue_fling =
      fling_curve_->ComputeScrollOffset(timestamp, &offset, &fling_velocity_);
  continue_fling = grid_->UpdateScrollOffset(
                       fling_last_offset_ ? offset.x() - fling_last_offset_->x()
                                          : offset.x()) &&
                   continue_fling;
  fling_last_offset_ = base::make_optional(offset);

  if (!continue_fling)
    EndFling();
}

void OverviewGridEventHandler::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  DCHECK_EQ(compositor, observed_compositor_);
  EndFling();
}

void OverviewGridEventHandler::HandleClickOrTap(ui::Event* event) {
  CHECK_EQ(ui::EP_PRETARGET, event->phase());
  // Events that happen while app list is sliding out during overview should
  // be ignored to prevent overview from disappearing out from under the user.
  if (!IsSlidingOutOverviewFromShelf())
    Shell::Get()->overview_controller()->EndOverview();
  event->StopPropagation();
}

void OverviewGridEventHandler::HandleFlingScroll(ui::GestureEvent* event) {
  fling_velocity_ = gfx::Vector2dF(event->details().velocity_x(),
                                   event->details().velocity_y());
  fling_curve_ =
      std::make_unique<ui::FlingCurve>(fling_velocity_, base::TimeTicks::Now());
  observed_compositor_ = const_cast<ui::Compositor*>(
      grid_->root_window()->layer()->GetCompositor());
  observed_compositor_->AddAnimationObserver(this);
}

void OverviewGridEventHandler::EndFling() {
  if (!observed_compositor_)
    return;

  observed_compositor_->RemoveAnimationObserver(this);
  observed_compositor_ = nullptr;
  fling_curve_.reset();
  fling_last_offset_ = base::nullopt;
  grid_->EndScroll();
}

}  // namespace ash
