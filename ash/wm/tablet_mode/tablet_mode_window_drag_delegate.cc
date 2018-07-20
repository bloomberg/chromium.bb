// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_drag_delegate.h"

#include "ash/shell.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"

namespace ash {

namespace {

// The threshold to compute the minimum vertical distance to start showing the
// drag indicators and preview window when dragging a window into splitscreen in
// tablet mode.
constexpr float kIndicatorsThresholdRatio = 0.1;

// The threshold to compute the vertical distance to hide the drag indicators
// and maximize the dragged window after the drag ends.
constexpr float kMaximizeThresholdRatio = 0.4;

// Returns the window selector if overview mode is active, otherwise returns
// nullptr.
WindowSelector* GetWindowSelector() {
  return Shell::Get()->window_selector_controller()->IsSelecting()
             ? Shell::Get()->window_selector_controller()->window_selector()
             : nullptr;
}

}  // namespace

TabletModeWindowDragDelegate::TabletModeWindowDragDelegate()
    : split_view_controller_(Shell::Get()->split_view_controller()),
      split_view_drag_indicators_(std::make_unique<SplitViewDragIndicators>()) {
}

TabletModeWindowDragDelegate::~TabletModeWindowDragDelegate() = default;

void TabletModeWindowDragDelegate::StartWindowDrag(
    aura::Window* dragged_window,
    const gfx::Point& location_in_screen) {
  dragged_window_ = dragged_window;

  PrepareForDraggedWindow(location_in_screen);

  // Disable the backdrop on the dragged window.
  original_backdrop_mode_ = dragged_window_->GetProperty(kBackdropWindowMode);
  dragged_window_->SetProperty(kBackdropWindowMode,
                               BackdropWindowMode::kDisabled);
  split_view_controller_->OnWindowDragStarted(dragged_window_);

  if (ShouldOpenOverviewWhenDragStarts()) {
    WindowSelectorController* controller =
        Shell::Get()->window_selector_controller();
    if (!controller->IsSelecting())
      controller->ToggleOverview();
  }

  if (GetWindowSelector())
    GetWindowSelector()->OnWindowDragStarted(dragged_window_);
}

void TabletModeWindowDragDelegate::ContinueWindowDrag(
    const gfx::Point& location_in_screen) {
  UpdateForDraggedWindow(location_in_screen);

  // Update drag indicators and preview window if necessary.
  IndicatorState indicator_state = GetIndicatorState(location_in_screen);
  split_view_drag_indicators_->SetIndicatorState(indicator_state,
                                                 location_in_screen);

  if (GetWindowSelector()) {
    GetWindowSelector()->OnWindowDragContinued(
        dragged_window_, location_in_screen, indicator_state);
  }
}

void TabletModeWindowDragDelegate::EndWindowDrag(
    wm::WmToplevelWindowEventHandler::DragResult result,
    const gfx::Point& location_in_screen) {
  EndingForDraggedWindow(result, location_in_screen);

  dragged_window_->SetProperty(kBackdropWindowMode, original_backdrop_mode_);
  SplitViewController::SnapPosition snap_position = SplitViewController::NONE;
  if (result == wm::WmToplevelWindowEventHandler::DragResult::SUCCESS &&
      split_view_controller_->CanSnap(dragged_window_)) {
    snap_position = GetSnapPosition(location_in_screen);
  }

  // The window might merge into an overview window or become a new window item
  // in overview mode.
  if (GetWindowSelector())
    GetWindowSelector()->OnWindowDragEnded(dragged_window_, location_in_screen);
  split_view_controller_->OnWindowDragEnded(dragged_window_, snap_position,
                                            location_in_screen);
  split_view_drag_indicators_->SetIndicatorState(IndicatorState::kNone,
                                                 location_in_screen);
  dragged_window_ = nullptr;
}

bool TabletModeWindowDragDelegate::ShouldOpenOverviewWhenDragStarts() {
  DCHECK(dragged_window_);
  return true;
}

int TabletModeWindowDragDelegate::GetIndicatorsVerticalThreshold(
    const gfx::Rect& work_area_bounds) const {
  return work_area_bounds.y() +
         work_area_bounds.height() * kIndicatorsThresholdRatio;
}

int TabletModeWindowDragDelegate::GetMaximizeVerticalThreshold(
    const gfx::Rect& work_area_bounds) const {
  return work_area_bounds.y() +
         work_area_bounds.height() * kMaximizeThresholdRatio;
}

SplitViewController::SnapPosition TabletModeWindowDragDelegate::GetSnapPosition(
    const gfx::Point& location_in_screen) const {
  gfx::Rect work_area_bounds = display::Screen::GetScreen()
                                   ->GetDisplayNearestWindow(dragged_window_)
                                   .work_area();

  // The user has to drag pass the indicator vertical threshold to snap the
  // window.
  if (location_in_screen.y() < GetIndicatorsVerticalThreshold(work_area_bounds))
    return SplitViewController::NONE;

  const bool is_landscape =
      split_view_controller_->IsCurrentScreenOrientationLandscape();
  const bool is_primary =
      split_view_controller_->IsCurrentScreenOrientationPrimary();

  // If split view mode is active during dragging, the dragged window will be
  // either snapped left or right (if it's not merged into overview window),
  // depending on the relative position of |location_in_screen| and the current
  // divider position.
  if (split_view_controller_->IsSplitViewModeActive()) {
    const int position =
        is_landscape ? location_in_screen.x() : location_in_screen.y();
    if (position < split_view_controller_->divider_position()) {
      return is_primary ? SplitViewController::LEFT
                        : SplitViewController::RIGHT;
    } else {
      return is_primary ? SplitViewController::RIGHT
                        : SplitViewController::LEFT;
    }
  }

  // Otherwise, check to see if the current event location |location_in_screen|
  // is within the drag indicators bounds.
  if (is_landscape) {
    const int screen_edge_inset =
        work_area_bounds.width() * kHighlightScreenPrimaryAxisRatio +
        kHighlightScreenEdgePaddingDp;
    work_area_bounds.Inset(screen_edge_inset, 0);
    if (location_in_screen.x() < work_area_bounds.x()) {
      return is_primary ? SplitViewController::LEFT
                        : SplitViewController::RIGHT;
    }
    if (location_in_screen.x() >= work_area_bounds.right()) {
      return is_primary ? SplitViewController::RIGHT
                        : SplitViewController::LEFT;
    }
    return SplitViewController::NONE;
  }
  // For portrait mode, since the drag always starts from the top of the
  // screen, we only allow the window to be dragged to snap to the bottom of
  // the screen.
  const int screen_edge_inset =
      work_area_bounds.height() * kHighlightScreenPrimaryAxisRatio +
      kHighlightScreenEdgePaddingDp;
  work_area_bounds.Inset(0, screen_edge_inset);
  if (location_in_screen.y() >= work_area_bounds.bottom())
    return is_primary ? SplitViewController::RIGHT : SplitViewController::LEFT;

  return SplitViewController::NONE;
}

IndicatorState TabletModeWindowDragDelegate::GetIndicatorState(
    const gfx::Point& location_in_screen) const {
  SplitViewController::SnapPosition snap_position =
      GetSnapPosition(location_in_screen);
  const bool can_snap = split_view_controller_->CanSnap(dragged_window_);
  if (snap_position != SplitViewController::NONE &&
      !split_view_controller_->IsSplitViewModeActive() && can_snap) {
    return snap_position == SplitViewController::LEFT
               ? IndicatorState::kPreviewAreaLeft
               : IndicatorState::kPreviewAreaRight;
  }

  // Do not show the drag indicators if split view mode is active.
  if (split_view_controller_->IsSplitViewModeActive())
    return IndicatorState::kNone;

  // If the event location hasn't passed the indicator vertical threshold, do
  // not show the drag indicators.
  gfx::Rect work_area_bounds = display::Screen::GetScreen()
                                   ->GetDisplayNearestWindow(dragged_window_)
                                   .work_area();
  if (location_in_screen.y() < GetIndicatorsVerticalThreshold(work_area_bounds))
    return IndicatorState::kNone;

  // If the event location has passed the maximize vertical threshold, and the
  // event location is not in snap indicator area, and overview mode is not
  // active at the moment, do not show the drag indicators.
  if (location_in_screen.y() >=
          GetMaximizeVerticalThreshold(work_area_bounds) &&
      snap_position == SplitViewController::NONE &&
      !Shell::Get()->window_selector_controller()->IsSelecting()) {
    return IndicatorState::kNone;
  }

  return can_snap ? IndicatorState::kDragArea : IndicatorState::kCannotSnap;
}

}  // namespace ash
