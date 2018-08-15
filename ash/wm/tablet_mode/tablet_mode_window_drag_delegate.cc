// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_drag_delegate.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ui/gfx/transform_util.h"
#include "ui/wm/core/coordinate_conversion.h"

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

// Gets the new selector item's bounds in overview grid that is displaying in
// the same root window as |dragged_window|.
gfx::Rect GetNewSelectorItemBounds(aura::Window* dragged_window) {
  if (!Shell::Get()->window_selector_controller()->IsSelecting())
    return gfx::Rect();

  WindowGrid* window_grid = GetWindowSelector()->GetGridWithRootWindow(
      dragged_window->GetRootWindow());
  if (!window_grid || window_grid->empty())
    return gfx::Rect();

  WindowSelectorItem* new_selector_item =
      window_grid->window_list().front().get();
  if (!window_grid->IsNewSelectorItemWindow(new_selector_item->GetWindow()))
    return gfx::Rect();

  return new_selector_item->target_bounds();
}

// Set |transform| to |window| and its transient child windows. |transform| is
// the transform that applies to |window| and needes to be adjusted for the
// transient child windows.
void SetTransform(aura::Window* window, const gfx::Transform& transform) {
  gfx::Point target_origin(window->GetTargetBounds().origin());
  for (auto* window_iter : wm::GetTransientTreeIterator(window)) {
    aura::Window* parent_window = window_iter->parent();
    gfx::Rect original_bounds(window_iter->GetTargetBounds());
    ::wm::ConvertRectToScreen(parent_window, &original_bounds);
    gfx::Transform new_transform =
        TransformAboutPivot(gfx::Point(target_origin.x() - original_bounds.x(),
                                       target_origin.y() - original_bounds.y()),
                            transform);
    window_iter->SetTransform(new_transform);
  }
}

}  // namespace

TabletModeWindowDragDelegate::TabletModeWindowDragDelegate()
    : split_view_controller_(Shell::Get()->split_view_controller()),
      split_view_drag_indicators_(std::make_unique<SplitViewDragIndicators>()),
      weak_ptr_factory_(this) {}

TabletModeWindowDragDelegate::~TabletModeWindowDragDelegate() = default;

void TabletModeWindowDragDelegate::StartWindowDrag(
    aura::Window* dragged_window,
    const gfx::Point& location_in_screen) {
  dragged_window_ = dragged_window;
  initial_location_in_screen_ = location_in_screen;

  PrepareForDraggedWindow(location_in_screen);

  // Update the shelf's visibility.
  RootWindowController::ForWindow(dragged_window_)
      ->GetShelfLayoutManager()
      ->UpdateVisibilityState();

  // Disable the backdrop on the dragged window.
  original_backdrop_mode_ = dragged_window_->GetProperty(kBackdropWindowMode);
  dragged_window_->SetProperty(kBackdropWindowMode,
                               BackdropWindowMode::kDisabled);

  WindowSelectorController* controller =
      Shell::Get()->window_selector_controller();
  bool was_overview_open = controller->IsSelecting();

  // If the dragged window is one of the snapped windows, SplitViewController
  // might open overview in the dragged window side of the screen.
  split_view_controller_->OnWindowDragStarted(dragged_window_);

  if (ShouldOpenOverviewWhenDragStarts() && !controller->IsSelecting())
    controller->ToggleOverview();

  if (controller->IsSelecting()) {
    // Only do animation if overview was open before the drag started. If the
    // overview is opened because of the window drag, do not do animation.
    GetWindowSelector()->OnWindowDragStarted(dragged_window_,
                                             /*animate=*/was_overview_open);
  }

  new_selector_item_bounds_ = GetNewSelectorItemBounds(dragged_window_);
}

void TabletModeWindowDragDelegate::ContinueWindowDrag(
    const gfx::Point& location_in_screen,
    UpdateDraggedWindowType type,
    const gfx::Rect& target_bounds) {
  if (!did_move_) {
    const gfx::Rect work_area_bounds =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(dragged_window_)
            .work_area();
    if (location_in_screen.y() >=
        GetIndicatorsVerticalThreshold(work_area_bounds)) {
      did_move_ = true;
    }
  }

  if (type == UpdateDraggedWindowType::UPDATE_BOUNDS) {
    // UPDATE_BOUNDS is used when dragging tab(s) out of a browser window.
    // Changing bounds might delete ourselves as the dragged (browser) window
    // tab(s) might merge into another browser window.
    base::WeakPtr<TabletModeWindowDragDelegate> delegate(
        weak_ptr_factory_.GetWeakPtr());
    if (target_bounds != dragged_window_->bounds()) {
      dragged_window_->SetBounds(target_bounds);
      if (!delegate)
        return;
    }
  } else {  // type == UpdateDraggedWindowType::UPDATE_TRANSFORM
    // UPDATE_TRANSFORM is used when dragging an entire window around, either
    // it's a browser window or an app window.
    UpdateDraggedWindowTransform(location_in_screen);
  }

  // For child classes to do their special handling if any.
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

  // If |dragged_window_|'s transform has changed during dragging, and it has
  // not been added into overview grid, restore its transform to identity.
  if (!dragged_window_->layer()->GetTargetTransform().IsIdentity() &&
      (!GetWindowSelector() ||
       !GetWindowSelector()->IsWindowInOverview(dragged_window_))) {
    SetTransform(dragged_window_, gfx::Transform());
  }

  dragged_window_ = nullptr;
  did_move_ = false;
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
  if (!did_move_ && location_in_screen.y() <
                        GetIndicatorsVerticalThreshold(work_area_bounds)) {
    return SplitViewController::NONE;
  }

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
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(dragged_window_)
          .work_area();
  if (!did_move_ && location_in_screen.y() <
                        GetIndicatorsVerticalThreshold(work_area_bounds)) {
    return IndicatorState::kNone;
  }

  // If the event location has passed the maximize vertical threshold, and the
  // event location is not in snap indicator area, and overview mode is not
  // active at the moment, do not show the drag indicators.
  if (location_in_screen.y() >=
          GetMaximizeVerticalThreshold(work_area_bounds) &&
      snap_position == SplitViewController::NONE &&
      !Shell::Get()->window_selector_controller()->IsSelecting()) {
    return IndicatorState::kNone;
  }

  // No top drag indicator if in portrait screen orientation.
  if (split_view_controller_->IsCurrentScreenOrientationLandscape())
    return can_snap ? IndicatorState::kDragArea : IndicatorState::kCannotSnap;

  return can_snap ? IndicatorState::kDragAreaRight
                  : IndicatorState::kCannotSnapRight;
}

void TabletModeWindowDragDelegate::UpdateDraggedWindowTransform(
    const gfx::Point& location_in_screen) {
  DCHECK(Shell::Get()->window_selector_controller()->IsSelecting());

  // Calculate the desired scale along the y-axis. The scale of the window
  // during drag is based on the distance from |y_location_in_screen| to the y
  // position of |new_selector_item_bounds_|. The dragged window will become
  // smaller when it becomes nearer to the new selector item. And then keep the
  // minimum scale if it has been dragged further than the new selector item.
  float scale = static_cast<float>(new_selector_item_bounds_.height()) /
                static_cast<float>(dragged_window_->bounds().height());
  int y_full = new_selector_item_bounds_.y();
  int y_diff = y_full - location_in_screen.y();
  if (y_diff >= 0)
    scale = (1.0f - scale) * y_diff / y_full + scale;

  gfx::Transform transform;
  const gfx::Rect window_bounds = dragged_window_->bounds();
  transform.Translate(
      (location_in_screen.x() - window_bounds.x()) -
          (initial_location_in_screen_.x() - window_bounds.x()) * scale,
      (location_in_screen.y() - window_bounds.y()) -
          (initial_location_in_screen_.y() - window_bounds.y()) * scale);
  transform.Scale(scale, scale);
  SetTransform(dragged_window_, transform);
}

}  // namespace ash
