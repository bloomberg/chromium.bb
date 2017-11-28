// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_window_drag_controller.h"

#include <memory>

#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/splitview/split_view_overview_overlay.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ui/aura/window.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// Returns true if |screen_orientation| is a primary orientation.
bool IsPrimaryScreenOrientation(
    blink::WebScreenOrientationLockType screen_orientation) {
  return screen_orientation ==
             blink::kWebScreenOrientationLockLandscapePrimary ||
         screen_orientation == blink::kWebScreenOrientationLockPortraitPrimary;
}

}  // namespace

OverviewWindowDragController::OverviewWindowDragController(
    WindowSelector* window_selector)
    : window_selector_(window_selector),
      split_view_controller_(Shell::Get()->split_view_controller()) {}

OverviewWindowDragController::~OverviewWindowDragController() = default;

void OverviewWindowDragController::InitiateDrag(
    WindowSelectorItem* item,
    const gfx::Point& location_in_screen) {
  item_ = item;

  previous_event_location_ = location_in_screen;
  // No need to track the initial event location if the event does not start in
  // a snap region.
  initial_event_location_ =
      GetSnapPosition(location_in_screen) == SplitViewController::NONE
          ? base::nullopt
          : base::make_optional(location_in_screen);

  window_selector_->SetSplitViewOverviewOverlayIndicatorType(
      split_view_controller_->CanSnap(item->GetWindow())
          ? IndicatorType::DRAG_AREA
          : IndicatorType::CANNOT_SNAP,
      location_in_screen);
}

void OverviewWindowDragController::Drag(const gfx::Point& location_in_screen) {
  if (!did_move_) {
    gfx::Vector2d distance = location_in_screen - previous_event_location_;
    // Do not start dragging if the distance from |location_in_screen| to
    // |previous_event_location_| is not greater than |kMinimumDragOffset|.
    if (std::abs(distance.x()) < kMinimumDragOffset &&
        std::abs(distance.y()) < kMinimumDragOffset) {
      return;
    }
  }
  did_move_ = true;

  // Update the dragged |item_|'s bounds accordingly.
  gfx::Rect bounds(item_->target_bounds());
  bounds.Offset(location_in_screen.x() - previous_event_location_.x(),
                location_in_screen.y() - previous_event_location_.y());
  item_->SetBounds(bounds, OverviewAnimationType::OVERVIEW_ANIMATION_NONE);
  previous_event_location_ = location_in_screen;

  if (ShouldUpdatePhantomWindowOrSnap(location_in_screen)) {
    UpdatePhantomWindowAndWindowGrid(location_in_screen);

    // Show the CANNOT_SNAP ui on the split view overview overlay if the window
    // cannot be snapped, otherwise show the drag ui only while the phantom
    // window is hidden.
    IndicatorType indicator_type = IndicatorType::CANNOT_SNAP;
    if (split_view_controller_->CanSnap(item_->GetWindow())) {
      indicator_type = IsPhantomWindowShowing() ? IndicatorType::NONE
                                                : IndicatorType::DRAG_AREA;
    }
    window_selector_->SetSplitViewOverviewOverlayIndicatorType(indicator_type,
                                                               gfx::Point());
  }
}

void OverviewWindowDragController::CompleteDrag(
    const gfx::Point& location_in_screen) {
  // Update window grid bounds and |snap_position_| in case the screen
  // orientation was changed.
  if (ShouldUpdatePhantomWindowOrSnap(location_in_screen))
    UpdatePhantomWindowAndWindowGrid(location_in_screen);
  phantom_window_controller_.reset();
  window_selector_->SetSplitViewOverviewOverlayIndicatorType(
      IndicatorType::NONE, gfx::Point());

  if (!did_move_) {
    ActivateDraggedWindow();
  } else {
    did_move_ = false;
    // If the window was dragged around but should not be snapped, move it back
    // to overview window grid.
    if (!ShouldUpdatePhantomWindowOrSnap(location_in_screen) ||
        snap_position_ == SplitViewController::NONE) {
      window_selector_->PositionWindows(true /* animate */);
    } else {
      SnapWindow(snap_position_);
    }
  }
}

void OverviewWindowDragController::ActivateDraggedWindow() {
  // If no drag was initiated (e.g., a click/tap on the overview window),
  // activate the window. If the split view is active and has a left window,
  // snap the current window to right. If the split view is active and has a
  // right window, snap the current window to left. If split view is active
  // and the selected window cannot be snapped, exit splitview and activate
  // the selected window, and also exit the overview.
  SplitViewController::State split_state = split_view_controller_->state();
  if (split_state == SplitViewController::NO_SNAP) {
    window_selector_->SelectWindow(item_);
  } else if (split_view_controller_->CanSnap(item_->GetWindow())) {
    SnapWindow(split_state == SplitViewController::LEFT_SNAPPED
                   ? SplitViewController::RIGHT
                   : SplitViewController::LEFT);
  } else {
    split_view_controller_->EndSplitView();
    window_selector_->SelectWindow(item_);
    split_view_controller_->ShowAppCannotSnapToast();
  }
}

void OverviewWindowDragController::ResetGesture() {
  phantom_window_controller_.reset();
  window_selector_->PositionWindows(true /* animate */);
  window_selector_->SetSplitViewOverviewOverlayIndicatorType(
      IndicatorType::NONE, gfx::Point());
}

void OverviewWindowDragController::ResetWindowSelector() {
  window_selector_ = nullptr;
}

void OverviewWindowDragController::UpdatePhantomWindowAndWindowGrid(
    const gfx::Point& location_in_screen) {
  // Attempt to show phantom window and move window grid only if the window is
  // snappable.
  if (!split_view_controller_->CanSnap(item_->GetWindow())) {
    snap_position_ = SplitViewController::NONE;
    phantom_window_controller_.reset();
    return;
  }

  SplitViewController::SnapPosition last_snap_position = snap_position_;
  snap_position_ = GetSnapPosition(location_in_screen);

  // If there is no current snapped window, update the window grid size if the
  // dragged window can be snapped if dropped.
  if (split_view_controller_->state() == SplitViewController::NO_SNAP &&
      snap_position_ != last_snap_position) {
    // Do not reposition the item that is currently being dragged.
    window_selector_->SetBoundsForWindowGridsInScreenIgnoringWindow(
        GetGridBounds(snap_position_), item_);
  }

  if (snap_position_ == SplitViewController::NONE ||
      snap_position_ != last_snap_position) {
    phantom_window_controller_.reset();
    if (snap_position_ == SplitViewController::NONE)
      return;
  }

  aura::Window* target_window = item_->GetWindow();
  gfx::Rect phantom_bounds_in_screen =
      split_view_controller_->GetSnappedWindowBoundsInScreen(target_window,
                                                             snap_position_);

  if (!phantom_window_controller_) {
    phantom_window_controller_ =
        std::make_unique<PhantomWindowController>(target_window);
  }
  phantom_window_controller_->Show(phantom_bounds_in_screen);
}

bool OverviewWindowDragController::ShouldUpdatePhantomWindowOrSnap(
    const gfx::Point& event_location) {
  if (initial_event_location_ == base::nullopt)
    return true;

  auto snap_position = GetSnapPosition(event_location);
  if (snap_position == SplitViewController::NONE) {
    // If the event started in a snap region, but has since moved out set
    // |initial_event_location_| to |event_location| which is guarenteed to not
    // be in a snap region so that the phantom window or snap mechanism works
    // normally for the rest of the drag.
    initial_event_location_ = base::nullopt;
    return true;
  }

  // The phantom window can update or the item can snap even if the drag events
  // are in the snap region, if the event has traveled past the threshold in the
  // direction of the attempted snap region.
  const gfx::Vector2d distance = event_location - *initial_event_location_;
  // Check the x-axis distance for landscape, y-axis distance for portrait.
  int distance_scalar =
      split_view_controller_->IsCurrentScreenOrientationLandscape()
          ? distance.x()
          : distance.y();

  // If the snap region is physically on the left/top side of the device, check
  // that |distance_scalar| is less than -|threshold|. If the snap region is
  // physically on the right/bottom side of the device, check that
  // |distance_scalar| is greater than |threshold|. Note: in some orientations
  // SplitViewController::LEFT is not physically on the left/top.
  const int threshold =
      OverviewWindowDragController::kMinimumDragOffsetAlreadyInSnapRegionDp;
  const bool inverted = !SplitViewController::IsLeftWindowOnTopOrLeftOfScreen(
      Shell::Get()->screen_orientation_controller()->GetCurrentOrientation());
  const bool on_the_left_or_top =
      (!inverted && snap_position == SplitViewController::LEFT) ||
      (inverted && snap_position == SplitViewController::RIGHT);

  return on_the_left_or_top ? distance_scalar <= -threshold
                            : distance_scalar >= threshold;
}

SplitViewController::SnapPosition OverviewWindowDragController::GetSnapPosition(
    const gfx::Point& location_in_screen) const {
  DCHECK(item_);
  gfx::Rect area(
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(item_->GetWindow()));
  ::wm::ConvertRectToScreen(item_->GetWindow()->GetRootWindow(), &area);

  blink::WebScreenOrientationLockType screen_orientation =
      Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();
  switch (screen_orientation) {
    case blink::kWebScreenOrientationLockLandscapePrimary:
    case blink::kWebScreenOrientationLockLandscapeSecondary:
      area.Inset(kScreenEdgeInsetForDrag, 0);
      if (location_in_screen.x() <= area.x())
        return IsPrimaryScreenOrientation(screen_orientation)
                   ? SplitViewController::LEFT
                   : SplitViewController::RIGHT;
      if (location_in_screen.x() >= area.right() - 1)
        return IsPrimaryScreenOrientation(screen_orientation)
                   ? SplitViewController::RIGHT
                   : SplitViewController::LEFT;
      return SplitViewController::NONE;

    case blink::kWebScreenOrientationLockPortraitPrimary:
    case blink::kWebScreenOrientationLockPortraitSecondary:
      area.Inset(0, kScreenEdgeInsetForDrag);
      if (location_in_screen.y() <= area.y())
        return IsPrimaryScreenOrientation(screen_orientation)
                   ? SplitViewController::RIGHT
                   : SplitViewController::LEFT;
      if (location_in_screen.y() >= area.bottom() - 1)
        return IsPrimaryScreenOrientation(screen_orientation)
                   ? SplitViewController::LEFT
                   : SplitViewController::RIGHT;
      return SplitViewController::NONE;

    default:
      NOTREACHED();
      return SplitViewController::NONE;
  }
}

gfx::Rect OverviewWindowDragController::GetGridBounds(
    SplitViewController::SnapPosition snap_position) {
  aura::Window* pending_snapped_window = item_->GetWindow();
  switch (snap_position) {
    case SplitViewController::NONE:
      return gfx::Rect(split_view_controller_->GetDisplayWorkAreaBoundsInParent(
          pending_snapped_window));
    case SplitViewController::LEFT:
      return split_view_controller_->GetSnappedWindowBoundsInScreen(
          pending_snapped_window, SplitViewController::RIGHT);
    case SplitViewController::RIGHT:
      return split_view_controller_->GetSnappedWindowBoundsInScreen(
          pending_snapped_window, SplitViewController::LEFT);
  }

  NOTREACHED();
  return gfx::Rect();
}

void OverviewWindowDragController::SnapWindow(
    SplitViewController::SnapPosition snap_position) {
  DCHECK_NE(snap_position, SplitViewController::NONE);

  item_->EnsureVisible();
  item_->RestoreWindow();

  // |item_| will be deleted after RemoveWindowSelectorItem().
  aura::Window* window = item_->GetWindow();
  window_selector_->RemoveWindowSelectorItem(item_);
  split_view_controller_->SnapWindow(window, snap_position);
  item_ = nullptr;
}

}  // namespace ash
