// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_controller.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/wm/drag_details.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_window_drag_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The scale factor that the source window should scale if the source window is
// not the dragge window && is not in splitscreen when drag starts && the user
// has dragged the window to pass the |kIndicatorThresholdRatio| vertical
// threshold.
constexpr float kSourceWindowScale = 0.85;

// Values of the scrim.
constexpr SkColor kScrimBackgroundColor = SK_ColorGRAY;
constexpr float kScrimOpacity = 0.8;
constexpr float kScrimBlur = 5.f;
constexpr int kScrimTransitionInMs = 250;
constexpr int kScrimRoundRectRadiusDp = 4;

// The threshold to compute the vertical distance to create/reset the scrim. The
// scrim is placed below the current dragged window. This value is smaller than
// kIndicatorsThreshouldRatio to prevent the dragged window to merge back to
// its source window during its source window's animation.
constexpr float kScrimResetThresholdRatio = 0.05;

// Returns the window selector if overview mode is active, otherwise returns
// nullptr.
WindowSelector* GetWindowSelector() {
  return Shell::Get()->window_selector_controller()->IsSelecting()
             ? Shell::Get()->window_selector_controller()->window_selector()
             : nullptr;
}

// Creates a transparent scrim which is placed below |dragged_window|.
std::unique_ptr<views::Widget> CreateScrim(aura::Window* dragged_window,
                                           const gfx::Rect& bounds) {
  std::unique_ptr<views::Widget> widget = CreateBackgroundWidget(
      /*root_window=*/dragged_window->GetRootWindow(),
      /*layer_type=*/ui::LAYER_TEXTURED,
      /*background_color=*/kScrimBackgroundColor,
      /*border_thickness=*/0,
      /*border_radius=*/kScrimRoundRectRadiusDp,
      /*border_color=*/SK_ColorTRANSPARENT,
      /*initial_opacity=*/0.f,
      /*parent=*/dragged_window->parent(),
      /*stack_on_top=*/false);
  widget->SetBounds(bounds);
  return widget;
}

// When the dragged window is dragged past this value, a transparent scrim will
// be created to place below the current dragged window to prevent the dragged
// window to merge into any browser window beneath it and when it's dragged back
// toward the top of the screen, the scrim will be destroyed.
int GetScrimVerticalThreshold(const gfx::Rect& work_area_bounds) {
  return work_area_bounds.y() +
         work_area_bounds.height() * kScrimResetThresholdRatio;
}

}  // namespace

TabletModeBrowserWindowDragController::TabletModeBrowserWindowDragController(
    wm::WindowState* window_state)
    : WindowResizer(window_state),
      drag_delegate_(std::make_unique<TabletModeWindowDragDelegate>()),
      weak_ptr_factory_(this) {
  DCHECK(details().is_resizable);
  DCHECK(!window_state->allow_set_bounds_direct());

  if (details().source != ::wm::WINDOW_MOVE_SOURCE_TOUCH) {
    ShellPort::Get()->LockCursor();
    did_lock_cursor_ = true;
  }

  previous_location_in_parent_ = details().initial_location_in_parent;
  window_state_->OnDragStarted(details().window_component);

  drag_delegate_->OnWindowDragStarted(GetTarget());

  // If the source window itself is the dragged window (i.e., all the source
  // window's tabs are dragged together when drag starts), open overview behind
  // the dragged window.
  aura::Window* source_window =
      GetTarget()->GetProperty(ash::kTabDraggingSourceWindowKey);
  if (!source_window &&
      !Shell::Get()->window_selector_controller()->IsSelecting()) {
    Shell::Get()->window_selector_controller()->ToggleOverview();
  }

  if (GetWindowSelector())
    GetWindowSelector()->OnWindowDragStarted(GetTarget());
}

TabletModeBrowserWindowDragController::
    ~TabletModeBrowserWindowDragController() {
  if (did_lock_cursor_)
    ShellPort::Get()->UnlockCursor();
}

void TabletModeBrowserWindowDragController::Drag(
    const gfx::Point& location_in_parent,
    int event_flags) {
  gfx::Point location_in_screen = location_in_parent;
  ::wm::ConvertPointToScreen(drag_delegate_->dragged_window()->parent(),
                             &location_in_screen);

  // Update the drag indicators and snap preview window if necessary.
  drag_delegate_->UpdateIndicatorState(location_in_screen);

  // Update the source window if necessary.
  UpdateSourceWindow(location_in_screen);

  // Update the scrim that beneath the dragged window if necessary.
  UpdateScrim(location_in_screen);

  // Update dragged window's bounds.
  gfx::Rect bounds = CalculateBoundsForDrag(location_in_parent);
  if (bounds != drag_delegate_->dragged_window()->bounds()) {
    base::WeakPtr<TabletModeBrowserWindowDragController> resizer(
        weak_ptr_factory_.GetWeakPtr());
    drag_delegate_->dragged_window()->SetBounds(bounds);
    if (!resizer)
      return;
  }

  previous_location_in_parent_ = location_in_parent;

  if (GetWindowSelector())
    GetWindowSelector()->OnWindowDragContinued(drag_delegate_->dragged_window(),
                                               location_in_screen);
}

void TabletModeBrowserWindowDragController::CompleteDrag() {
  EndDragImpl(wm::WmToplevelWindowEventHandler::DragResult::SUCCESS);
}

void TabletModeBrowserWindowDragController::RevertDrag() {
  EndDragImpl(wm::WmToplevelWindowEventHandler::DragResult::REVERT);
}

void TabletModeBrowserWindowDragController::EndDragImpl(
    wm::WmToplevelWindowEventHandler::DragResult result) {
  gfx::Point previous_location_in_screen = previous_location_in_parent_;
  ::wm::ConvertPointToScreen(drag_delegate_->dragged_window()->parent(),
                             &previous_location_in_screen);
  if (result == wm::WmToplevelWindowEventHandler::DragResult::SUCCESS)
    window_state_->OnCompleteDrag(previous_location_in_screen);
  else
    window_state_->OnRevertDrag(previous_location_in_screen);

  // The window might merge into an overview window or become a new window item
  // in overview mode.
  if (GetWindowSelector()) {
    GetWindowSelector()->OnWindowDragEnded(drag_delegate_->dragged_window(),
                                           previous_location_in_screen);
  }

  // The source window might have been scaled during dragging, update its bounds
  // to ensure it has the right bounds after the drag ends.
  aura::Window* source_window = drag_delegate_->dragged_window()->GetProperty(
      ash::kTabDraggingSourceWindowKey);
  if (source_window && !wm::GetWindowState(source_window)->IsSnapped()) {
    TabletModeWindowState::UpdateWindowPosition(
        wm::GetWindowState(source_window));
  }
  drag_delegate_->OnWindowDragEnded(result, previous_location_in_screen);
}

void TabletModeBrowserWindowDragController::UpdateSourceWindow(
    const gfx::Point& location_in_screen) {
  // Only do the scale if the source window is not the dragged window && the
  // source window is not in splitscreen && the source window is not in
  // overview.
  aura::Window* source_window = drag_delegate_->dragged_window()->GetProperty(
      ash::kTabDraggingSourceWindowKey);
  if (!source_window || source_window == drag_delegate_->dragged_window() ||
      source_window == drag_delegate_->split_view_controller()->left_window() ||
      source_window ==
          drag_delegate_->split_view_controller()->right_window() ||
      (GetWindowSelector() &&
       GetWindowSelector()->IsWindowInOverview(source_window))) {
    return;
  }

  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(drag_delegate_->dragged_window())
          .work_area();
  gfx::Rect expected_bounds(work_area_bounds);
  if (location_in_screen.y() >=
      TabletModeWindowDragDelegate::GetIndicatorsVerticalThreshold(
          work_area_bounds)) {
    SplitViewController::SnapPosition snap_position =
        drag_delegate_->GetSnapPosition(location_in_screen);

    if (snap_position == SplitViewController::NONE) {
      // Scale down the source window if the event location passes the vertical
      // |kIndicatorThresholdRatio| threshold.
      expected_bounds.ClampToCenteredSize(
          gfx::Size(work_area_bounds.width() * kSourceWindowScale,
                    work_area_bounds.height() * kSourceWindowScale));
    } else {
      // Put the source window on the other side of the split screen.
      expected_bounds =
          drag_delegate_->split_view_controller()
              ->GetSnappedWindowBoundsInScreen(
                  source_window, snap_position == SplitViewController::LEFT
                                     ? SplitViewController::RIGHT
                                     : SplitViewController::LEFT);
    }
  }

  source_window->SetBoundsInScreen(
      expected_bounds,
      display::Screen::GetScreen()->GetDisplayNearestWindow(source_window));
}

void TabletModeBrowserWindowDragController::UpdateScrim(
    const gfx::Point& location_in_screen) {
  const gfx::Rect work_area_bounds = display::Screen::GetScreen()
                                         ->GetDisplayNearestWindow(GetTarget())
                                         .work_area();
  if (location_in_screen.y() < GetScrimVerticalThreshold(work_area_bounds)) {
    // Remove |scrim_| entirely so that the dragged window can be merged back
    // to the source window when the dragged window is dragged back toward the
    // top area of the screen.
    scrim_.reset();
    return;
  }

  // If overview mode is active, do not show the scrim on the overview side of
  // the screen.
  if (Shell::Get()->window_selector_controller()->IsSelecting()) {
    WindowGrid* window_grid = GetWindowSelector()->GetGridWithRootWindow(
        GetTarget()->GetRootWindow());
    if (window_grid && window_grid->bounds().Contains(location_in_screen)) {
      scrim_.reset();
      return;
    }
  }

  SplitViewController::SnapPosition snap_position =
      drag_delegate_->GetSnapPosition(location_in_screen);
  gfx::Rect expected_bounds(work_area_bounds);
  if (drag_delegate_->split_view_controller()->IsSplitViewModeActive()) {
    expected_bounds =
        drag_delegate_->split_view_controller()->GetSnappedWindowBoundsInScreen(
            GetTarget(), snap_position);
  } else {
    expected_bounds.Inset(kHighlightScreenEdgePaddingDp,
                          kHighlightScreenEdgePaddingDp);
  }

  bool should_show_blurred_scrim = false;
  if (location_in_screen.y() >=
      TabletModeWindowDragDelegate::GetMaximizeVerticalThreshold(
          work_area_bounds)) {
    if (drag_delegate_->split_view_controller()->IsSplitViewModeActive() !=
        (snap_position == SplitViewController::NONE)) {
      should_show_blurred_scrim = true;
    }
  }
  // When the event is between |indicators_vertical_threshold| and
  // |maximize_vertical_threshold|, the scrim is still shown but is invisible
  // to the user (transparent). It's needed to prevent the dragged window to
  // merge into the scaled down source window.
  ShowScrim(should_show_blurred_scrim ? kScrimOpacity : 0.f,
            should_show_blurred_scrim ? kScrimBlur : 0.f, expected_bounds);
}

void TabletModeBrowserWindowDragController::ShowScrim(
    float opacity,
    float blur,
    const gfx::Rect& bounds_in_screen) {
  gfx::Rect bounds(bounds_in_screen);
  ::wm::ConvertRectFromScreen(GetTarget()->parent(), &bounds);

  if (scrim_ && scrim_->GetLayer()->GetTargetOpacity() == opacity &&
      scrim_->GetNativeWindow()->bounds() == bounds) {
    return;
  }

  if (!scrim_)
    scrim_ = CreateScrim(GetTarget(), bounds);
  GetTarget()->parent()->StackChildBelow(scrim_->GetNativeWindow(),
                                         GetTarget());
  scrim_->GetLayer()->SetBackgroundBlur(blur);

  if (scrim_->GetNativeWindow()->bounds() != bounds) {
    scrim_->SetOpacity(0.f);
    scrim_->SetBounds(bounds);
  }
  ui::ScopedLayerAnimationSettings animation(scrim_->GetLayer()->GetAnimator());
  animation.SetTweenType(gfx::Tween::EASE_IN_OUT);
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kScrimTransitionInMs));
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  scrim_->SetOpacity(opacity);
}

}  // namespace ash
