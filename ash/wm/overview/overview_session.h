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
#include "ash/public/cpp/split_view.h"
#include "ash/shell_observer.h"
#include "ash/wm/overview/scoped_overview_hide_windows.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/activation_change_observer.h"

namespace gfx {
class Point;
class PointF;
}  // namespace gfx

namespace ui {
class KeyEvent;
class ScopedLayerAnimationSettings;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace ash {
class OverviewDelegate;
class OverviewGrid;
class OverviewItem;
class OverviewWindowDragController;
class RoundedLabelWidget;
class SplitViewDragIndicators;

enum class IndicatorState;

// The Overview shows a grid of all of your windows, allowing to select
// one by clicking or tapping on it.
class ASH_EXPORT OverviewSession : public display::DisplayObserver,
                                   public aura::WindowObserver,
                                   public ui::EventHandler,
                                   public ShellObserver,
                                   public SplitViewObserver {
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
    kWindowDragged,
    // Used only when it's desired to exit overview mode immediately without
    // animations. This is used when performing the desk switch animation when
    // the source desk is in overview mode, while the target desk is not.
    // This should not be used for entering overview mode.
    kImmediateExit
  };

  // Callback which fills out the passed settings object. Used by several
  // functions so different callers can do similar animations with different
  // settings.
  using UpdateAnimationSettingsCallback =
      base::RepeatingCallback<void(ui::ScopedLayerAnimationSettings* settings)>;

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

  // Called to show or hide the split view drag indicators. This will do
  // nothing if split view is not enabled. |event_location| is used to reparent
  // |split_view_drag_indicators_|'s widget, if necessary.
  void SetSplitViewDragIndicatorsIndicatorState(
      IndicatorState indicator_state,
      const gfx::Point& event_location);

  // Retrieves the window grid whose root window matches |root_window|. Returns
  // nullptr if the window grid is not found.
  OverviewGrid* GetGridWithRootWindow(aura::Window* root_window);

  // Adds |window| at the specified |index| into the grid with the same root
  // window. Does nothing if that grid does not exist in |grid_list_| or already
  // contains |window|. If |reposition| is true, repositions all items in the
  // target grid (unless it already contained |window|), except those in
  // |ignored_items|. If |animate| is true, animates the repositioning.
  // |animate| has no effect if |reposition| is false.
  void AddItem(aura::Window* window,
               bool reposition,
               bool animate,
               const base::flat_set<OverviewItem*>& ignored_items = {},
               size_t index = 0);

  // Similar to the above function, but adds the window at the end of the grid.
  void AppendItem(aura::Window* window, bool reposition, bool animate);

  // Removes |overview_item| from the corresponding grid. No items are
  // repositioned.
  void RemoveItem(OverviewItem* overview_item);

  void InitiateDrag(OverviewItem* item, const gfx::PointF& location_in_screen);
  void Drag(OverviewItem* item, const gfx::PointF& location_in_screen);
  void CompleteDrag(OverviewItem* item, const gfx::PointF& location_in_screen);
  void StartNormalDragMode(const gfx::PointF& location_in_screen);
  void Fling(OverviewItem* item,
             const gfx::PointF& location_in_screen,
             float velocity_x,
             float velocity_y);
  void ActivateDraggedWindow();
  void ResetDraggedWindowGesture();

  // Called when a window (either it's browser window or an app window)
  // start/continue/end being dragged in tablet mode.
  // TODO(xdai): Currently it doesn't work for multi-display scenario.
  void OnWindowDragStarted(aura::Window* dragged_window, bool animate);
  void OnWindowDragContinued(aura::Window* dragged_window,
                             const gfx::PointF& location_in_screen,
                             IndicatorState indicator_state);
  void OnWindowDragEnded(aura::Window* dragged_window,
                         const gfx::PointF& location_in_screen,
                         bool should_drop_window_into_overview,
                         bool snap);

  // Positions all overview items except those in |ignored_items|.
  void PositionWindows(bool animate,
                       const base::flat_set<OverviewItem*>& ignored_items = {});

  // Returns true if |window| is currently showing in overview.
  bool IsWindowInOverview(const aura::Window* window);

  // Returns the overview item for |window|, or nullptr if |window| doesn't have
  // a corresponding item in overview mode.
  OverviewItem* GetOverviewItemForWindow(const aura::Window* window);

  // Set the window grid that's displaying in |root_window| not animate when
  // exiting overview mode, i.e., all window items in the grid will not animate
  // when exiting overview mode. It may be called in two cases: 1) When a window
  // gets snapped (either from overview or not) and thus cause the end of the
  // overview mode, we should not do the exiting animation; 2) When a window
  // is dragged around and when released, it causes the end of the overview
  // mode, we also should not do the exiting animation.
  void SetWindowListNotAnimatedWhenExiting(aura::Window* root_window);

  // Shifts and fades the grid in |grid_list_| associated with |location|.
  // Returns a ui::ScopedLayerAnimationSettings object for the caller to
  // observe.
  // TODO(sammiequon): Change |new_y| to use float.
  std::unique_ptr<ui::ScopedLayerAnimationSettings>
  UpdateGridAtLocationYPositionAndOpacity(
      int64_t display_id,
      int new_y,
      float opacity,
      UpdateAnimationSettingsCallback callback);

  // Updates all the overview items' mask and shadow.
  void UpdateMaskAndShadow();

  // Called when the overview mode starting animation completes.
  void OnStartingAnimationComplete(bool canceled);

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

  // Returns true if all its window grids don't have any window item.
  bool IsEmpty() const;

  // display::DisplayObserver:
  void OnDisplayRemoved(const display::Display& display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowDestroying(aura::Window* window) override;

  // ShelObserver:
  void OnShellDestroying() override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // SplitViewObserver:
  void OnSplitViewStateChanged(SplitViewState previous_state,
                               SplitViewState state) override;
  void OnSplitViewDividerPositionChanged() override;

  OverviewDelegate* delegate() { return delegate_; }

  bool is_shutting_down() const { return is_shutting_down_; }
  void set_is_shutting_down(bool is_shutting_down) {
    is_shutting_down_ = is_shutting_down;
  }

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

  RoundedLabelWidget* no_windows_widget_for_testing() {
    return no_windows_widget_.get();
  }

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

  void UpdateNoWindowsWidget();

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

  // A widget that is shown if we entered overview without any windows opened.
  std::unique_ptr<RoundedLabelWidget> no_windows_widget_;

  // True when performing operations that may cause window activations. This is
  // used to prevent handling the resulting expected activation. This is
  // initially true until this is initialized.
  bool ignore_activations_ = true;

  // True when overview mode is exiting.
  bool is_shutting_down_ = false;

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
