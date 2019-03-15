// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_GRID_H_
#define ASH_WM_OVERVIEW_OVERVIEW_GRID_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "ash/rotator/screen_rotation_animator_observer.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/window_state_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ui {
class Shadow;
}

namespace views {
class Widget;
}

namespace ash {

class DesksBarView;
class OverviewItem;

// Represents a grid of windows in the Overview Mode in a particular root
// window, and manages a selection widget that can be moved with the arrow keys.
// The idea behind the movement strategy is that it should be possible to access
// any window pressing a given arrow key repeatedly.
// +-------+  +-------+  +-------+
// |   0   |  |   1   |  |   2   |
// +-------+  +-------+  +-------+
// +-------+  +-------+  +-------+
// |   3   |  |   4   |  |   5   |
// +-------+  +-------+  +-------+
// +-------+
// |   6   |
// +-------+
// Example sequences:
//  - Going right to left
//    0, 1, 2, 3, 4, 5, 6
// The selector is switched to the next window grid (if available) or wrapped if
// it reaches the end of its movement sequence.
class ASH_EXPORT OverviewGrid : public aura::WindowObserver,
                                public wm::WindowStateObserver,
                                public ScreenRotationAnimatorObserver {
 public:
  OverviewGrid(aura::Window* root_window,
               const std::vector<aura::Window*>& window_list,
               OverviewSession* overview_session,
               const gfx::Rect& bounds_in_screen);
  ~OverviewGrid() override;

  // Exits overview mode, fading out the |shield_widget_| if necessary.
  void Shutdown();

  // Prepares the windows in this grid for overview. This will restore all
  // minimized windows and ensure they are visible.
  void PrepareForOverview();

  // Positions all the windows in rows of equal height scaling each window to
  // fit that height. Optionally animates the windows to their targets when
  // |animate| is true. If |ignored_item| is not null and is an item in
  // |window_list_|, that item is not positioned. This is for split screen.
  // |transition| specifies the overview state when this function is called.
  void PositionWindows(bool animate,
                       OverviewItem* ignored_item = nullptr,
                       OverviewSession::OverviewTransition transition =
                           OverviewSession::OverviewTransition::kInOverview);

  // Updates |selected_index_| according to the specified |direction| and calls
  // MoveSelectionWidget(). Returns |true| if the new selection index is out of
  // this window grid bounds.
  bool Move(OverviewSession::Direction direction, bool animate);

  // Returns the target selected window, or NULL if there is none selected.
  OverviewItem* SelectedWindow() const;

  // Returns the OverviewItem if a window is contained in any of the
  // OverviewItems this grid owns. Returns nullptr if no such a OverviewItem
  // exist.
  OverviewItem* GetOverviewItemContaining(const aura::Window* window) const;

  // Adds |window| to the grid. Intended to be used by split view. |window|
  // cannot already be on the grid. If |reposition| is true, reposition all
  // window items in the grid after adding the item. If |animate| is true,
  // reposition with animation.
  void AddItem(aura::Window* window, bool reposition, bool animate);

  // Removes |overview_item| from the grid. If |reposition| is true, reposition
  // all window items in the grid after removing the item.
  void RemoveItem(OverviewItem* overview_item, bool reposition);

  // Sets bounds for the window grid and positions all windows in the grid.
  void SetBoundsAndUpdatePositions(const gfx::Rect& bounds_in_screen);
  void SetBoundsAndUpdatePositionsIgnoringWindow(const gfx::Rect& bounds,
                                                 OverviewItem* ignored_item);

  // Shows or hides the selection widget. To be called by an overview item when
  // it is dragged.
  void SetSelectionWidgetVisibility(bool visible);

  void ShowNoRecentsWindowMessage(bool visible);

  void UpdateCannotSnapWarningVisibility();

  // Called when any OverviewItem on any OverviewGrid has started/ended being
  // dragged.
  void OnSelectorItemDragStarted(OverviewItem* item);
  void OnSelectorItemDragEnded();

  // Called when a window (either it's browser window or an app window)
  // start/continue/end being dragged in tablet mode.
  void OnWindowDragStarted(aura::Window* dragged_window, bool animate);
  void OnWindowDragContinued(aura::Window* dragged_window,
                             const gfx::Point& location_in_screen,
                             IndicatorState indicator_state);
  void OnWindowDragEnded(aura::Window* dragged_window,
                         const gfx::Point& location_in_screen,
                         bool should_drop_window_into_overview);

  // Returns true if |window| is the placeholder window from the drop target.
  bool IsDropTargetWindow(aura::Window* window) const;

  // Returns the overview item that accociates with |drop_target_widget_|.
  // Returns nullptr if overview does not have the drop target.
  OverviewItem* GetDropTarget();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  // TODO(flackr): Handle window bounds changed in OverviewItem. See also
  // OnWindowPropertyChanged() below.
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // wm::WindowStateObserver:
  void OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                   mojom::WindowStateType old_type) override;

  // ScreenRotationAnimatorObserver:
  void OnScreenCopiedBeforeRotation() override;
  void OnScreenRotationAnimationFinished(ScreenRotationAnimator* animator,
                                         bool canceled) override;

  // Called when overview starting animation completes.
  void OnStartingAnimationComplete();

  // Checks if the grid needs to have the wallpaper animated. Returns false if
  // one of the grids windows covers the the entire workspace, true otherwise.
  bool ShouldAnimateWallpaper() const;

  bool IsNoItemsIndicatorLabelVisibleForTesting();

  gfx::Rect GetNoItemsIndicatorLabelBoundsForTesting() const;

  // Calculates |should_animate_when_entering_| and
  // |should_animate_when_exiting_| of the overview items based on where
  // the first MRU window covering the available workspace is found.
  // |selected_item| is not nullptr if |selected_item| is the selected item when
  // exiting overview mode.
  void CalculateWindowListAnimationStates(
      OverviewItem* selected_item,
      OverviewSession::OverviewTransition transition);

  // Do not animate the entire window list during exiting the overview. It's
  // used when splitview and overview mode are both active, selecting a window
  // will put the window in splitview mode and also end the overview mode. In
  // this case the windows in OverviewGrid should not animate when exiting the
  // overivew mode. These windows will use ZERO tween so that transforms will
  // reset at the end of animation.
  void SetWindowListNotAnimatedWhenExiting();

  // Starts a nudge, with |item| being the item that may be deleted. This method
  // calculates which items in |window_list_| are to be updated, and their
  // destination bounds and fills |nudge_data_| accordingly.
  void StartNudge(OverviewItem* item);

  // Moves items in |nudge_data_| towards their destination bounds based on
  // |value|, which must be between 0.0 and 1.0.
  void UpdateNudge(OverviewItem* item, double value);

  // Clears |nudge_data_|.
  void EndNudge();

  // Called after PositionWindows when entering overview from the home launcher
  // screen. Translates all windows vertically and animates to their final
  // locations.
  void SlideWindowsIn();

  // Update the y position and opacity of the entire grid. Does this by
  // transforming the grids |shield_widget_| and the windows in |window_list_|.
  // If |callback| is true transformation and opacity change should be animated.
  // The animation settings will be set by the caller via |callback|.
  void UpdateYPositionAndOpacity(
      int new_y,
      float opacity,
      const gfx::Rect& work_area,
      OverviewSession::UpdateAnimationSettingsCallback callback);

  // Returns the window of the overview item that contains |location_in_screen|.
  aura::Window* GetTargetWindowOnLocation(const gfx::Point& location_in_screen);

  // Returns true when the desks bar view is showing desks mini views (or will
  // show them once it is created).
  bool IsDesksBarViewActive() const;

  // Returns true if the grid has no more windows.
  bool empty() const { return window_list_.empty(); }

  // Returns how many overview items are in the grid.
  size_t size() const { return window_list_.size(); }

  // Returns true if the selection widget is active.
  bool is_selecting() const { return selection_widget_ != nullptr; }

  // Returns the root window in which the grid displays the windows.
  const aura::Window* root_window() const { return root_window_; }

  const std::vector<std::unique_ptr<OverviewItem>>& window_list() const {
    return window_list_;
  }

  OverviewSession* overview_session() { return overview_session_; }

  const gfx::Rect bounds() const { return bounds_; }

  bool should_animate_when_exiting() const {
    return should_animate_when_exiting_;
  }

  views::Widget* shield_widget() { return shield_widget_.get(); }

  views::Widget* drop_target_widget_for_testing() {
    return drop_target_widget_.get();
  }

  void set_suspend_reposition(bool value) { suspend_reposition_ = value; }

  const DesksBarView* GetDesksBarViewForTesting() const;

 private:
  class ShieldView;
  class TargetWindowObserver;
  friend class OverviewSessionTest;

  // Struct which holds data required to perform nudges.
  struct NudgeData {
    size_t index;
    gfx::RectF src;
    gfx::RectF dst;
  };

  // Initializes the screen shield widget.
  void InitShieldWidget(bool animate);

  // Internal function to initialize the selection widget.
  void InitSelectionWidget(OverviewSession::Direction direction);

  // Moves the selection widget to the specified |direction|.
  void MoveSelectionWidget(OverviewSession::Direction direction,
                           bool recreate_selection_widget,
                           bool out_of_bounds,
                           bool animate);

  // Moves the selection widget to the targeted window.
  void MoveSelectionWidgetToTarget(bool animate);

  // Gets the layout of the overview items. Layout is done in 2 stages
  // maintaining fixed MRU ordering.
  // 1. Optimal height is determined. In this stage |height| is bisected to find
  //    maximum height which still allows all the windows to fit.
  // 2. Row widths are balanced. In this stage the available width is reduced
  //    until some windows are no longer fitting or until the difference between
  //    the narrowest and the widest rows starts growing.
  // Overall this achieves the goals of maximum size for previews (or maximum
  // row height which is equivalent assuming fixed height), balanced rows and
  // minimal wasted space.
  std::vector<gfx::RectF> GetWindowRects(OverviewItem* ignored_item);

  // Attempts to fit all |out_rects| inside |bounds|. The method ensures that
  // the |out_rects| vector has appropriate size and populates it with the
  // values placing rects next to each other left-to-right in rows of equal
  // |height|. While fitting |out_rects| several metrics are collected that can
  // be used by the caller. |out_max_bottom| specifies the bottom that the rects
  // are extending to. |out_min_right| and |out_max_right| report the right
  // bound of the narrowest and the widest rows respectively. In-values of the
  // |out_max_bottom|, |out_min_right| and |out_max_right| parameters are
  // ignored and their values are always initialized inside this method. Returns
  // true on success and false otherwise.
  bool FitWindowRectsInBounds(const gfx::Rect& bounds,
                              int height,
                              OverviewItem* ignored_item,
                              std::vector<gfx::RectF>* out_rects,
                              int* out_max_bottom,
                              int* out_min_right,
                              int* out_max_right);

  // Calculates |overview_item|'s |should_animate_when_entering_|,
  // |should_animate_when_exiting_|. |selected| is true if |overview_item| is
  // the selected item when exiting overview mode.
  void CalculateOverviewItemAnimationState(
      OverviewItem* overview_item,
      bool* has_fullscreen_coverred,
      bool selected,
      OverviewSession::OverviewTransition transition);

  // Returns the overview item iterator that contains |window|.
  std::vector<std::unique_ptr<OverviewItem>>::iterator
  GetOverviewItemIterContainingWindow(aura::Window* window);

  // Adds the |dragged_window| into overview on drag ended. Might need to update
  // the window's bounds if it has been resized.
  void AddDraggedWindowIntoOverviewOnDragEnd(aura::Window* dragged_window);

  // Root window the grid is in.
  aura::Window* root_window_;

  // Pointer to the OverviewSession that spawned this grid.
  OverviewSession* overview_session_;

  // Vector containing all the windows in this grid.
  std::vector<std::unique_ptr<OverviewItem>> window_list_;

  ScopedObserver<aura::Window, OverviewGrid> window_observer_;
  ScopedObserver<wm::WindowState, OverviewGrid> window_state_observer_;

  // Widget that darkens the screen background.
  std::unique_ptr<views::Widget> shield_widget_;

  // A pointer to |shield_widget_|'s content view.
  ShieldView* shield_view_ = nullptr;

  // Widget that indicates to the user which is the selected window.
  std::unique_ptr<views::Widget> selection_widget_;

  // Shadow around the selector.
  std::unique_ptr<ui::Shadow> selector_shadow_;

  // The drop target widget. It has a plus sign in the middle. It's created when
  // a window (not from overview) is being dragged, and is destroyed when the
  // drag ends or overview mode is ended. When the dragged window is dropped
  // onto the drop target, the dragged window is added to the overview.
  std::unique_ptr<views::Widget> drop_target_widget_;

  // The observer of the target window, which is the window that the dragged
  // tabs are going to merge into after the drag ends. After the dragged tabs
  // merge into the target window, and if the target window is a minimized
  // window in overview and is not destroyed yet, we need to update the overview
  // minimized widget's content view so that it reflects the merge.
  std::unique_ptr<TargetWindowObserver> target_window_observer_;

  // Current selected window position.
  size_t selected_index_ = 0;

  // Number of columns in the grid.
  size_t num_columns_ = 0;

  // True only after all windows have been prepared for overview.
  bool prepared_for_overview_ = false;

  // True if the overview grid should animate when exiting overview mode. Note
  // even if it's true, it doesn't mean all window items in the grid should
  // animate when exiting overview, instead each window item's animation status
  // is controlled by its own |should_animate_when_exiting_|. But if it's false,
  // all window items in the grid don't have animation.
  bool should_animate_when_exiting_ = true;

  // This OverviewGrid's total bounds in screen coordinates.
  gfx::Rect bounds_;

  // Collection of the items which should be nudged. This should only be
  // non-empty if a nudge is in progress.
  std::vector<NudgeData> nudge_data_;

  // True to skip |PositionWindows()|. Used to avoid O(n^2) layout
  // when reposition windows in tablet overview mode.
  bool suspend_reposition_ = false;

  DISALLOW_COPY_AND_ASSIGN(OverviewGrid);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_GRID_H_
