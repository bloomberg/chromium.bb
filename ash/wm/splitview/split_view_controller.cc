// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_controller.h"

#include <cmath>
#include <memory>

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_data.h"
#include "ash/system/toast/toast_manager.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/sys_info.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Three fixed position ratios of the divider, which means the divider can
// always be moved to these three positions.
constexpr float kFixedPositionRatios[] = {0.f, 0.5f, 1.0f};

// Two optional position ratios of the divider. Whether the divider can be moved
// to these two positions depends on the minimum size of the snapped windows.
constexpr float kOneThirdPositionRatio = 0.33f;
constexpr float kTwoThirdPositionRatio = 0.67f;

// Toast data.
constexpr char kAppCannotSnapToastId[] = "split_view_app_cannot_snap";
constexpr int kAppCannotSnapToastDurationMs = 2500;

gfx::Point GetBoundedPosition(const gfx::Point& location_in_screen,
                              const gfx::Rect& bounds_in_screen) {
  return gfx::Point(
      std::max(std::min(location_in_screen.x(), bounds_in_screen.right() - 1),
               bounds_in_screen.x()),
      std::max(std::min(location_in_screen.y(), bounds_in_screen.bottom() - 1),
               bounds_in_screen.y()));
}

// Transpose the given |rect|.
void TransposeRect(gfx::Rect* rect) {
  rect->SetRect(rect->y(), rect->x(), rect->height(), rect->width());
}

// Returns true if |window| is an Arc app window.
bool IsArcAppWindow(aura::Window* window) {
  return window && window->GetProperty(aura::client::kAppType) ==
                       static_cast<int>(AppType::ARC_APP);
}

// Gets the window that is stacked above the other. The windows for comparison
// must have the same parent if they are both not nullptr.
aura::Window* GetWindowStackedAbove(aura::Window* window1,
                                    aura::Window* window2) {
  if (!window1 || !window2)
    return window1 ? window1 : window2;

  DCHECK(window1->parent() == window2->parent());
  const aura::Window::Windows windows = window1->parent()->children();
  auto window1_i = std::find(windows.begin(), windows.end(), window1);
  auto window2_i = std::find(windows.begin(), windows.end(), window2);
  return window1_i > window2_i ? window1 : window2;
}

mojom::SplitViewState ToMojomSplitViewState(SplitViewController::State state) {
  switch (state) {
    case SplitViewController::NO_SNAP:
      return mojom::SplitViewState::NO_SNAP;
    case SplitViewController::LEFT_SNAPPED:
      return mojom::SplitViewState::LEFT_SNAPPED;
    case SplitViewController::RIGHT_SNAPPED:
      return mojom::SplitViewState::RIGHT_SNAPPED;
    case SplitViewController::BOTH_SNAPPED:
      return mojom::SplitViewState::BOTH_SNAPPED;
    default:
      NOTREACHED();
      return mojom::SplitViewState::NO_SNAP;
  }
}

mojom::WindowStateType GetStateTypeFromSnapPostion(
    SplitViewController::SnapPosition snap_position) {
  DCHECK(snap_position != SplitViewController::NONE);
  if (snap_position == SplitViewController::LEFT)
    return mojom::WindowStateType::LEFT_SNAPPED;
  if (snap_position == SplitViewController::RIGHT)
    return mojom::WindowStateType::RIGHT_SNAPPED;
  NOTREACHED();
  return mojom::WindowStateType::DEFAULT;
}

// Returns the minimum size of the window according to the screen orientation.
int GetMinimumWindowSize(aura::Window* window, bool is_landscape) {
  int minimum_width = 0;
  if (window && window->delegate()) {
    gfx::Size minimum_size = window->delegate()->GetMinimumSize();
    minimum_width = is_landscape ? minimum_size.width() : minimum_size.height();
  }
  return minimum_width;
};

// Returns true if |window| is currently snapped.
bool IsSnapped(aura::Window* window) {
  if (!window)
    return false;
  return wm::GetWindowState(window)->IsSnapped();
}

}  // namespace

SplitViewController::SplitViewController() {
  display::Screen::GetScreen()->AddObserver(this);
}

SplitViewController::~SplitViewController() {
  display::Screen::GetScreen()->RemoveObserver(this);
  EndSplitView();
}

// static
bool SplitViewController::ShouldAllowSplitView() {
#if defined(GOOGLE_CHROME_BUILD)
  // Disable splitscreen on M66 stable channel unless it is explicity enabled by
  // the user, and keep it enabled by default for all other channels on M66.
  // It will be reverted later on M67.
  constexpr char ChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
  constexpr char kChromeOSStableChannelString[] = "stable-channel";
  std::string channel;
  if (base::SysInfo::GetLsbReleaseValue(kChromeOSReleaseTrack, &channel) &&
      channel == kChromeOSStableChannelString &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableTabletSplitView)) {
    return false;
  }
#endif

  if (!Shell::Get()
           ->tablet_mode_controller()
           ->IsTabletModeWindowManagerEnabled()) {
    return false;
  }
  return true;
}

void SplitViewController::BindRequest(
    mojom::SplitViewControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

bool SplitViewController::CanSnap(aura::Window* window) {
  if (!wm::CanActivateWindow(window))
    return false;
  if (!wm::GetWindowState(window)->CanSnap())
    return false;
  if (window->delegate()) {
    // If the window's minimum size is larger than half of the display's work
    // area size, the window can't be snapped in this case.
    const gfx::Size min_size = window->delegate()->GetMinimumSize();
    const gfx::Rect display_area = GetDisplayWorkAreaBoundsInScreen(window);
    bool is_landscape = (display_area.width() > display_area.height());
    if ((is_landscape && min_size.width() > display_area.width() / 2) ||
        (!is_landscape && min_size.height() > display_area.height() / 2)) {
      return false;
    }
  }
  return true;
}

bool SplitViewController::IsSplitViewModeActive() const {
  return state_ != NO_SNAP;
}

OrientationLockType SplitViewController::GetCurrentScreenOrientation() const {
  // ScreenOrientationController might be nullptr during shutdown.
  if (!Shell::Get()->screen_orientation_controller())
    return OrientationLockType::kAny;
  return Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();
}

bool SplitViewController::IsCurrentScreenOrientationLandscape() const {
  return IsLandscapeOrientation(GetCurrentScreenOrientation());
}

bool SplitViewController::IsCurrentScreenOrientationPrimary() const {
  return IsPrimaryOrientation(GetCurrentScreenOrientation());
}

void SplitViewController::SnapWindow(aura::Window* window,
                                     SnapPosition snap_position,
                                     const gfx::Rect& window_item_bounds) {
  DCHECK(window && CanSnap(window));
  DCHECK_NE(snap_position, NONE);

  if (state_ == NO_SNAP) {
    // Add observers when the split view mode starts.
    Shell::Get()->AddShellObserver(this);
    Shell::Get()->activation_client()->AddObserver(this);
    Shell::Get()->tablet_mode_controller()->AddObserver(this);
    Shell::Get()->NotifySplitViewModeStarting();

    divider_position_ = GetDefaultDividerPosition(window);
    default_snap_position_ = snap_position;
    split_view_divider_ =
        std::make_unique<SplitViewDivider>(this, window->GetRootWindow());
    splitview_start_time_ = base::Time::Now();
  }

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
  StartObserving(window);

  if (!window_item_bounds.IsEmpty())
    overview_window_item_bounds_map_[window] = window_item_bounds;

  // Update the divider position and window bounds before snapping a new window.
  // Since the minimum size of |window| maybe larger than currently bounds in
  // |snap_position|.
  if (state_ != NO_SNAP) {
    MoveDividerToClosestFixedPosition();
    UpdateSnappedWindowsAndDividerBounds();
  }

  if (wm::GetWindowState(window)->GetStateType() ==
      GetStateTypeFromSnapPostion(snap_position)) {
    // If the window has already been snapped, just activate it. Restore its
    // transform if applicable. Also update the split view state and notify the
    // observers about the change.
    UpdateSplitViewStateAndNotifyObservers();
    RestoreAndActivateSnappedWindow(window);
  } else {
    // Otherwise, try to snap it first. It will be activated later after the
    // window is snapped. The split view state will also be updated after the
    // window is snapped.
    const wm::WMEvent event((snap_position == LEFT) ? wm::WM_EVENT_SNAP_LEFT
                                                    : wm::WM_EVENT_SNAP_RIGHT);
    wm::GetWindowState(window)->OnWMEvent(&event);
  }

  base::RecordAction(base::UserMetricsAction("SplitView_SnapWindow"));
}

void SplitViewController::SwapWindows() {
  if (state_ != BOTH_SNAPPED)
    return;

  DCHECK(left_window_ && right_window_);

  aura::Window* new_left_window = right_window_;
  aura::Window* new_right_window = left_window_;
  left_window_ = new_left_window;
  right_window_ = new_right_window;

  MoveDividerToClosestFixedPosition();
  UpdateSnappedWindowsAndDividerBounds();

  base::RecordAction(
      base::UserMetricsAction("SplitView_DoubleTapDividerSwapWindows"));
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

  gfx::Rect left_or_top_rect, right_or_bottom_rect;
  GetSnappedWindowBoundsInScreenInternal(window, &left_or_top_rect,
                                         &right_or_bottom_rect);

  // Adjust the bounds for |left_or_top_rect| and |right_or_bottom_rect| if the
  // desired bound is smaller than the minimum bounds of the window.
  AdjustSnappedWindowBounds(&left_or_top_rect, &right_or_bottom_rect);

  if (IsCurrentScreenOrientationPrimary())
    return (snap_position == LEFT) ? left_or_top_rect : right_or_bottom_rect;
  else
    return (snap_position == LEFT) ? right_or_bottom_rect : left_or_top_rect;
}

gfx::Rect SplitViewController::GetDisplayWorkAreaBoundsInParent(
    aura::Window* window) const {
  aura::Window* root_window = window->GetRootWindow();
  return screen_util::GetDisplayWorkAreaBoundsInParent(
      root_window->GetChildById(kShellWindowId_DefaultContainer));
}

gfx::Rect SplitViewController::GetDisplayWorkAreaBoundsInScreen(
    aura::Window* window) const {
  gfx::Rect bounds = GetDisplayWorkAreaBoundsInParent(window);
  ::wm::ConvertRectToScreen(window->GetRootWindow(), &bounds);
  return bounds;
}

gfx::Rect SplitViewController::GetSnappedWindowBoundsInScreenUnadjusted(
    aura::Window* window,
    SnapPosition snap_position) {
  const gfx::Rect work_area_bounds_in_screen =
      GetDisplayWorkAreaBoundsInScreen(window);
  if (snap_position == NONE)
    return work_area_bounds_in_screen;

  gfx::Rect left_or_top_rect, right_or_bottom_rect;
  GetSnappedWindowBoundsInScreenInternal(window, &left_or_top_rect,
                                         &right_or_bottom_rect);

  if (IsCurrentScreenOrientationPrimary())
    return (snap_position == LEFT) ? left_or_top_rect : right_or_bottom_rect;
  else
    return (snap_position == LEFT) ? right_or_bottom_rect : left_or_top_rect;
}

void SplitViewController::StartResize(const gfx::Point& location_in_screen) {
  DCHECK(IsSplitViewModeActive());
  is_resizing_ = true;
  split_view_divider_->UpdateDividerBounds(is_resizing_);
  previous_event_location_ = location_in_screen;

  smooth_resize_window_ = GetWindowForSmoothResize();
  DCHECK(smooth_resize_window_);
  wm::WindowState* window_state = wm::GetWindowState(smooth_resize_window_);
  gfx::Point location_in_parent(location_in_screen);
  ::wm::ConvertPointFromScreen(smooth_resize_window_->parent(),
                               &location_in_parent);
  int window_component = GetWindowComponentForResize(smooth_resize_window_);
  window_state->CreateDragDetails(location_in_parent, window_component,
                                  ::wm::WINDOW_MOVE_SOURCE_TOUCH);
  window_state->OnDragStarted(window_component);

  base::RecordAction(base::UserMetricsAction("SplitView_ResizeWindows"));
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
  UpdateDividerPosition(modified_location_in_screen);
  NotifyDividerPositionChanged();

  // Update the black scrim layer's bounds and opacity.
  UpdateBlackScrim(modified_location_in_screen);

  // Update the snapped window/windows and divider's position.
  UpdateSnappedWindowsAndDividerBounds();

  // Apply window transform if necessary.
  SetWindowsTransformDuringResizing();

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
  MoveDividerToClosestFixedPosition();
  NotifyDividerPositionChanged();
  RestoreWindowsTransformAfterResizing();

  if (smooth_resize_window_) {
    // Update snapped window/windows bounds before sending OnCompleteDrag() for
    // smoother resizing visual result.
    UpdateSnappedWindowsAndDividerBounds();
    wm::WindowState* window_state = wm::GetWindowState(smooth_resize_window_);
    window_state->OnCompleteDrag(
        GetEndDragLocationInScreen(smooth_resize_window_, location_in_screen));
    window_state->DeleteDragDetails();
  }

  // Need to update snapped windows bounds even if the split view mode may have
  // to exit. Otherwise it's possible for a snapped window stuck in the edge of
  // of the screen while overview mode is active.
  UpdateSnappedWindowsAndDividerBounds();

  // Check if one of the snapped windows needs to be closed.
  if (ShouldEndSplitViewAfterResizing()) {
    aura::Window* active_window = GetActiveWindowAfterResizingUponExit();

    // Track the window that needs to be put back into the overview list if we
    // remain in overview mode.
    aura::Window* insert_overview_window = nullptr;
    if (Shell::Get()->window_selector_controller()->IsSelecting())
      insert_overview_window = GetDefaultSnappedWindow();
    EndSplitView();
    if (active_window) {
      EndOverview();
      wm::ActivateWindow(active_window);
    } else if (insert_overview_window) {
      Shell::Get()->window_selector_controller()->window_selector()->AddItem(
          insert_overview_window);
    }
  }
}

void SplitViewController::ShowAppCannotSnapToast() {
  ash::ToastData toast(
      kAppCannotSnapToastId,
      l10n_util::GetStringUTF16(IDS_ASH_SPLIT_VIEW_CANNOT_SNAP),
      kAppCannotSnapToastDurationMs, base::Optional<base::string16>());
  ash::Shell::Get()->toast_manager()->Show(toast);
}

void SplitViewController::EndSplitView() {
  if (!IsSplitViewModeActive())
    return;

  // Remove observers when the split view mode ends.
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);

  StopObserving(left_window_);
  StopObserving(right_window_);
  left_window_ = nullptr;
  right_window_ = nullptr;
  split_view_divider_.reset();
  black_scrim_layer_.reset();
  default_snap_position_ = NONE;
  divider_position_ = -1;
  overview_window_item_bounds_map_.clear();

  UpdateSplitViewStateAndNotifyObservers();
  base::RecordAction(base::UserMetricsAction("SplitView_EndSplitView"));
  UMA_HISTOGRAM_LONG_TIMES("Ash.SplitView.TimeInSplitView",
                           base::Time::Now() - splitview_start_time_);
}

void SplitViewController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SplitViewController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SplitViewController::AddObserver(mojom::SplitViewObserverPtr observer) {
  mojom::SplitViewObserver* observer_ptr = observer.get();
  mojo_observers_.AddPtr(std::move(observer));
  observer_ptr->OnSplitViewStateChanged(ToMojomSplitViewState(state_));
}

void SplitViewController::OnWindowDestroying(aura::Window* window) {
  DCHECK(IsSplitViewModeActive());
  DCHECK(window == left_window_ || window == right_window_);
  if (smooth_resize_window_ == window)
    smooth_resize_window_ = nullptr;
  auto iter = overview_window_item_bounds_map_.find(window);
  if (iter != overview_window_item_bounds_map_.end())
    overview_window_item_bounds_map_.erase(iter);
  OnSnappedWindowMinimizedOrDestroyed(window);
}

void SplitViewController::OnPostWindowStateTypeChange(
    ash::wm::WindowState* window_state,
    ash::mojom::WindowStateType old_type) {
  if (window_state->IsSnapped()) {
    UpdateSplitViewStateAndNotifyObservers();
    RestoreAndActivateSnappedWindow(window_state->window());
  } else if (window_state->IsFullscreen() || window_state->IsMaximized()) {
    // End split view mode if one of the snapped windows gets maximized /
    // full-screened. Also end overview mode if overview mode is active at the
    // moment.
    EndSplitView();
    EndOverview();
  } else if (window_state->IsMinimized()) {
    OnSnappedWindowMinimizedOrDestroyed(window_state->window());
    // Insert the minimized window back to overview if split view mode is ended
    // because of the minimization of the window, but overview mode is still
    // active at the moment.
    if (!IsSplitViewModeActive() &&
        Shell::Get()->window_selector_controller()->IsSelecting()) {
      Shell::Get()->window_selector_controller()->window_selector()->AddItem(
          window_state->window());
    }
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

  // Only snap window that hasn't been snapped.
  if (!gained_active || gained_active == left_window_ ||
      gained_active == right_window_) {
    return;
  }

  // Only window in MRU list can be snapped.
  if (!base::ContainsValue(
          Shell::Get()->mru_window_tracker()->BuildMruWindowList(),
          gained_active)) {
    return;
  }

  // If it's a user positionable window but can't be snapped, end split view
  // mode and show the cannot snap toast.
  if (!CanSnap(gained_active)) {
    if (wm::GetWindowState(gained_active)->IsUserPositionable()) {
      EndSplitView();
      ShowAppCannotSnapToast();
    }
    return;
  }

  // If the to-be-snapped window comes from the overview grid, get its overview
  // window item bounds before trying to snap it.
  gfx::Rect window_item_bounds;
  if (Shell::Get()->window_selector_controller()->IsSelecting()) {
    WindowSelector* window_selector =
        Shell::Get()->window_selector_controller()->window_selector();
    WindowGrid* current_grid = window_selector->GetGridWithRootWindow(
        GetDefaultSnappedWindow()->GetRootWindow());
    if (current_grid) {
      WindowSelectorItem* item =
          current_grid->GetWindowSelectorItemContaining(gained_active);
      if (item) {
        window_item_bounds = item->target_bounds();
        window_selector->RemoveWindowSelectorItem(item);
      }
    }
  }

  // Snap the window on the non-default side of the screen if split view mode
  // is active.
  SnapWindow(gained_active, (default_snap_position_ == LEFT) ? RIGHT : LEFT,
             window_item_bounds);
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

void SplitViewController::OnOverviewModeEnding() {
  DCHECK(IsSplitViewModeActive());

  if (state_ == BOTH_SNAPPED)
    return;

  WindowSelector* window_selector =
      Shell::Get()->window_selector_controller()->window_selector();
  WindowGrid* current_grid = window_selector->GetGridWithRootWindow(
      GetDefaultSnappedWindow()->GetRootWindow());
  if (!current_grid)
    return;

  // If split view mode is active but only has one snapped window when overview
  // mode is ending, retrieve the first snappable window in the overview window
  // grid and snap it.
  const auto& windows = current_grid->window_list();
  if (windows.size() > 0) {
    for (const auto& window_selector_item : windows) {
      aura::Window* window = window_selector_item->GetWindow();
      if (CanSnap(window) && window != GetDefaultSnappedWindow()) {
        const gfx::Rect item_bounds = window_selector_item->target_bounds();
        window_selector->RemoveWindowSelectorItem(window_selector_item.get());
        SnapWindow(window, (default_snap_position_ == LEFT) ? RIGHT : LEFT,
                   item_bounds);
        return;
      }
    }

    // Arriving here we know there is no window in the window grid can be
    // snapped, in this case end the splitview mode and show cannot snap
    // toast.
    EndSplitView();
    ShowAppCannotSnapToast();
  }
}

void SplitViewController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (!display.IsInternal())
    return;

  // We need update |previous_screen_orientation_| even though split view mode
  // is not active at the moment.
  OrientationLockType previous_screen_orientation =
      previous_screen_orientation_;
  previous_screen_orientation_ = GetCurrentScreenOrientation();

  if (!IsSplitViewModeActive())
    return;

  display::Display current_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          GetDefaultSnappedWindow());
  if (display.id() != current_display.id())
    return;

  // If one of the snapped windows becomes unsnappable, end the split view mode
  // directly.
  if ((left_window_ && !CanSnap(left_window_)) ||
      (right_window_ && !CanSnap(right_window_))) {
    if (!Shell::Get()->session_controller()->IsUserSessionBlocked())
      EndSplitView();
    return;
  }

  // Update |divider_position_| if the top/left window changes.
  if ((metrics & (display::DisplayObserver::DISPLAY_METRIC_ROTATION)) &&
      (IsPrimaryOrientation(previous_screen_orientation) !=
       IsCurrentScreenOrientationPrimary())) {
    const int work_area_long_length = GetDividerEndPosition();
    const gfx::Size divider_size = SplitViewDivider::GetDividerSize(
        display.work_area(), GetCurrentScreenOrientation(),
        false /* is_dragging */);
    const int divider_short_length =
        std::min(divider_size.width(), divider_size.height());
    divider_position_ =
        work_area_long_length - divider_short_length - divider_position_;
  }

  // For other display configuration changes, we only move the divider to the
  // closest fixed position.
  if (!is_resizing_)
    MoveDividerToClosestFixedPosition();

  NotifyDividerPositionChanged();
  UpdateSnappedWindowsAndDividerBounds();
}

void SplitViewController::OnTabletModeEnding() {
  EndSplitView();
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

void SplitViewController::UpdateSplitViewStateAndNotifyObservers() {
  State previous_state = state_;
  if (IsSnapped(left_window_) && IsSnapped(right_window_))
    state_ = BOTH_SNAPPED;
  else if (IsSnapped(left_window_))
    state_ = LEFT_SNAPPED;
  else if (IsSnapped(right_window_))
    state_ = RIGHT_SNAPPED;
  else
    state_ = NO_SNAP;

  // We still notify observers even if |state_| doesn't change as it's possible
  // to snap a window to a position that already has a snapped window.
  NotifySplitViewStateChanged(previous_state, state_);

  if (previous_state == state_)
    return;
  if (previous_state == NO_SNAP)
    Shell::Get()->NotifySplitViewModeStarted();
  else if (state_ == NO_SNAP)
    Shell::Get()->NotifySplitViewModeEnded();
}

void SplitViewController::NotifySplitViewStateChanged(State previous_state,
                                                      State state) {
  // It's possible that |previous_state| equals to |state| (e.g., snap a window
  // to LEFT and then snap another window to LEFT too.) In this case we still
  // should notify its observers.
  for (Observer& observer : observers_)
    observer.OnSplitViewStateChanged(previous_state, state);
  mojo_observers_.ForAllPtrs([state](mojom::SplitViewObserver* observer) {
    observer->OnSplitViewStateChanged(ToMojomSplitViewState(state));
  });
}

void SplitViewController::NotifyDividerPositionChanged() {
  for (Observer& observer : observers_)
    observer.OnSplitViewDividerPositionChanged();
}

int SplitViewController::GetDefaultDividerPosition(aura::Window* window) const {
  const gfx::Rect work_area_bounds_in_screen =
      GetDisplayWorkAreaBoundsInScreen(window);
  const gfx::Size divider_size = SplitViewDivider::GetDividerSize(
      work_area_bounds_in_screen, GetCurrentScreenOrientation(),
      false /* is_dragging */);
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

void SplitViewController::UpdateSnappedWindowsAndDividerBounds() {
  DCHECK(IsSplitViewModeActive());

  // Update the snapped windows' bounds.
  if (IsSnapped(left_window_)) {
    const wm::WMEvent left_window_event(wm::WM_EVENT_SNAP_LEFT);
    wm::GetWindowState(left_window_)->OnWMEvent(&left_window_event);
  }
  if (IsSnapped(right_window_)) {
    const wm::WMEvent right_window_event(wm::WM_EVENT_SNAP_RIGHT);
    wm::GetWindowState(right_window_)->OnWMEvent(&right_window_event);
  }

  // Update divider's bounds.
  split_view_divider_->UpdateDividerBounds(is_resizing_);
}

SplitViewController::SnapPosition SplitViewController::GetBlackScrimPosition(
    const gfx::Point& location_in_screen) {
  const gfx::Rect work_area_bounds =
      GetDisplayWorkAreaBoundsInScreen(GetDefaultSnappedWindow());
  if (!work_area_bounds.Contains(location_in_screen))
    return NONE;

  gfx::Size left_window_min_size, right_window_min_size;
  if (left_window_ && left_window_->delegate())
    left_window_min_size = left_window_->delegate()->GetMinimumSize();
  if (right_window_ && right_window_->delegate())
    right_window_min_size = right_window_->delegate()->GetMinimumSize();

  bool is_primary = IsCurrentScreenOrientationPrimary();
  int long_length = GetDividerEndPosition();
  // The distance from the current resizing position to the left or right side
  // of the screen. Note: left or right side here means the side of the
  // |left_window_| or |right_window_|.
  int left_window_distance = 0, right_window_distance = 0;
  int min_left_length = 0, min_right_length = 0;

  if (IsCurrentScreenOrientationLandscape()) {
    int left_distance = location_in_screen.x() - work_area_bounds.x();
    int right_distance = work_area_bounds.right() - location_in_screen.x();
    left_window_distance = is_primary ? left_distance : right_distance;
    right_window_distance = is_primary ? right_distance : left_distance;

    min_left_length = left_window_min_size.width();
    min_right_length = right_window_min_size.width();
  } else {
    int top_distance = location_in_screen.y() - work_area_bounds.y();
    int bottom_distance = work_area_bounds.bottom() - location_in_screen.y();
    left_window_distance = is_primary ? top_distance : bottom_distance;
    right_window_distance = is_primary ? bottom_distance : top_distance;

    min_left_length = left_window_min_size.height();
    min_right_length = right_window_min_size.height();
  }

  if (left_window_distance < long_length * kOneThirdPositionRatio ||
      left_window_distance < min_left_length) {
    return LEFT;
  }
  if (right_window_distance < long_length * kOneThirdPositionRatio ||
      right_window_distance < min_right_length) {
    return RIGHT;
  }

  return NONE;
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

void SplitViewController::GetSnappedWindowBoundsInScreenInternal(
    aura::Window* window,
    gfx::Rect* left_or_top_rect,
    gfx::Rect* right_or_bottom_rect) {
  const gfx::Rect work_area_bounds_in_screen =
      GetDisplayWorkAreaBoundsInScreen(window);

  // |divide_position_| might not be properly initialized yet.
  divider_position_ = (divider_position_ < 0)
                          ? GetDefaultDividerPosition(window)
                          : divider_position_;
  const gfx::Rect divider_bounds = SplitViewDivider::GetDividerBoundsInScreen(
      work_area_bounds_in_screen, GetCurrentScreenOrientation(),
      divider_position_, false /* is_dragging */);

  SplitRect(work_area_bounds_in_screen, divider_bounds,
            IsCurrentScreenOrientationLandscape(), left_or_top_rect,
            right_or_bottom_rect);
}

void SplitViewController::SplitRect(const gfx::Rect& work_area_rect,
                                    const gfx::Rect& divider_rect,
                                    const bool is_split_vertically,
                                    gfx::Rect* left_or_top_rect,
                                    gfx::Rect* right_or_bottom_rect) {
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

void SplitViewController::MoveDividerToClosestFixedPosition() {
  DCHECK(IsSplitViewModeActive());

  const gfx::Rect work_area_bounds_in_screen =
      GetDisplayWorkAreaBoundsInScreen(GetDefaultSnappedWindow());
  const gfx::Size divider_size = SplitViewDivider::GetDividerSize(
      work_area_bounds_in_screen, GetCurrentScreenOrientation(),
      false /* is_dragging */);
  const int divider_thickness =
      std::min(divider_size.width(), divider_size.height());

  // The values in |kFixedPositionRatios| represent the fixed position of the
  // center of the divider while |divider_position_| represent the origin of the
  // divider rectangle. So, before calling FindClosestFixedPositionRatio,
  // extract the center from |divider_position_|. The result will also be the
  // center of the divider, so extract the origin, unless the result is on of
  // the endpoints.
  int work_area_long_length = GetDividerEndPosition();
  float closest_ratio = FindClosestPositionRatio(
      divider_position_ + std::floor(divider_thickness / 2.f),
      work_area_long_length);
  divider_position_ = std::floor(work_area_long_length * closest_ratio);
  if (closest_ratio > 0.f && closest_ratio < 1.f)
    divider_position_ -= std::floor(divider_thickness / 2.f);
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
    return IsCurrentScreenOrientationPrimary() ? right_window_ : left_window_;
  } else {
    return IsCurrentScreenOrientationPrimary() ? left_window_ : right_window_;
  }
}

int SplitViewController::GetDividerEndPosition() {
  const gfx::Rect work_area_bounds =
      GetDisplayWorkAreaBoundsInScreen(GetDefaultSnappedWindow());
  return std::max(work_area_bounds.width(), work_area_bounds.height());
}

void SplitViewController::OnSnappedWindowMinimizedOrDestroyed(
    aura::Window* window) {
  DCHECK(window);
  DCHECK(window == left_window_ || window == right_window_);
  if (left_window_ == window) {
    StopObserving(left_window_);
    left_window_ = nullptr;
  } else {
    StopObserving(right_window_);
    right_window_ = nullptr;
  }

  if (!left_window_ && !right_window_) {
    // If there is no snapped window at this moment, ends split view mode. Note
    // this will update overview window grid bounds if the overview mode is
    // active at the moment.
    EndSplitView();
  } else {
    // If there is still one snapped window after minimizing/closing one snapped
    // window, update its snap state and open overview window grid.
    State previous_state = state_;
    state_ = left_window_ ? LEFT_SNAPPED : RIGHT_SNAPPED;
    default_snap_position_ = left_window_ ? LEFT : RIGHT;
    NotifySplitViewStateChanged(previous_state, state_);
    StartOverview();
  }
}

void SplitViewController::AdjustSnappedWindowBounds(
    gfx::Rect* left_or_top_rect,
    gfx::Rect* right_or_bottom_rect) {
  aura::Window* left_or_top_window =
      IsCurrentScreenOrientationPrimary() ? left_window_ : right_window_;
  aura::Window* right_or_bottom_window =
      IsCurrentScreenOrientationPrimary() ? right_window_ : left_window_;

  const bool is_landscape = IsCurrentScreenOrientationLandscape();
  const int left_minimum_width =
      GetMinimumWindowSize(left_or_top_window, is_landscape);
  const int right_minimum_width =
      GetMinimumWindowSize(right_or_bottom_window, is_landscape);

  if (!is_landscape) {
    TransposeRect(left_or_top_rect);
    TransposeRect(right_or_bottom_rect);
  }

  if (left_or_top_rect->width() < left_minimum_width)
    left_or_top_rect->set_width(left_minimum_width);
  if (right_or_bottom_rect->width() < right_minimum_width) {
    right_or_bottom_rect->set_x(
        right_or_bottom_rect->x() -
        (right_minimum_width - right_or_bottom_rect->width()));
    right_or_bottom_rect->set_width(right_minimum_width);
  }

  if (!is_landscape) {
    TransposeRect(left_or_top_rect);
    TransposeRect(right_or_bottom_rect);
  }
}

float SplitViewController::FindClosestPositionRatio(float distance,
                                                    float length) {
  float current_ratio = distance / length;
  float closest_ratio = 0.f;
  std::vector<float> position_ratios(
      kFixedPositionRatios,
      kFixedPositionRatios + sizeof(kFixedPositionRatios) / sizeof(float));
  GetDividerOptionalPositionRatios(&position_ratios);
  for (float ratio : position_ratios) {
    if (std::abs(current_ratio - ratio) <
        std::abs(current_ratio - closest_ratio)) {
      closest_ratio = ratio;
    }
  }
  return closest_ratio;
}

void SplitViewController::GetDividerOptionalPositionRatios(
    std::vector<float>* position_ratios) {
  bool is_left_or_top = IsCurrentScreenOrientationPrimary();
  aura::Window* left_or_top_window =
      is_left_or_top ? left_window_ : right_window_;
  aura::Window* right_or_bottom_window =
      is_left_or_top ? right_window_ : left_window_;
  bool is_landscape = IsCurrentScreenOrientationLandscape();

  int long_length = GetDividerEndPosition();
  float min_size_left_ratio = 0.f, min_size_right_ratio = 0.f;
  int min_left_size = 0, min_right_size = 0;
  if (left_or_top_window && left_or_top_window->delegate()) {
    gfx::Size min_size = left_or_top_window->delegate()->GetMinimumSize();
    min_left_size = is_landscape ? min_size.width() : min_size.height();
  }
  if (right_or_bottom_window && right_or_bottom_window->delegate()) {
    gfx::Size min_size = right_or_bottom_window->delegate()->GetMinimumSize();
    min_right_size = is_landscape ? min_size.width() : min_size.height();
  }

  min_size_left_ratio = static_cast<float>(min_left_size) / long_length;
  min_size_right_ratio = static_cast<float>(min_right_size) / long_length;
  if (min_size_left_ratio <= kOneThirdPositionRatio)
    position_ratios->push_back(kOneThirdPositionRatio);

  if (min_size_right_ratio <= kOneThirdPositionRatio)
    position_ratios->push_back(kTwoThirdPositionRatio);
}

aura::Window* SplitViewController::GetWindowForSmoothResize() {
  DCHECK(IsSplitViewModeActive());

  // If there is only one snapped window, return it.
  if (!left_window_ || !right_window_)
    return left_window_ ? left_window_ : right_window_;

  // If both of two snapped windows are Arc app windows or both are not Arc app
  // windows, return the one who is stacked above the other. Otherwise return
  // the one who is an Arc app window.
  if (IsArcAppWindow(left_window_) == IsArcAppWindow(right_window_))
    return GetWindowStackedAbove(left_window_, right_window_);
  else
    return IsArcAppWindow(left_window_) ? left_window_ : right_window_;
}

int SplitViewController::GetWindowComponentForResize(aura::Window* window) {
  if (window && (window == left_window_ || window == right_window_)) {
    switch (GetCurrentScreenOrientation()) {
      case OrientationLockType::kLandscapePrimary:
        return (window == left_window_) ? HTRIGHT : HTLEFT;
      case OrientationLockType::kLandscapeSecondary:
        return (window == left_window_) ? HTLEFT : HTRIGHT;
      case OrientationLockType::kPortraitSecondary:
        return (window == left_window_) ? HTTOP : HTBOTTOM;
      case OrientationLockType::kPortraitPrimary:
        return (window == left_window_) ? HTBOTTOM : HTTOP;
      default:
        return HTNOWHERE;
    }
  }
  return HTNOWHERE;
}

gfx::Point SplitViewController::GetEndDragLocationInScreen(
    aura::Window* window,
    const gfx::Point& location_in_screen) {
  gfx::Point end_location(location_in_screen);
  if (!window || (window != left_window_ && window != right_window_))
    return end_location;

  const gfx::Rect bounds = (window == left_window_)
                               ? GetSnappedWindowBoundsInScreen(window, LEFT)
                               : GetSnappedWindowBoundsInScreen(window, RIGHT);
  switch (GetCurrentScreenOrientation()) {
    case OrientationLockType::kLandscapePrimary:
      end_location.set_x(window == left_window_ ? bounds.right() : bounds.x());
      break;
    case OrientationLockType::kLandscapeSecondary:
      end_location.set_x(window == left_window_ ? bounds.x() : bounds.right());
      break;
    case OrientationLockType::kPortraitSecondary:
      end_location.set_y(window == left_window_ ? bounds.y() : bounds.bottom());
      break;
    case OrientationLockType::kPortraitPrimary:
      end_location.set_y(window == left_window_ ? bounds.bottom() : bounds.y());
      break;
    default:
      NOTREACHED();
      break;
  }
  return end_location;
}

void SplitViewController::RestoreAndActivateSnappedWindow(
    aura::Window* window) {
  DCHECK(window == left_window_ || window == right_window_);

  // If the snapped window comes from the overview window grid, calculate a good
  // starting transform based on the overview window item's bounds.
  gfx::Transform starting_transform;
  auto iter = overview_window_item_bounds_map_.find(window);
  if (iter != overview_window_item_bounds_map_.end()) {
    const gfx::Rect item_bounds = iter->second;
    overview_window_item_bounds_map_.erase(iter);

    // Calculate the starting transform based on the window's expected snapped
    // bounds and its window item bounds in overview.
    const gfx::Rect snapped_bounds = GetSnappedWindowBoundsInScreen(
        window, (window == left_window_) ? LEFT : RIGHT);
    starting_transform = ScopedTransformOverviewWindow::GetTransformForRect(
        snapped_bounds, item_bounds);
  }

  // Restore the window's transform first if it's not identity.
  if (!window->layer()->GetTargetTransform().IsIdentity()) {
    for (auto* window_iter : wm::GetTransientTreeIterator(window)) {
      if (!starting_transform.IsIdentity())
        window_iter->SetTransform(starting_transform);
      DoSplitviewTransformAnimation(window_iter->layer(),
                                    SPLITVIEW_ANIMATION_RESTORE_OVERVIEW_WINDOW,
                                    gfx::Transform(), nullptr);
    }
  }
  wm::ActivateWindow(window);

  // Stack the other snapped window below the current active window so that the
  // two snapped window are always the top two windows when split view mode is
  // active.
  aura::Window* stacking_target =
      (window == left_window_) ? right_window_ : left_window_;
  if (stacking_target)
    window->parent()->StackChildBelow(stacking_target, window);
}

void SplitViewController::SetWindowsTransformDuringResizing() {
  DCHECK(IsSplitViewModeActive());
  const bool is_landscape = IsCurrentScreenOrientationLandscape();
  aura::Window* left_or_top_window =
      IsCurrentScreenOrientationPrimary() ? left_window_ : right_window_;
  aura::Window* right_or_bottom_window =
      IsCurrentScreenOrientationPrimary() ? right_window_ : left_window_;

  gfx::Rect left_or_top_rect, right_or_bottom_rect;
  GetSnappedWindowBoundsInScreenInternal(
      GetDefaultSnappedWindow(), &left_or_top_rect, &right_or_bottom_rect);

  gfx::Transform left_or_top_transform;
  if (left_or_top_window) {
    const int left_size =
        is_landscape ? left_or_top_rect.width() : left_or_top_rect.height();
    const int left_minimum_size =
        GetMinimumWindowSize(left_or_top_window, is_landscape);
    const int distance = left_size - left_minimum_size;
    if (distance < 0) {
      left_or_top_transform.Translate(is_landscape ? distance : 0,
                                      is_landscape ? 0 : distance);
    }
    SetTransform(left_or_top_window, left_or_top_transform);
  }

  gfx::Transform right_or_bottom_transform;
  if (right_or_bottom_window) {
    const int right_size = is_landscape ? right_or_bottom_rect.width()
                                        : right_or_bottom_rect.height();
    const int right_minimum_size =
        GetMinimumWindowSize(right_or_bottom_window, is_landscape);
    const int distance = right_size - right_minimum_size;
    if (distance < 0) {
      right_or_bottom_transform.Translate(is_landscape ? -distance : 0,
                                          is_landscape ? 0 : -distance);
    }
    SetTransform(right_or_bottom_window, right_or_bottom_transform);
  }

  if (black_scrim_layer_.get()) {
    black_scrim_layer_->SetTransform(left_or_top_transform.IsIdentity()
                                         ? right_or_bottom_transform
                                         : left_or_top_transform);
  }
}

void SplitViewController::RestoreWindowsTransformAfterResizing() {
  DCHECK(IsSplitViewModeActive());
  if (left_window_)
    SetTransform(left_window_, gfx::Transform());
  if (right_window_)
    SetTransform(right_window_, gfx::Transform());
  if (black_scrim_layer_.get())
    black_scrim_layer_->SetTransform(gfx::Transform());
}

void SplitViewController::SetTransform(aura::Window* window,
                                       const gfx::Transform& transform) {
  DCHECK(window);
  for (auto* window_iter : wm::GetTransientTreeIterator(window))
    window_iter->SetTransform(transform);
}

void SplitViewController::StartOverview() {
  if (!Shell::Get()->window_selector_controller()->IsSelecting())
    Shell::Get()->window_selector_controller()->ToggleOverview();
}

void SplitViewController::EndOverview() {
  if (Shell::Get()->window_selector_controller()->IsSelecting())
    Shell::Get()->window_selector_controller()->ToggleOverview();
}

}  // namespace ash
