// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_controller.h"

#include <memory>

#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Five fixed position ratios of the divider.
constexpr float kFixedPositionRatios[] = {0.0f, 0.33f, 0.5f, 0.67f, 1.0f};

float FindClosestFixedPositionRatio(float distance, float length) {
  float current_ratio = distance / length;
  float closest_ratio = 0.f;
  for (float ratio : kFixedPositionRatios) {
    if (std::abs(current_ratio - ratio) <
        std::abs(current_ratio - closest_ratio))
      closest_ratio = ratio;
  }
  return closest_ratio;
}

gfx::Point GetBoundedPosition(const gfx::Point& location_in_screen,
                              const gfx::Rect& bounds_in_screen) {
  return gfx::Point(
      std::max(std::min(location_in_screen.x(), bounds_in_screen.right() - 1),
               bounds_in_screen.x()),
      std::max(std::min(location_in_screen.y(), bounds_in_screen.bottom() - 1),
               bounds_in_screen.y()));
}

// Returns ture if |left_window_| should be placed on the left or top side of
// the screen.
bool IsLeftWindowOnTopOrLeftOfScreen(
    blink::WebScreenOrientationLockType screen_orientation) {
  return screen_orientation ==
             blink::kWebScreenOrientationLockLandscapePrimary ||
         screen_orientation ==
             blink::kWebScreenOrientationLockPortraitSecondary;
}

}  // namespace

SplitViewController::SplitViewController() {}

SplitViewController::~SplitViewController() {
  EndSplitView();
}

// static
bool SplitViewController::ShouldAllowSplitView() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableTabletSplitView)) {
    return false;
  }
  if (!Shell::Get()
           ->tablet_mode_controller()
           ->IsTabletModeWindowManagerEnabled()) {
    return false;
  }
  return true;
}

// static
bool SplitViewController::CanSnap(aura::Window* window) {
  return wm::CanActivateWindow(window) ? wm::GetWindowState(window)->CanSnap()
                                       : false;
}

bool SplitViewController::IsSplitViewModeActive() const {
  return state_ != NO_SNAP;
}

bool SplitViewController::IsCurrentScreenOrientationLandscape() const {
  return screen_orientation_ ==
             blink::kWebScreenOrientationLockLandscapePrimary ||
         screen_orientation_ ==
             blink::kWebScreenOrientationLockLandscapeSecondary;
}

bool SplitViewController::IsCurrentScreenOrientationPrimary() const {
  return screen_orientation_ ==
             blink::kWebScreenOrientationLockLandscapePrimary ||
         screen_orientation_ == blink::kWebScreenOrientationLockPortraitPrimary;
}

void SplitViewController::SnapWindow(aura::Window* window,
                                     SnapPosition snap_position) {
  DCHECK(window && CanSnap(window));
  DCHECK_NE(snap_position, NONE);

  if (state_ == NO_SNAP) {
    // Add observers when the split view mode starts.
    Shell::Get()->AddShellObserver(this);
    Shell::Get()->activation_client()->AddObserver(this);
    display::Screen::GetScreen()->AddObserver(this);
    Shell::Get()->NotifySplitViewModeStarting();

    screen_orientation_ =
        Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();
    divider_position_ = GetDefaultDividerPosition(window);
    default_snap_position_ = snap_position;
    split_view_divider_ =
        std::make_unique<SplitViewDivider>(this, window->GetRootWindow());
  }

  State previous_state = state_;
  if (snap_position == LEFT) {
    if (left_window_ != window) {
      StopObserving(left_window_);
      left_window_ = window;
    }
    right_window_ = (window == right_window_) ? nullptr : right_window_;
  } else if (snap_position == RIGHT) {
    if (right_window_ != window) {
      StopObserving(right_window_);
      right_window_ = window;
    }
    left_window_ = (window == left_window_) ? nullptr : left_window_;
  }

  if (left_window_ && right_window_) {
    state_ = BOTH_SNAPPED;
  } else if (left_window_) {
    state_ = LEFT_SNAPPED;
  } else if (right_window_) {
    state_ = RIGHT_SNAPPED;
  }

  StartObserving(window);
  const wm::WMEvent event((snap_position == LEFT) ? wm::WM_EVENT_SNAP_LEFT
                                                  : wm::WM_EVENT_SNAP_RIGHT);
  wm::GetWindowState(window)->OnWMEvent(&event);
  wm::ActivateWindow(window);

  // Stack the other snapped window below the current active window so that
  // the snapped two windows are always the top two windows while resizing.
  aura::Window* stacking_target =
      (window == left_window_) ? right_window_ : left_window_;
  if (stacking_target)
    window->parent()->StackChildBelow(stacking_target, window);

  NotifySplitViewStateChanged(previous_state, state_);
}

void SplitViewController::SwapWindows() {
  if (state_ != BOTH_SNAPPED)
    return;

  DCHECK(left_window_ && right_window_);

  aura::Window* new_left_window = right_window_;
  aura::Window* new_right_window = left_window_;

  SnapWindow(new_left_window, LEFT);
  SnapWindow(new_right_window, RIGHT);
}

aura::Window* SplitViewController::GetDefaultSnappedWindow() {
  if (default_snap_position_ == LEFT)
    return left_window_;
  if (default_snap_position_ == RIGHT)
    return right_window_;
  return nullptr;
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInParent(
    aura::Window* window,
    SnapPosition snap_position) {
  gfx::Rect bounds = GetSnappedWindowBoundsInScreen(window, snap_position);
  ::wm::ConvertRectFromScreen(window->GetRootWindow(), &bounds);
  return bounds;
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInScreen(
    aura::Window* window,
    SnapPosition snap_position) {
  const gfx::Rect work_area_bounds_in_screen =
      GetDisplayWorkAreaBoundsInScreen(window);
  if (snap_position == NONE)
    return work_area_bounds_in_screen;

  // |screen_orientation_| and |divide_position_| might not be properly
  // initialized yet.
  if (screen_orientation_ == blink::kWebScreenOrientationLockDefault) {
    screen_orientation_ =
        Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();
  }
  divider_position_ = (divider_position_ < 0)
                          ? GetDefaultDividerPosition(window)
                          : divider_position_;
  const gfx::Rect divider_bounds = SplitViewDivider::GetDividerBoundsInScreen(
      work_area_bounds_in_screen, screen_orientation_, divider_position_,
      false /* is_dragging */);

  gfx::Rect left_or_top_rect;
  gfx::Rect right_or_bottom_rect;
  SplitRect(work_area_bounds_in_screen, divider_bounds,
            IsCurrentScreenOrientationLandscape(), &left_or_top_rect,
            &right_or_bottom_rect);

  if (IsLeftWindowOnTopOrLeftOfScreen(screen_orientation_))
    return (snap_position == LEFT) ? left_or_top_rect : right_or_bottom_rect;
  else
    return (snap_position == LEFT) ? right_or_bottom_rect : left_or_top_rect;
}

gfx::Rect SplitViewController::GetDisplayWorkAreaBoundsInParent(
    aura::Window* window) const {
  aura::Window* root_window = window->GetRootWindow();
  return ScreenUtil::GetDisplayWorkAreaBoundsInParent(
      root_window->GetChildById(kShellWindowId_DefaultContainer));
}

gfx::Rect SplitViewController::GetDisplayWorkAreaBoundsInScreen(
    aura::Window* window) const {
  gfx::Rect bounds = GetDisplayWorkAreaBoundsInParent(window);
  ::wm::ConvertRectToScreen(window->GetRootWindow(), &bounds);
  return bounds;
}

void SplitViewController::StartResize(const gfx::Point& location_in_screen) {
  DCHECK(IsSplitViewModeActive());
  is_resizing_ = true;
  split_view_divider_->UpdateDividerBounds(is_resizing_);
  previous_event_location_ = location_in_screen;
}

void SplitViewController::Resize(const gfx::Point& location_in_screen) {
  DCHECK(IsSplitViewModeActive());

  if (!is_resizing_)
    return;

  const gfx::Rect work_area_bounds =
      GetDisplayWorkAreaBoundsInScreen(GetDefaultSnappedWindow());
  gfx::Point modified_location_in_screen =
      GetBoundedPosition(location_in_screen, work_area_bounds);

  // Update |divider_position_|.
  const int previous_divider_position = divider_position_;
  UpdateDividerPosition(modified_location_in_screen);
  NotifyDividerPositionChanged();

  // Restack windows order if necessary.
  RestackWindows(previous_divider_position, divider_position_);

  // Update the black scrim layer's bounds and opacity.
  UpdateBlackScrim(modified_location_in_screen);

  // Update the snapped window/windows and divider's position.
  UpdateSnappedWindowsAndDividerBounds();

  previous_event_location_ = modified_location_in_screen;
}

void SplitViewController::EndResize(const gfx::Point& location_in_screen) {
  DCHECK(IsSplitViewModeActive());
  // TODO(xdai): Use fade out animation instead of just removing it.
  black_scrim_layer_.reset();
  is_resizing_ = false;

  const gfx::Rect work_area_bounds =
      GetDisplayWorkAreaBoundsInScreen(GetDefaultSnappedWindow());
  gfx::Point modified_location_in_screen =
      GetBoundedPosition(location_in_screen, work_area_bounds);
  UpdateDividerPosition(modified_location_in_screen);
  MoveDividerToClosestFixedPostion();
  NotifyDividerPositionChanged();

  // Check if one of the snapped windows needs to be closed.
  if (ShouldEndSplitViewAfterResizing()) {
    aura::Window* active_window = GetActiveWindowAfterResizingUponExit();
    if (active_window)
      wm::ActivateWindow(active_window);
    EndSplitView();
  } else {
    UpdateSnappedWindowsAndDividerBounds();
  }
}

void SplitViewController::EndSplitView() {
  if (!IsSplitViewModeActive())
    return;

  // Remove observers when the split view mode ends.
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);

  StopObserving(left_window_);
  StopObserving(right_window_);
  left_window_ = nullptr;
  right_window_ = nullptr;
  split_view_divider_.reset();
  black_scrim_layer_.reset();
  default_snap_position_ = NONE;
  screen_orientation_ = blink::kWebScreenOrientationLockDefault;
  divider_position_ = -1;

  State previous_state = state_;
  state_ = NO_SNAP;
  NotifySplitViewStateChanged(previous_state, state_);

  Shell::Get()->NotifySplitViewModeEnded();
}

void SplitViewController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SplitViewController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SplitViewController::OnWindowDestroying(aura::Window* window) {
  // If one of the snapped window gets closed, end the split view mode. The
  // behavior might change in the future.
  DCHECK(IsSplitViewModeActive());
  DCHECK(window == left_window_ || window == right_window_);
  EndSplitView();
}

void SplitViewController::OnPostWindowStateTypeChange(
    ash::wm::WindowState* window_state,
    ash::mojom::WindowStateType old_type) {
  DCHECK(IsSplitViewModeActive());

  if (window_state->IsFullscreen() || window_state->IsMinimized() ||
      window_state->IsMaximized()) {
    // TODO(xdai): Decide what to do if one of the snapped windows gets
    // minimized /maximized / full-screened. For now we end the split view mode
    // for simplicity.
    EndSplitView();
  }
}

void SplitViewController::OnWindowActivated(ActivationReason reason,
                                            aura::Window* gained_active,
                                            aura::Window* lost_active) {
  DCHECK(IsSplitViewModeActive());

  // If |gained_active| was activated as a side effect of a window disposition
  // change, do nothing. For example, when a snapped window is closed, another
  // window will be activated before OnWindowDestroying() is called. We should
  // not try to snap another window in this case.
  if (reason == ActivationReason::WINDOW_DISPOSITION_CHANGED)
    return;

  // Only snap window that can be snapped but hasn't been snapped.
  if (!gained_active || gained_active == left_window_ ||
      gained_active == right_window_ || !CanSnap(gained_active)) {
    return;
  }

  // Only window in MRU list can be snapped.
  aura::Window::Windows windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList();
  if (std::find(windows.begin(), windows.end(), gained_active) == windows.end())
    return;

  // Snap the window on the non-default side of the screen if split view mode
  // is active.
  if (default_snap_position_ == LEFT)
    SnapWindow(gained_active, SplitViewController::RIGHT);
  else if (default_snap_position_ == RIGHT)
    SnapWindow(gained_active, SplitViewController::LEFT);
}

void SplitViewController::OnOverviewModeStarting() {
  DCHECK(IsSplitViewModeActive());

  // If split view mode is active, reset |state_| to make it be able to select
  // another window from overview window grid.
  State previous_state = state_;
  if (default_snap_position_ == LEFT) {
    StopObserving(right_window_);
    right_window_ = nullptr;
    state_ = LEFT_SNAPPED;
  } else if (default_snap_position_ == RIGHT) {
    StopObserving(left_window_);
    left_window_ = nullptr;
    state_ = RIGHT_SNAPPED;
  }
  NotifySplitViewStateChanged(previous_state, state_);
}

void SplitViewController::OnOverviewModeEnded() {
  DCHECK(IsSplitViewModeActive());

  // If split view mode is active but only has one snapped window, use the MRU
  // window list to auto select another window to snap.
  if (state_ != BOTH_SNAPPED) {
    aura::Window::Windows windows =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList();
    for (auto* window : windows) {
      if (CanSnap(window) && window != GetDefaultSnappedWindow()) {
        if (default_snap_position_ == LEFT)
          SnapWindow(window, SplitViewController::RIGHT);
        else if (default_snap_position_ == RIGHT)
          SnapWindow(window, SplitViewController::LEFT);
        break;
      }
    }
  }
}

void SplitViewController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  DCHECK(IsSplitViewModeActive());

  display::Display current_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          GetDefaultSnappedWindow());
  if (display.id() != current_display.id())
    return;

  blink::WebScreenOrientationLockType previous_screen_orientation =
      screen_orientation_;
  screen_orientation_ =
      Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();

  // Update |divider_position_| if the top/left window changes.
  if ((metrics & (display::DisplayObserver::DISPLAY_METRIC_ROTATION)) &&
      (IsLeftWindowOnTopOrLeftOfScreen(previous_screen_orientation) !=
       IsLeftWindowOnTopOrLeftOfScreen(screen_orientation_))) {
    const int work_area_long_length = GetDividerEndPosition();
    const gfx::Size divider_size = SplitViewDivider::GetDividerSize(
        display.work_area(), screen_orientation_, false /* is_dragging */);
    const int divider_short_length =
        std::min(divider_size.width(), divider_size.height());
    divider_position_ =
        work_area_long_length - divider_short_length - divider_position_;
  }

  // For other display configuration changes, we only move the divider to the
  // closest fixed position.
  if (!is_resizing_)
    MoveDividerToClosestFixedPostion();

  NotifyDividerPositionChanged();
  UpdateSnappedWindowsAndDividerBounds();
}

void SplitViewController::StartObserving(aura::Window* window) {
  if (window && !window->HasObserver(this)) {
    window->AddObserver(this);
    wm::GetWindowState(window)->AddObserver(this);
    split_view_divider_->AddObservedWindow(window);
  }
}

void SplitViewController::StopObserving(aura::Window* window) {
  if (window && window->HasObserver(this)) {
    window->RemoveObserver(this);
    wm::GetWindowState(window)->RemoveObserver(this);
    split_view_divider_->RemoveObservedWindow(window);
  }
}

void SplitViewController::NotifySplitViewStateChanged(State previous_state,
                                                      State state) {
  // It's possible that |previous_state| equals to |state| (e.g., snap a window
  // to LEFT and then snap another window to LEFT too.) In this case we still
  // should notify its observers.
  for (Observer& observer : observers_)
    observer.OnSplitViewStateChanged(previous_state, state);
}

void SplitViewController::NotifyDividerPositionChanged() {
  for (Observer& observer : observers_)
    observer.OnSplitViewDividerPositionChanged();
}

int SplitViewController::GetDefaultDividerPosition(aura::Window* window) const {
  const gfx::Rect work_area_bounds_in_screen =
      GetDisplayWorkAreaBoundsInScreen(window);
  const gfx::Size divider_size = SplitViewDivider::GetDividerSize(
      work_area_bounds_in_screen, screen_orientation_, false /* is_dragging */);
  if (IsCurrentScreenOrientationLandscape())
    return (work_area_bounds_in_screen.width() - divider_size.width()) * 0.5f;
  else
    return (work_area_bounds_in_screen.height() - divider_size.height()) * 0.5f;
}

void SplitViewController::UpdateBlackScrim(
    const gfx::Point& location_in_screen) {
  DCHECK(IsSplitViewModeActive());

  if (!black_scrim_layer_) {
    // Create an invisible black scrim layer.
    black_scrim_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
    black_scrim_layer_->SetColor(SK_ColorBLACK);
    GetDefaultSnappedWindow()->GetRootWindow()->layer()->Add(
        black_scrim_layer_.get());
    GetDefaultSnappedWindow()->GetRootWindow()->layer()->StackAtTop(
        black_scrim_layer_.get());
  }

  // Decide where the black scrim should show and update its bounds.
  SnapPosition position = GetBlackScrimPosition(location_in_screen);
  if (position == NONE) {
    black_scrim_layer_.reset();
    return;
  }
  black_scrim_layer_->SetBounds(
      GetSnappedWindowBoundsInScreen(GetDefaultSnappedWindow(), position));

  // Update its opacity. The opacity increases as it gets closer to the edge of
  // the screen.
  float opacity = 0.f;
  const gfx::Rect work_area_bounds =
      GetDisplayWorkAreaBoundsInScreen(GetDefaultSnappedWindow());
  if (IsCurrentScreenOrientationLandscape()) {
    int distance_x =
        std::min(std::abs(location_in_screen.x() - work_area_bounds.x()),
                 std::abs(work_area_bounds.right() - location_in_screen.x()));
    opacity = 1.f - float(distance_x) / float(work_area_bounds.width());
  } else {
    int distance_y =
        std::min(std::abs(location_in_screen.y() - work_area_bounds.y()),
                 std::abs(work_area_bounds.bottom() - location_in_screen.y()));
    opacity = 1.f - float(distance_y) / float(work_area_bounds.height());
  }
  black_scrim_layer_->SetOpacity(opacity);
}

void SplitViewController::RestackWindows(const int previous_divider_position,
                                         const int current_divider_position) {
  if (!left_window_ || !right_window_)
    return;
  DCHECK(IsSplitViewModeActive());
  DCHECK_EQ(left_window_->parent(), right_window_->parent());

  const int mid_position = GetDefaultDividerPosition(GetDefaultSnappedWindow());
  if (std::signbit(previous_divider_position - mid_position) ==
      std::signbit(current_divider_position - mid_position)) {
    // No need to restack windows if the divider position doesn't pass over the
    // middle position.
    return;
  }

  if ((current_divider_position < mid_position) ==
      IsLeftWindowOnTopOrLeftOfScreen(screen_orientation_)) {
    left_window_->parent()->StackChildAbove(right_window_, left_window_);
  } else {
    left_window_->parent()->StackChildAbove(left_window_, right_window_);
  }
}

void SplitViewController::UpdateSnappedWindowsAndDividerBounds() {
  DCHECK(IsSplitViewModeActive());

  // Update the snapped windows' bounds.
  if (left_window_ && wm::GetWindowState(left_window_)->IsSnapped()) {
    const wm::WMEvent left_window_event(wm::WM_EVENT_SNAP_LEFT);
    wm::GetWindowState(left_window_)->OnWMEvent(&left_window_event);
  }
  if (right_window_ && wm::GetWindowState(right_window_)->IsSnapped()) {
    const wm::WMEvent right_window_event(wm::WM_EVENT_SNAP_RIGHT);
    wm::GetWindowState(right_window_)->OnWMEvent(&right_window_event);
  }

  // Update divider's bounds.
  split_view_divider_->UpdateDividerBounds(is_resizing_);
}

SplitViewController::SnapPosition SplitViewController::GetBlackScrimPosition(
    const gfx::Point& location_in_screen) {
  SnapPosition position = SplitViewController::NONE;
  const gfx::Rect work_area_bounds =
      GetDisplayWorkAreaBoundsInScreen(GetDefaultSnappedWindow());
  if (!work_area_bounds.Contains(location_in_screen))
    return position;

  switch (screen_orientation_) {
    case blink::kWebScreenOrientationLockLandscapePrimary:
    case blink::kWebScreenOrientationLockLandscapeSecondary:
      if (location_in_screen.x() <
          work_area_bounds.x() +
              work_area_bounds.width() * kFixedPositionRatios[1]) {
        position = IsCurrentScreenOrientationPrimary() ? LEFT : RIGHT;
      } else if (location_in_screen.x() >
                 work_area_bounds.x() +
                     work_area_bounds.width() * kFixedPositionRatios[3]) {
        position = IsCurrentScreenOrientationPrimary() ? RIGHT : LEFT;
      }
      break;
    case blink::kWebScreenOrientationLockPortraitPrimary:
    case blink::kWebScreenOrientationLockPortraitSecondary:
      if (location_in_screen.y() >
          work_area_bounds.y() +
              work_area_bounds.height() * kFixedPositionRatios[3]) {
        position = IsCurrentScreenOrientationPrimary() ? LEFT : RIGHT;
      } else if (location_in_screen.y() <
                 work_area_bounds.y() +
                     work_area_bounds.height() * kFixedPositionRatios[1]) {
        position = IsCurrentScreenOrientationPrimary() ? RIGHT : LEFT;
      }
      break;
    default:
      break;
  }
  return position;
}

void SplitViewController::UpdateDividerPosition(
    const gfx::Point& location_in_screen) {
  if (IsCurrentScreenOrientationLandscape()) {
    divider_position_ += location_in_screen.x() - previous_event_location_.x();
  } else {
    divider_position_ += location_in_screen.y() - previous_event_location_.y();
  }
  divider_position_ = std::max(0, divider_position_);
}

void SplitViewController::SplitRect(const gfx::Rect& work_area_rect,
                                    const gfx::Rect& divider_rect,
                                    const bool is_split_vertically,
                                    gfx::Rect* left_or_top_rect,
                                    gfx::Rect* right_or_bottom_rect) {
  if (!work_area_rect.Contains(divider_rect))
    return;

  if (is_split_vertically) {
    left_or_top_rect->SetRect(work_area_rect.x(), work_area_rect.y(),
                              divider_rect.x() - work_area_rect.x(),
                              work_area_rect.height());
    right_or_bottom_rect->SetRect(divider_rect.right(), work_area_rect.y(),
                                  work_area_rect.width() -
                                      left_or_top_rect->width() -
                                      divider_rect.width(),
                                  work_area_rect.height());
  } else {
    left_or_top_rect->SetRect(work_area_rect.x(), work_area_rect.y(),
                              work_area_rect.width(),
                              divider_rect.y() - work_area_rect.y());
    right_or_bottom_rect->SetRect(
        work_area_rect.x(), divider_rect.bottom(), work_area_rect.width(),
        work_area_rect.height() - left_or_top_rect->height() -
            divider_rect.height());
  }
}

void SplitViewController::MoveDividerToClosestFixedPostion() {
  DCHECK(IsSplitViewModeActive());

  float ratio =
      FindClosestFixedPositionRatio(divider_position_, GetDividerEndPosition());
  divider_position_ = GetDividerEndPosition() * ratio;
}

bool SplitViewController::ShouldEndSplitViewAfterResizing() {
  DCHECK(IsSplitViewModeActive());

  return divider_position_ == 0 || divider_position_ == GetDividerEndPosition();
}

aura::Window* SplitViewController::GetActiveWindowAfterResizingUponExit() {
  DCHECK(IsSplitViewModeActive());

  if (!ShouldEndSplitViewAfterResizing())
    return nullptr;

  if (divider_position_ == 0) {
    return IsLeftWindowOnTopOrLeftOfScreen(screen_orientation_) ? right_window_
                                                                : left_window_;
  } else {
    return IsLeftWindowOnTopOrLeftOfScreen(screen_orientation_) ? left_window_
                                                                : right_window_;
  }
}

int SplitViewController::GetDividerEndPosition() {
  const gfx::Rect work_area_bounds =
      GetDisplayWorkAreaBoundsInScreen(GetDefaultSnappedWindow());
  return std::max(work_area_bounds.width(), work_area_bounds.height());
}

}  // namespace ash
