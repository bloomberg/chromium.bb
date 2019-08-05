// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_session.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_delegate.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/rounded_label_widget.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/switchable_windows.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Values for the no items indicator which appears when opening overview mode
// with no opened windows.
constexpr int kNoItemsIndicatorHeightDp = 32;
constexpr int kNoItemsIndicatorHorizontalPaddingDp = 16;
constexpr int kNoItemsIndicatorRoundingDp = 16;
constexpr int kNoItemsIndicatorVerticalPaddingDp = 8;
constexpr SkColor kNoItemsIndicatorBackgroundColor =
    SkColorSetA(SK_ColorBLACK, 204);
constexpr SkColor kNoItemsIndicatorTextColor = SK_ColorWHITE;

// Returns the bounds for the overview window grid according to the split view
// state. If split view mode is active, the overview window should open on the
// opposite side of the default snap window. If |divider_changed| is true, maybe
// clamp the bounds to a minimum size and shift the bounds offscreen.
gfx::Rect GetGridBoundsInScreen(aura::Window* root_window,
                                bool divider_changed) {
  gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          root_window);

  // If the shelf is in auto hide, overview will force it to be in auto hide
  // shown, but we want to place the thumbnails as if the shelf was shown, so
  // manually update the work area.
  if (Shelf::ForWindow(root_window)->GetVisibilityState() == SHELF_AUTO_HIDE) {
    const int inset = ShelfConstants::shelf_size();
    switch (Shelf::ForWindow(root_window)->alignment()) {
      case SHELF_ALIGNMENT_BOTTOM:
      case SHELF_ALIGNMENT_BOTTOM_LOCKED:
        work_area.Inset(0, 0, 0, inset);
        break;
      case SHELF_ALIGNMENT_LEFT:
        work_area.Inset(inset, 0, 0, 0);
        break;
      case SHELF_ALIGNMENT_RIGHT:
        work_area.Inset(0, 0, inset, 0);
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  if (!split_view_controller->InSplitViewMode())
    return work_area;

  SplitViewController::SnapPosition opposite_position =
      (split_view_controller->default_snap_position() ==
       SplitViewController::LEFT)
          ? SplitViewController::RIGHT
          : SplitViewController::LEFT;
  gfx::Rect bounds = split_view_controller->GetSnappedWindowBoundsInScreen(
      root_window, opposite_position);
  if (!divider_changed)
    return bounds;

  const bool landscape = IsCurrentScreenOrientationLandscape();
  const int min_length =
      (landscape ? work_area.width() : work_area.height()) / 3;
  const int current_length = landscape ? bounds.width() : bounds.height();

  if (current_length > min_length)
    return bounds;

  // Clamp bounds' length to the minimum length.
  if (landscape)
    bounds.set_width(min_length);
  else
    bounds.set_height(min_length);

  // The |opposite_position| will be physically on the left or top of the screen
  // (depending on whether the orientation is landscape or portrait
  //  respectively), if |opposite_position| is left AND current orientation is
  // primary, OR |opposite_position| is right AND current orientation is not
  // primary. This is an X-NOR condition.
  const bool primary = IsCurrentScreenOrientationPrimary();
  const bool left_or_top =
      (primary == (opposite_position == SplitViewController::LEFT));
  if (left_or_top) {
    // If we are shifting to the left or top we need to update the origin as
    // well.
    const int offset = min_length - current_length;
    bounds.Offset(landscape ? gfx::Vector2d(-offset, 0)
                            : gfx::Vector2d(0, -offset));
  }

  return bounds;
}

void EndOverview() {
  Shell::Get()->overview_controller()->EndOverview();
}

}  // namespace

OverviewSession::OverviewSession(OverviewDelegate* delegate)
    : delegate_(delegate),
      restore_focus_window_(window_util::GetFocusedWindow()),
      overview_start_time_(base::Time::Now()),
      highlight_controller_(
          std::make_unique<OverviewHighlightController>(this)) {
  DCHECK(delegate_);
  Shell::Get()->AddPreTargetHandler(this);
}

OverviewSession::~OverviewSession() {
  DCHECK(observed_windows_.empty());
  // Don't delete |window_drag_controller_| yet since the stack might be still
  // using it.
  if (window_drag_controller_) {
    window_drag_controller_->ResetOverviewSession();
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
        FROM_HERE, window_drag_controller_.release());
  }
}

// NOTE: The work done in Init() is not done in the constructor because it may
// cause other, unrelated classes, to make indirect method calls on a partially
// constructed object.
void OverviewSession::Init(const WindowList& windows,
                           const WindowList& hide_windows) {
  Shell::Get()->AddShellObserver(this);

  hide_overview_windows_ = std::make_unique<ScopedOverviewHideWindows>(
      std::move(hide_windows), /*force_hidden=*/false);
  if (restore_focus_window_)
    restore_focus_window_->AddObserver(this);

  if (ShouldAllowSplitView())
    split_view_drag_indicators_ = std::make_unique<SplitViewDragIndicators>();

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::sort(root_windows.begin(), root_windows.end(),
            [](const aura::Window* a, const aura::Window* b) {
              // Since we don't know if windows are vertically or horizontally
              // oriented we use both x and y position. This may be confusing
              // if you have 3 or more monitors which are not strictly
              // horizontal or vertical but that case is not yet supported.
              return (a->GetBoundsInScreen().x() + a->GetBoundsInScreen().y()) <
                     (b->GetBoundsInScreen().x() + b->GetBoundsInScreen().y());
            });

  for (auto* root : root_windows) {
    // Observed switchable containers for newly created windows on all root
    // windows.
    for (auto* container :
         GetSwitchableContainersForRoot(root, /*active_desk_only=*/true)) {
      container->AddObserver(this);
      observed_windows_.insert(container);
    }

    auto grid = std::make_unique<OverviewGrid>(
        root, windows, this,
        GetGridBoundsInScreen(root, /*divider_changed=*/false));
    num_items_ += grid->size();
    grid_list_.push_back(std::move(grid));
  }

  // The calls to OverviewGrid::PrepareForOverview() requires some
  // LayoutManagers to perform layouts so that windows are correctly visible and
  // properly animated in overview mode. Otherwise these layouts should be
  // suppressed during overview mode so they don't conflict with overview mode
  // animations.

  // Do not call PrepareForOverview until all items are added to window_list_
  // as we don't want to cause any window updates until all windows in
  // overview are observed. See http://crbug.com/384495.
  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_) {
    overview_grid->PrepareForOverview();

    // Do not animate if there is any window that is being dragged in the
    // grid.
    if (enter_exit_overview_type_ == EnterExitOverviewType::kImmediateEnter) {
      overview_grid->PositionWindows(/*animate=*/false);
    } else if (enter_exit_overview_type_ ==
               EnterExitOverviewType::kSlideInEnter) {
      overview_grid->PositionWindows(/*animate=*/false);
      overview_grid->SlideWindowsIn();
    } else {
      // EnterExitOverviewType::kSwipeFromShelf is an exit only type, so it
      // should not appear here.
      DCHECK_NE(enter_exit_overview_type_,
                EnterExitOverviewType::kSwipeFromShelf);
      overview_grid->PositionWindows(/*animate=*/true, /*ignored_items=*/{},
                                     OverviewTransition::kEnter);
    }
  }

  UpdateNoWindowsWidget();

  // Create the widget that will receive focus while in overview mode for
  // accessiblity purposes.
  overview_focus_widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
  params.bounds = gfx::Rect(0, 0, 2, 2);
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.name = "OverviewModeFocusedWidget";
  params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_StatusContainer);
  overview_focus_widget_->Init(params);

  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.Items", num_items_);

  Shell::Get()->split_view_controller()->AddObserver(this);

  display::Screen::GetScreen()->AddObserver(this);
  base::RecordAction(base::UserMetricsAction("WindowSelector_Overview"));
  // Send an a11y alert.
  Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
      AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED);

  ignore_activations_ = false;
}

// NOTE: The work done in Shutdown() is not done in the destructor because it
// may cause other, unrelated classes, to make indirect calls to
// restoring_minimized_windows() on a partially destructed object.
void OverviewSession::Shutdown() {
  // This should have been set already when the process of ending overview mode
  // began. See OverviewController::OnSelectionEnded().
  DCHECK(is_shutting_down_);

  Shell::Get()->RemovePreTargetHandler(this);
  Shell::Get()->RemoveShellObserver(this);

  // Stop observing screen metrics changes first to avoid auto-positioning
  // windows in response to work area changes from window activation.
  display::Screen::GetScreen()->RemoveObserver(this);

  // Stop observing split view state changes before restoring window focus.
  // Otherwise the activation of the window triggers OnSplitViewStateChanged()
  // that will call into this function again.
  Shell::Get()->split_view_controller()->RemoveObserver(this);

  size_t remaining_items = 0;
  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_) {
    // During shutdown, do not animate all windows in overview if we need to
    // animate the snapped window.
    if (overview_grid->should_animate_when_exiting() &&
        enter_exit_overview_type_ != EnterExitOverviewType::kImmediateExit) {
      overview_grid->CalculateWindowListAnimationStates(
          selected_item_ &&
                  selected_item_->overview_grid() == overview_grid.get()
              ? selected_item_
              : nullptr,
          OverviewTransition::kExit, /*target_bounds=*/{});
    }
    for (const auto& overview_item : overview_grid->window_list())
      overview_item->RestoreWindow(/*reset_transform=*/true);
    remaining_items += overview_grid->size();
  }

  // Setting focus after restoring windows' state avoids unnecessary animations.
  // No need to restore if we are sliding to the home launcher screen, as all
  // windows will be minimized.
  const bool should_focus =
      enter_exit_overview_type_ == EnterExitOverviewType::kNormal ||
      enter_exit_overview_type_ == EnterExitOverviewType::kImmediateExit;
  ResetFocusRestoreWindow(should_focus);
  RemoveAllObservers();

  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_)
    overview_grid->Shutdown();

  DCHECK(num_items_ >= remaining_items);
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.OverviewClosedItems",
                           num_items_ - remaining_items);
  UMA_HISTOGRAM_MEDIUM_TIMES("Ash.WindowSelector.TimeInOverview",
                             base::Time::Now() - overview_start_time_);

  grid_list_.clear();

  if (no_windows_widget_) {
    if (enter_exit_overview_type_ == EnterExitOverviewType::kImmediateExit) {
      ImmediatelyCloseWidgetOnExit(std::move(no_windows_widget_));
      return;
    }

    // Fade out the no windows widget. This animation continues past the
    // lifetime of |this|.
    FadeOutWidgetAndMaybeSlideOnExit(std::move(no_windows_widget_),
                                     OVERVIEW_ANIMATION_RESTORE_WINDOW);
  }
}

void OverviewSession::OnGridEmpty() {
  if (!IsEmpty())
    return;

  if (Shell::Get()->split_view_controller()->InTabletSplitViewMode())
    UpdateNoWindowsWidget();
  else
    EndOverview();
}

void OverviewSession::IncrementSelection(int increment) {
  Move(increment < 0);
}

bool OverviewSession::AcceptSelection() {
  if (!highlight_controller_->GetHighlightedItem())
    return false;
  SelectWindow(highlight_controller_->GetHighlightedItem());
  return true;
}

void OverviewSession::SelectWindow(OverviewItem* item) {
  aura::Window* window = item->GetWindow();
  aura::Window::Windows window_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  if (!window_list.empty()) {
    // Record WindowSelector_ActiveWindowChanged if the user is selecting a
    // window other than the window that was active prior to entering overview
    // mode (i.e., the window at the front of the MRU list).
    if (window_list[0] != window) {
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_ActiveWindowChanged"));
      Shell::Get()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(
          TaskSwitchSource::OVERVIEW_MODE);
    }
    const auto it = std::find(window_list.begin(), window_list.end(), window);
    if (it != window_list.end()) {
      // Record 1-based index so that selecting a top MRU window will record 1.
      UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.SelectionDepth",
                               1 + it - window_list.begin());
    }
  }
  item->EnsureVisible();
  wm::ActivateWindow(window);
}

void OverviewSession::SetSplitViewDragIndicatorsIndicatorState(
    IndicatorState indicator_state,
    const gfx::Point& event_location) {
  DCHECK(split_view_drag_indicators_);
  split_view_drag_indicators_->SetIndicatorState(indicator_state,
                                                 event_location);
}

OverviewGrid* OverviewSession::GetGridWithRootWindow(
    aura::Window* root_window) {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    if (grid->root_window() == root_window)
      return grid.get();
  }

  return nullptr;
}

void OverviewSession::AddItem(
    aura::Window* window,
    bool reposition,
    bool animate,
    const base::flat_set<OverviewItem*>& ignored_items,
    size_t index) {
  // Early exit if a grid already contains |window|.
  OverviewGrid* grid = GetGridWithRootWindow(window->GetRootWindow());
  if (!grid || grid->GetOverviewItemContaining(window))
    return;

  grid->AddItem(window, reposition, animate, ignored_items, index);
  ++num_items_;

  UpdateNoWindowsWidget();

  // Transfer focus from |window| to |overview_focus_widget_| to match the
  // behavior of entering overview mode in the beginning.
  DCHECK(overview_focus_widget_);
  wm::ActivateWindow(GetOverviewFocusWindow());
}

void OverviewSession::AppendItem(aura::Window* window,
                                 bool reposition,
                                 bool animate) {
  // Early exit if a grid already contains |window|.
  OverviewGrid* grid = GetGridWithRootWindow(window->GetRootWindow());
  if (!grid || grid->GetOverviewItemContaining(window))
    return;

  grid->AppendItem(window, reposition, animate);
  ++num_items_;

  UpdateNoWindowsWidget();

  // Transfer focus from |window| to |overview_focus_widget_| to match the
  // behavior of entering overview mode in the beginning.
  DCHECK(overview_focus_widget_);
  wm::ActivateWindow(GetOverviewFocusWindow());
}

void OverviewSession::RemoveItem(OverviewItem* overview_item) {
  if (overview_item->GetWindow()->HasObserver(this)) {
    overview_item->GetWindow()->RemoveObserver(this);
    observed_windows_.erase(overview_item->GetWindow());
    if (overview_item->GetWindow() == restore_focus_window_)
      restore_focus_window_ = nullptr;
  }

  overview_item->overview_grid()->RemoveItem(overview_item);
  --num_items_;

  UpdateNoWindowsWidget();
}

void OverviewSession::InitiateDrag(OverviewItem* item,
                                   const gfx::PointF& location_in_screen,
                                   bool allow_drag_to_close) {
  highlight_controller_->SetFocusHighlightVisibility(false);
  window_drag_controller_ = std::make_unique<OverviewWindowDragController>(
      this, item, allow_drag_to_close);
  window_drag_controller_->InitiateDrag(location_in_screen);

  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->OnSelectorItemDragStarted(item);
}

void OverviewSession::Drag(OverviewItem* item,
                           const gfx::PointF& location_in_screen) {
  DCHECK(window_drag_controller_);
  DCHECK_EQ(item, window_drag_controller_->item());
  window_drag_controller_->Drag(location_in_screen);
}

void OverviewSession::CompleteDrag(OverviewItem* item,
                                   const gfx::PointF& location_in_screen) {
  DCHECK(window_drag_controller_);
  DCHECK_EQ(item, window_drag_controller_->item());
  const bool snap =
      window_drag_controller_->CompleteDrag(location_in_screen) ==
      OverviewWindowDragController::DragResult::kSuccessfulDragToSnap;
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->OnSelectorItemDragEnded(snap);

  highlight_controller_->SetFocusHighlightVisibility(true);
}

void OverviewSession::StartNormalDragMode(
    const gfx::PointF& location_in_screen) {
  window_drag_controller_->StartNormalDragMode(location_in_screen);
}

void OverviewSession::Fling(OverviewItem* item,
                            const gfx::PointF& location_in_screen,
                            float velocity_x,
                            float velocity_y) {
  // Its possible a fling event is not paired with a tap down event. Ignore
  // these flings.
  if (!window_drag_controller_ || item != window_drag_controller_->item())
    return;

  const bool snap =
      window_drag_controller_->Fling(location_in_screen, velocity_x,
                                     velocity_y) ==
      OverviewWindowDragController::DragResult::kSuccessfulDragToSnap;
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->OnSelectorItemDragEnded(snap);
}

void OverviewSession::ActivateDraggedWindow() {
  window_drag_controller_->ActivateDraggedWindow();
}

void OverviewSession::ResetDraggedWindowGesture() {
  window_drag_controller_->ResetGesture();
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->OnSelectorItemDragEnded(/*snap=*/false);
}

void OverviewSession::OnWindowDragStarted(aura::Window* dragged_window,
                                          bool animate) {
  OverviewGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragStarted(dragged_window, animate);
}

void OverviewSession::OnWindowDragContinued(
    aura::Window* dragged_window,
    const gfx::PointF& location_in_screen,
    IndicatorState indicator_state) {
  OverviewGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragContinued(dragged_window, location_in_screen,
                                     indicator_state);
}
void OverviewSession::OnWindowDragEnded(aura::Window* dragged_window,
                                        const gfx::PointF& location_in_screen,
                                        bool should_drop_window_into_overview,
                                        bool snap) {
  OverviewGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragEnded(dragged_window, location_in_screen,
                                 should_drop_window_into_overview, snap);
}

void OverviewSession::PositionWindows(
    bool animate,
    const base::flat_set<OverviewItem*>& ignored_items) {
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_)
    grid->PositionWindows(animate, ignored_items);

  RefreshNoWindowsWidgetBounds(animate);
}

bool OverviewSession::IsWindowInOverview(const aura::Window* window) {
  for (const std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    if (grid->GetOverviewItemContaining(window))
      return true;
  }
  return false;
}

OverviewItem* OverviewSession::GetOverviewItemForWindow(
    const aura::Window* window) {
  for (const std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    OverviewItem* item = grid->GetOverviewItemContaining(window);
    if (item)
      return item;
  }

  return nullptr;
}

void OverviewSession::SetWindowListNotAnimatedWhenExiting(
    aura::Window* root_window) {
  // Find the grid accociated with |root_window|.
  OverviewGrid* grid = GetGridWithRootWindow(root_window);
  if (grid)
    grid->SetWindowListNotAnimatedWhenExiting();
}

std::unique_ptr<ui::ScopedLayerAnimationSettings>
OverviewSession::UpdateGridAtLocationYPositionAndOpacity(
    int64_t display_id,
    int new_y,
    float opacity,
    UpdateAnimationSettingsCallback callback) {
  OverviewGrid* grid = GetGridWithRootWindow(
      Shell::Get()->GetRootWindowForDisplayId(display_id));
  if (!grid)
    return nullptr;

  if (no_windows_widget_) {
    // Translate and fade |no_windows_widget_| if it is visible.
    DCHECK(grid->empty());
    aura::Window* window = no_windows_widget_->GetNativeWindow();
    std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
    if (!callback.is_null()) {
      settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
          window->layer()->GetAnimator());
      callback.Run(settings.get());
    }
    window->SetTransform(
        gfx::Transform(1.f, 0.f, 0.f, 1.f, 0.f, static_cast<float>(new_y)));
    window->layer()->SetOpacity(opacity);
    return settings;
  }

  return grid->UpdateYPositionAndOpacity(new_y, opacity, callback);
}

void OverviewSession::UpdateRoundedCornersAndShadow() {
  for (auto& grid : grid_list_)
    for (auto& window : grid->window_list())
      window->UpdateRoundedCornersAndShadow();
}

void OverviewSession::OnStartingAnimationComplete(bool canceled) {
  for (auto& grid : grid_list_)
    grid->OnStartingAnimationComplete(canceled);

  if (!canceled) {
    if (overview_focus_widget_)
      overview_focus_widget_->Show();
    Shell::Get()->overview_controller()->DelayedUpdateRoundedCornersAndShadow();
  }
}

void OverviewSession::OnWindowActivating(
    ::wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (ignore_activations_ || gained_active == GetOverviewFocusWindow())
    return;

  if (features::IsVirtualDesksEnabled() &&
      DesksController::Get()->AreDesksBeingModified()) {
    // Activating a desk from its mini view will activate its most-recently used
    // window, but this should not result in ending overview mode now.
    // Overview will be ended explicitly as part of the desk activation
    // animation.
    return;
  }

  if (!gained_active) {
    // Cancel overview session and do not restore focus when active window is
    // set to nullptr. This happens when removing a display.
    ResetFocusRestoreWindow(false);
    EndOverview();
    return;
  }

  auto* grid = GetGridWithRootWindow(gained_active->GetRootWindow());
  if (!grid)
    return;

  // Do not cancel overview mode if the window activation was caused by
  // snapping window to one side of the screen.
  if (Shell::Get()->split_view_controller()->InSplitViewMode())
    return;

  // Do not cancel overview mode if the window activation was caused while
  // dragging overview mode offscreen.
  if (IsSlidingOutOverviewFromShelf())
    return;

  // Don't restore focus on exit if a window was just activated.
  ResetFocusRestoreWindow(false);

  // Do not cancel overview mode while a window or overview item is being
  // dragged as evidenced by the presence of a drop target. (Dragging to close
  // does not count; canceling overview mode is okay then.)
  for (std::unique_ptr<OverviewGrid>& overview_grid : grid_list_) {
    if (overview_grid->GetDropTarget())
      return;
  }

  const auto& windows = grid->window_list();
  auto iter = std::find_if(
      windows.begin(), windows.end(),
      [gained_active](const std::unique_ptr<OverviewItem>& window) {
        return window->Contains(gained_active);
      });

  if (iter != windows.end())
    selected_item_ = iter->get();
  EndOverview();
}

aura::Window* OverviewSession::GetOverviewFocusWindow() {
  if (overview_focus_widget_)
    return overview_focus_widget_->GetNativeWindow();

  return nullptr;
}

aura::Window* OverviewSession::GetHighlightedWindow() {
  OverviewItem* item = highlight_controller_->GetHighlightedItem();
  if (!item)
    return nullptr;
  return item->GetWindow();
}

void OverviewSession::SuspendReposition() {
  for (auto& grid : grid_list_)
    grid->set_suspend_reposition(true);
}

void OverviewSession::ResumeReposition() {
  for (auto& grid : grid_list_)
    grid->set_suspend_reposition(false);
}

bool OverviewSession::IsEmpty() const {
  for (const auto& grid : grid_list_) {
    if (!grid->empty())
      return false;
  }
  return true;
}

void OverviewSession::OnHighlightedItemActivated(OverviewItem* item) {
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.ArrowKeyPresses",
                           num_key_presses_);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.WindowSelector.KeyPressesOverItemsRatio",
                              (num_key_presses_ * 100) / num_items_, 1, 300,
                              30);
  base::RecordAction(
      base::UserMetricsAction("WindowSelector_OverviewEnterKey"));
  SelectWindow(item);
}

void OverviewSession::OnHighlightedItemClosed(OverviewItem* item) {
  base::RecordAction(
      base::UserMetricsAction("WindowSelector_OverviewCloseKey"));
  item->CloseWindow();
}

void OverviewSession::OnDisplayRemoved(const display::Display& display) {
  // TODO(flackr): Keep window selection active on remaining displays.
  EndOverview();
}

void OverviewSession::OnDisplayMetricsChanged(const display::Display& display,
                                              uint32_t metrics) {
  // For metrics changes that happen when the split view mode is active, the
  // display bounds will be adjusted in OnSplitViewDividerPositionChanged().
  if (Shell::Get()->split_view_controller()->InSplitViewMode())
    return;
  OnDisplayBoundsChanged();
}

void OverviewSession::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  // Only care about newly added children of |observed_windows_|.
  if (!observed_windows_.count(params.receiver) ||
      !observed_windows_.count(params.new_parent)) {
    return;
  }

  // Removing a desk while in overview mode results in reparenting the windows
  // of that desk to the associated container of another desk. This is a window
  // hierarchy change that shouldn't result in exiting overview mode.
  if (features::IsVirtualDesksEnabled() &&
      DesksController::Get()->AreDesksBeingModified()) {
    return;
  }

  aura::Window* new_window = params.target;
  WindowState* state = WindowState::Get(new_window);
  if (!state->IsUserPositionable() || state->IsPip())
    return;

  // If the new window is added when splitscreen is active, do nothing.
  // SplitViewController will do the right thing to snap the window or end
  // overview mode.
  if (Shell::Get()->split_view_controller()->InSplitViewMode() &&
      new_window->GetRootWindow() == Shell::Get()
                                         ->split_view_controller()
                                         ->GetDefaultSnappedWindow()
                                         ->GetRootWindow()) {
    return;
  }

  if (IsSwitchableContainer(new_window->parent()) &&
      !::wm::GetTransientParent(new_window)) {
    // The new window is in one of the switchable containers, abort overview.
    EndOverview();
    return;
  }
}

void OverviewSession::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  observed_windows_.erase(window);
  if (window == restore_focus_window_)
    restore_focus_window_ = nullptr;
}

void OverviewSession::OnKeyEvent(ui::KeyEvent* event) {
  // If app list is open when overview is active (it can happen in clamshell
  // mode, when we snap an overview window to one side of the screen and then
  // open the app list to select an app to snap to the other side), in this case
  // we let the app list to handle the key event.
  // TODO(crbug.com/952315): Explore better ways to handle this splitview +
  // overview + applist case.
  Shell* shell = Shell::Get();
  if (shell->app_list_controller() &&
      shell->app_list_controller()->IsVisible() &&
      Shell::Get()->split_view_controller()->InClamshellSplitViewMode()) {
    return;
  }

  if (event->type() != ui::ET_KEY_PRESSED)
    return;

  switch (event->key_code()) {
    case ui::VKEY_BROWSER_BACK:
      FALLTHROUGH;
    case ui::VKEY_ESCAPE:
      // Cancel overview unless we're in single split mode with no overview
      // windows.
      if (!(IsEmpty() && shell->split_view_controller()->InSplitViewMode()))
        EndOverview();
      break;
    case ui::VKEY_UP:
      ++num_key_presses_;
      Move(/*reverse=*/true);
      break;
    case ui::VKEY_DOWN:
      ++num_key_presses_;
      Move(/*reverse=*/false);
      break;
    case ui::VKEY_RIGHT:
    case ui::VKEY_TAB:
      if (event->key_code() == ui::VKEY_RIGHT ||
          !(event->flags() & ui::EF_SHIFT_DOWN)) {
        ++num_key_presses_;
        Move(/*reverse=*/false);
        break;
      }
      FALLTHROUGH;
    case ui::VKEY_LEFT:
      ++num_key_presses_;
      Move(/*reverse=*/true);
      break;
    case ui::VKEY_W: {
      if (!(event->flags() & ui::EF_CONTROL_DOWN))
        return;

      if (!highlight_controller_->MaybeCloseHighlightedView())
        return;
      break;
    }
    case ui::VKEY_RETURN: {
      if (!highlight_controller_->MaybeActivateHighlightedView())
        return;
      break;
    }
    default:
      return;
  }

  event->SetHandled();
  event->StopPropagation();
}

void OverviewSession::OnShellDestroying() {
  // Cancel selection will call |Shutodnw()|, which will remove observer.
  EndOverview();
}

void OverviewSession::OnSplitViewStateChanged(SplitViewState previous_state,
                                              SplitViewState state) {
  // Do nothing if overview is being shutdown.
  if (!Shell::Get()->overview_controller()->InOverviewSession())
    return;

  const bool unsnappable_window_activated =
      state == SplitViewState::kNoSnap &&
      Shell::Get()->split_view_controller()->end_reason() ==
          SplitViewController::EndReason::kUnsnappableWindowActivated;

  // Restore focus unless either a window was just snapped (and activated) or
  // split view mode was ended by activating an unsnappable window.
  if (state != SplitViewState::kNoSnap || unsnappable_window_activated)
    ResetFocusRestoreWindow(false);

  // If two windows were snapped to both sides of the screen or an unsnappable
  // window was just activated, or we're in single split mode in clamshell mode
  // and there is no window in overview, end overview mode and bail out.
  if (state == SplitViewState::kBothSnapped || unsnappable_window_activated ||
      (Shell::Get()->split_view_controller()->InClamshellSplitViewMode() &&
       IsEmpty())) {
    EndOverview();
    return;
  }

  // Adjust the overview window grid bounds if overview mode is active.
  OnDisplayBoundsChanged();
  for (auto& grid : grid_list_)
    grid->UpdateCannotSnapWarningVisibility();

  // Notify |split_view_drag_indicators_| if split view mode ended.
  if (split_view_drag_indicators_ && state == SplitViewState::kNoSnap)
    split_view_drag_indicators_->OnSplitViewModeEnded();
}

void OverviewSession::OnSplitViewDividerPositionChanged() {
  DCHECK(Shell::Get()->split_view_controller()->InSplitViewMode());
  // Re-calculate the bounds for the window grids and position all the windows.
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    grid->SetBoundsAndUpdatePositions(
        GetGridBoundsInScreen(const_cast<aura::Window*>(grid->root_window()),
                              /*divider_changed=*/true),
        /*ignored_items=*/{});
  }
  PositionWindows(/*animate=*/false);
  UpdateNoWindowsWidget();
}

void OverviewSession::ResetFocusRestoreWindow(bool focus) {
  if (!restore_focus_window_)
    return;

  if (features::IsVirtualDesksEnabled()) {
    // Do not restore focus to a window that exists on an inactive desk.
    focus &= base::Contains(DesksController::Get()->active_desk()->windows(),
                            restore_focus_window_);
  }

  // Ensure the window is still in the window hierarchy and not in the middle
  // of teardown.
  if (focus && restore_focus_window_->GetRootWindow()) {
    base::AutoReset<bool> restoring_focus(&ignore_activations_, true);
    wm::ActivateWindow(restore_focus_window_);
  }
  // If the window is in the observed_windows_ list it needs to continue to be
  // observed.
  if (observed_windows_.find(restore_focus_window_) ==
      observed_windows_.end()) {
    restore_focus_window_->RemoveObserver(this);
  }
  restore_focus_window_ = nullptr;
}

void OverviewSession::Move(bool reverse) {
  // Do not allow moving the highlight while in the middle of a drag.
  if (window_drag_controller_ && window_drag_controller_->item())
    return;

  highlight_controller_->MoveHighlight(reverse);
}

void OverviewSession::RemoveAllObservers() {
  for (auto* window : observed_windows_)
    window->RemoveObserver(this);
  observed_windows_.clear();

  display::Screen::GetScreen()->RemoveObserver(this);
  if (restore_focus_window_)
    restore_focus_window_->RemoveObserver(this);
}

void OverviewSession::OnDisplayBoundsChanged() {
  // Re-calculate the bounds for the window grids and position all the windows.
  for (std::unique_ptr<OverviewGrid>& grid : grid_list_) {
    grid->SetBoundsAndUpdatePositions(
        GetGridBoundsInScreen(const_cast<aura::Window*>(grid->root_window()),
                              /*divider_changed=*/false),
        /*ignored_items=*/{});
  }
  PositionWindows(/*animate=*/false);
  UpdateNoWindowsWidget();
  if (split_view_drag_indicators_)
    split_view_drag_indicators_->OnDisplayBoundsChanged();
}

void OverviewSession::UpdateNoWindowsWidget() {
  if (is_shutting_down_)
    return;

  // Hide the widget if there is an item in overview.
  if (!IsEmpty()) {
    no_windows_widget_.reset();
    return;
  }

  if (!no_windows_widget_) {
    // Create and fade in the widget.
    RoundedLabelWidget::InitParams params;
    params.name = "OverviewNoWindowsLabel";
    params.horizontal_padding = kNoItemsIndicatorHorizontalPaddingDp;
    params.vertical_padding = kNoItemsIndicatorVerticalPaddingDp;
    params.background_color = kNoItemsIndicatorBackgroundColor;
    params.foreground_color = kNoItemsIndicatorTextColor;
    params.rounding_dp = kNoItemsIndicatorRoundingDp;
    params.preferred_height = kNoItemsIndicatorHeightDp;
    params.message_id = IDS_ASH_OVERVIEW_NO_RECENT_ITEMS;
    params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
        desks_util::GetActiveDeskContainerId());
    no_windows_widget_ = std::make_unique<RoundedLabelWidget>();
    no_windows_widget_->Init(params);

    aura::Window* widget_window = no_windows_widget_->GetNativeWindow();
    widget_window->SetProperty(kHideInDeskMiniViewKey, true);
    widget_window->parent()->StackChildAtBottom(widget_window);
    ScopedOverviewAnimationSettings settings(OVERVIEW_ANIMATION_NO_RECENTS_FADE,
                                             widget_window);
    no_windows_widget_->SetOpacity(1.f);
  }

  RefreshNoWindowsWidgetBounds(/*animate=*/false);
}

void OverviewSession::RefreshNoWindowsWidgetBounds(bool animate) {
  if (!no_windows_widget_)
    return;

  auto* grid = GetGridWithRootWindow(Shell::GetPrimaryRootWindow());
  DCHECK(grid);
  no_windows_widget_->SetBoundsCenteredIn(grid->GetGridEffectiveBounds(),
                                          animate);
}

}  // namespace ash
