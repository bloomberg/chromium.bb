// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_SESSION_H_
#define ASH_WM_OVERVIEW_OVERVIEW_SESSION_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/scoped_overview_hide_windows.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/activation_change_observer.h"

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ui {
class KeyEvent;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace ash {

class OverviewWindowDragController;
class SplitViewDragIndicators;
class OverviewGrid;
class OverviewDelegate;
class OverviewItem;

enum class IndicatorState;

// The Overview shows a grid of all of your windows, allowing to select
// one by clicking or tapping on it.
class ASH_EXPORT OverviewSession : public display::DisplayObserver,
                                   public aura::WindowObserver,
                                   public ui::EventHandler,
                                   public SplitViewController::Observer {
 public:
  enum Direction { LEFT, UP, RIGHT, DOWN };

  enum class OverviewTransition {
    kEnter,       // In the entering process of overview.
    kInOverview,  // Already in overview.
    kExit         // In the exiting process of overview.
  };

  // Enum describing the different ways overview can be entered or exited.
  enum class EnterExitOverviewType {
    // The default way, window(s) animate from their initial bounds to the grid
    // bounds. Window(s) that are not visible to the user do not get animated.
    // This should always be the type when in clamshell mode.
    kNormal,
    // When going to or from a state which all window(s) are minimized, slides
    // the windows in or out. This will minimize windows on exit if needed, so
    // that we do not need to add a delayed observer to handle minimizing the
    // windows after overview exit animations are finished.
    kWindowsMinimized,
    // Overview can be closed by swiping up from the shelf. In this mode, the
    // call site will handle shifting the bounds of the windows, so overview
    // code does not need to handle any animations. This is an exit only type.
    kSwipeFromShelf,
    // Overview can be opened by start dragging a window from top or be closed
    // if the dragged window restores back to maximized/full-screened. On enter
    // this mode is same as kNormal, except when all windows are minimized, the
    // launcher does not animate in. On exit this mode is used to avoid the
    // update bounds animation of the windows in overview grid on overview mode
    // ended.
    kWindowDragged
  };

  // Callback which fills out the passed settings object. Used by several
  // functions so different callers can do similar animations with different
  // settings.
  using UpdateAnimationSettingsCallback =
      base::RepeatingCallback<void(ui::ScopedLayerAnimationSettings* settings,
                                   bool observe)>;

  using WindowList = std::vector<aura::Window*>;

  explicit OverviewSession(OverviewDelegate* delegate);
  ~OverviewSession() override;

  // Initialize with the windows that can be selected.
  void Init(const WindowList& windows, const WindowList& hide_windows);

  // Perform cleanup that cannot be done in the destructor.
  void Shutdown();

  // Cancels window selection.
  void CancelSelection();

  // Called when the last overview item from a grid is deleted.
  void OnGridEmpty(OverviewGrid* grid);

  // Moves the current selection by |increment| items. Positive values of
  // |increment| move the selection forward, negative values move it backward.
  void IncrementSelection(int increment);

  // Accepts current selection if any. Returns true if a selection was made,
  // false otherwise.
  bool AcceptSelection();

  // Activates |item's| window.
  void SelectWindow(OverviewItem* item);

  // Called to set bounds for window grids. Used for split view.
  void SetBoundsForOverviewGridsInScreenIgnoringWindow(
      const gfx::Rect& bounds,
      OverviewItem* ignored_item);

  // Called to show or hide the split view drag indicators. This will do
  // nothing if split view is not enabled. |event_location| is used to reparent
  // |split_view_drag_indicators_|'s widget, if necessary.
  void SetSplitViewDragIndicatorsIndicatorState(
      IndicatorState indicator_state,
      const gfx::Point& event_location);

  // Retrieves the window grid whose root window matches |root_window|. Returns
  // nullptr if the window grid is not found.
  OverviewGrid* GetGridWithRootWindow(aura::Window* root_window);

  // Add |window| to the grid in |grid_list_| with the same root window. Does
  // nothing if the grid already contains |window|. And if |reposition| is true,
  // re-position all windows in the target window grid. If |animate| is true,
  // re-position with animation. This function may be called in two scenarios:
  // 1) when a item in split view mode was previously snapped but should now be
  // returned to the window grid (e.g. split view divider dragged to either
  // edge, or a window is snapped to a postion that already has a snapped
  // window); 2) when a window (not from overview) is dragged while overview is
  // open and the window is dropped on the drop target, the dragged window is
  // then added to the overview.
  void AddItem(aura::Window* window, bool reposition, bool animate);

  // Removes the overview item from the overview grid. And if
  // |reposition| is true, re-position all windows in the target overview grid.
  // This may be called in two scenarioes: 1) when a user drags an overview item
  // to snap to one side of the screen, the item should be removed from the
  // overview grid; 2) when a window (not from overview) ends its dragging while
  // overview is open, the drop target should be removed. Note in both cases,
  // the windows in the window grid do not need to be repositioned.
  void RemoveOverviewItem(OverviewItem* item, bool reposition);

  void InitiateDrag(OverviewItem* item, const gfx::Point& location_in_screen);
  void Drag(OverviewItem* item, const gfx::Point& location_in_screen);
  void CompleteDrag(OverviewItem* item, const gfx::Point& location_in_screen);
  void StartSplitViewDragMode(const gfx::Point& location_in_screen);
  void Fling(OverviewItem* item,
             const gfx::Point& location_in_screen,
             float velocity_x,
             float velocity_y);
  void ActivateDraggedWindow();
  void ResetDraggedWindowGesture();

  // Called when a window (either it's browser window or an app window)
  // start/continue/end being dragged in tablet mode.
  // TODO(xdai): Currently it doesn't work for multi-display scenario.
  void OnWindowDragStarted(aura::Window* dragged_window, bool animate);
  void OnWindowDragContinued(aura::Window* dragged_window,
                             const gfx::Point& location_in_screen,
                             IndicatorState indicator_state);
  void OnWindowDragEnded(aura::Window* dragged_window,
                         const gfx::Point& location_in_screen,
                         bool should_drop_window_into_overview);

  // Positions all of the windows in the overview, except |ignored_item|.
  void PositionWindows(bool animate, OverviewItem* ignored_item = nullptr);

  // Returns true if |window| is currently showing in overview.
  bool IsWindowInOverview(const aura::Window* window);

  // Set the window grid that's displaying in |root_window| not animate when
  // exiting overview mode, i.e., all window items in the grid will not animate
  // when exiting overview mode. It may be called in two cases: 1) When a window
  // gets snapped (either from overview or not) and thus cause the end of the
  // overview mode, we should not do the exiting animation; 2) When a window
  // is dragged around and when released, it causes the end of the overview
  // mode, we also should not do the exiting animation.
  void SetWindowListNotAnimatedWhenExiting(aura::Window* root_window);

  // Shifts and fades the grid in |grid_list_| associated with |location|.
  void UpdateGridAtLocationYPositionAndOpacity(
      int64_t display_id,
      int new_y,
      float opacity,
      const gfx::Rect& work_area,
      UpdateAnimationSettingsCallback callback);

  // Updates all the overview items' mask and shadow.
  void UpdateMaskAndShadow();

  // Called when the overview mode starting animation completes.
  void OnStartingAnimationComplete(bool canceled);

  // Returns true if any of the grids in |grid_list_| shield widgets are still
  // animating.
  bool IsOverviewGridAnimating();

  // Called when windows are being activated/deactivated during
  // overview mode.
  void OnWindowActivating(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active);

  // Gets the window which keeps focus for the duration of overview mode.
  aura::Window* GetOverviewFocusWindow();

  // Suspends/Resumes window re-positiong in overview.
  void SuspendReposition();
  void ResumeReposition();

  OverviewDelegate* delegate() { return delegate_; }

  SplitViewDragIndicators* split_view_drag_indicators() {
    return split_view_drag_indicators_.get();
  }

  EnterExitOverviewType enter_exit_overview_type() const {
    return enter_exit_overview_type_;
  }
  void set_enter_exit_overview_type(EnterExitOverviewType val) {
    enter_exit_overview_type_ = val;
  }

  OverviewWindowDragController* window_drag_controller() {
    return window_drag_controller_.get();
  }

  const std::vector<std::unique_ptr<OverviewGrid>>& grid_list_for_testing()
      const {
    return grid_list_;
  }

  size_t num_items_for_testing() const { return num_items_; }

  // display::DisplayObserver:
  void OnDisplayRemoved(const display::Display& display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowDestroying(aura::Window* window) override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // SplitViewController::Observer:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;
  void OnSplitViewDividerPositionChanged() override;

 private:
  friend class OverviewSessionTest;

  // |focus|, restores focus to the stored window.
  void ResetFocusRestoreWindow(bool focus);

  // Helper function that moves the selection widget to |direction| on the
  // corresponding window grid.
  void Move(Direction direction, bool animate);

  // Removes all observers that were registered during construction and/or
  // initialization.
  void RemoveAllObservers();

  // Called when the display area for the overview window grids changed.
  void OnDisplayBoundsChanged();

  // Returns true if all its window grids don't have any window item.
  bool IsEmpty();

  // Tracks observed windows.
  base::flat_set<aura::Window*> observed_windows_;

  // Weak pointer to the overview delegate which will be called when a selection
  // is made.
  OverviewDelegate* delegate_;

  // A weak pointer to the window which was focused on beginning window
  // selection. If window selection is canceled the focus should be restored to
  // this window.
  aura::Window* restore_focus_window_ = nullptr;

  // A hidden window that receives focus while in overview mode. It is needed
  // because accessibility needs something focused for it to work and we cannot
  // use one of the overview windows otherwise ::wm::ActivateWindow will not
  // work.
  // TODO(sammiequon): Investigate if we can focus the |selection_widget_| in
  // OverviewGrid when it is created, or if we can focus a widget from the
  // virtual desks UI when that is complete, or we may be able to add some
  // mechanism to trigger accessibility events without a focused window.
  std::unique_ptr<views::Widget> overview_focus_widget_;

  // True when performing operations that may cause window activations. This is
  // used to prevent handling the resulting expected activation. This is
  // initially true until this is initialized.
  bool ignore_activations_ = true;

  // List of all the window overview grids, one for each root window.
  std::vector<std::unique_ptr<OverviewGrid>> grid_list_;

  // The owner of the widget which displays splitview related information in
  // overview mode. This will be nullptr if split view is not enabled.
  std::unique_ptr<SplitViewDragIndicators> split_view_drag_indicators_;

  // Tracks the index of the root window the selection widget is in.
  size_t selected_grid_index_ = 0;

  // The following variables are used for metric collection purposes. All of
  // them refer to this particular overview session and are not cumulative:
  // The time when overview was started.
  base::Time overview_start_time_;

  // The number of arrow key presses.
  size_t num_key_presses_ = 0;

  // The number of items in the overview.
  size_t num_items_ = 0;

  // Stores the overview enter/exit type. See the enum declaration for
  // information on how these types affect overview mode.
  EnterExitOverviewType enter_exit_overview_type_ =
      EnterExitOverviewType::kNormal;

  // The selected item when exiting overview mode. nullptr if no window
  // selected.
  OverviewItem* selected_item_ = nullptr;

  // The drag controller for a window in the overview mode.
  std::unique_ptr<OverviewWindowDragController> window_drag_controller_;

  std::unique_ptr<ScopedOverviewHideWindows> hide_overview_windows_;

  DISALLOW_COPY_AND_ASSIGN(OverviewSession);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_SESSION_H_
