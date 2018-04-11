// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/scoped_hide_overview_windows.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/wm/public/activation_change_observer.h"

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace views {
class Textfield;
class Widget;
}  // namespace views

namespace ash {

class OverviewWindowDragController;
class SplitViewDragIndicators;
class WindowGrid;
class WindowSelectorDelegate;
class WindowSelectorItem;
class WindowSelectorTest;

enum class IndicatorState;

// The WindowSelector shows a grid of all of your windows, allowing to select
// one by clicking or tapping on it.
class ASH_EXPORT WindowSelector : public display::DisplayObserver,
                                  public aura::WindowObserver,
                                  public ::wm::ActivationChangeObserver,
                                  public views::TextfieldController,
                                  public SplitViewController::Observer {
 public:
  // Returns true if the window can be selected in overview mode.
  static bool IsSelectable(const aura::Window* window);

  enum Direction { LEFT, UP, RIGHT, DOWN };

  enum class OverviewTransition {
    kEnter,  // In the entering process of overview.
    kExit    // In the exiting process of overview.
  };

  using WindowList = std::vector<aura::Window*>;

  explicit WindowSelector(WindowSelectorDelegate* delegate);
  ~WindowSelector() override;

  // Initialize with the windows that can be selected.
  void Init(const WindowList& windows, const WindowList& hide_windows);

  // Perform cleanup that cannot be done in the destructor.
  void Shutdown();

  // Cancels window selection.
  void CancelSelection();

  // Called when the last window selector item from a grid is deleted.
  void OnGridEmpty(WindowGrid* grid);

  // Moves the current selection by |increment| items. Positive values of
  // |increment| move the selection forward, negative values move it backward.
  void IncrementSelection(int increment);

  // Accepts current selection if any. Returns true if a selection was made,
  // false otherwise.
  bool AcceptSelection();

  // Activates |item's| window.
  void SelectWindow(WindowSelectorItem* item);

  // Called when |window| is about to get closed.
  void WindowClosing(WindowSelectorItem* window);

  // Called to set bounds for window grids. Used for split view.
  void SetBoundsForWindowGridsInScreen(const gfx::Rect& bounds);
  void SetBoundsForWindowGridsInScreenIgnoringWindow(
      const gfx::Rect& bounds,
      WindowSelectorItem* ignored_item);

  // Called to show or hide the split view drag indicators. This will do
  // nothing if split view is not enabled. |event_location| is used to reparent
  // |split_view_drag_indicators_|'s widget, if necessary.
  void SetSplitViewDragIndicatorsIndicatorState(
      IndicatorState indicator_state,
      const gfx::Point& event_location);
  // Retrieves the window grid whose root window matches |root_window|. Returns
  // nullptr if the window grid is not found.
  WindowGrid* GetGridWithRootWindow(aura::Window* root_window);

  // Add |window| to the grid in |grid_list_| with the same root window. Does
  // nothing if the grid already contains |window|.
  void AddItem(aura::Window* window);

  // Removes the window selector item from the overview window grid.
  void RemoveWindowSelectorItem(WindowSelectorItem* item);

  void InitiateDrag(WindowSelectorItem* item,
                    const gfx::Point& location_in_screen);
  void Drag(WindowSelectorItem* item, const gfx::Point& location_in_screen);
  void CompleteDrag(WindowSelectorItem* item,
                    const gfx::Point& location_in_screen);
  void ActivateDraggedWindow();
  void ResetDraggedWindowGesture();

  // Positions all of the windows in the overview.
  void PositionWindows(bool animate);

  // If we are in middle of ending overview mode.
  bool IsShuttingDown() const;

  // Checks if the grid associated with a given |root_window| needs to have the
  // wallpaper animated. Returns false if one of the grids windows covers the
  // the entire workspace, true otherwise.
  bool ShouldAnimateWallpaper(aura::Window* root_window);

  WindowSelectorDelegate* delegate() { return delegate_; }

  bool restoring_minimized_windows() const {
    return restoring_minimized_windows_;
  }

  int text_filter_bottom() const { return text_filter_bottom_; }

  const std::vector<std::unique_ptr<WindowGrid>>& grid_list_for_testing()
      const {
    return grid_list_;
  }

  SplitViewDragIndicators* split_view_drag_indicators() {
    return split_view_drag_indicators_.get();
  }

  OverviewWindowDragController* window_drag_controller() {
    return window_drag_controller_.get();
  }

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& display) override;
  void OnDisplayRemoved(const display::Display& display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowDestroying(aura::Window* window) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;
  void OnAttemptToReactivateWindow(aura::Window* request_active,
                                   aura::Window* actual_active) override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // SplitViewController::Observer:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;
  void OnSplitViewDividerPositionChanged() override;

 private:
  friend class WindowSelectorTest;

  // Returns the aura::Window for |text_filter_widget_|.
  aura::Window* GetTextFilterWidgetWindow();

  // Repositions and resizes |text_filter_widget_| on
  // DisplayMetricsChanged event.
  void RepositionTextFilterOnDisplayMetricsChange();

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
  std::set<aura::Window*> observed_windows_;

  // Weak pointer to the selector delegate which will be called when a
  // selection is made.
  WindowSelectorDelegate* delegate_;

  // A weak pointer to the window which was focused on beginning window
  // selection. If window selection is canceled the focus should be restored to
  // this window.
  aura::Window* restore_focus_window_;

  // True when performing operations that may cause window activations. This is
  // used to prevent handling the resulting expected activation.
  bool ignore_activations_ = false;

  // List of all the window overview grids, one for each root window.
  std::vector<std::unique_ptr<WindowGrid>> grid_list_;

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

  // Indicates if the text filter is shown on screen (rather than above it).
  bool showing_text_filter_ = false;

  // Window text filter widget. As the user writes on it, we filter the items
  // in the overview. It is also responsible for handling overview key events,
  // such as enter key to select.
  std::unique_ptr<views::Widget> text_filter_widget_;

  // The current length of the string entered into the text filtering textfield.
  size_t text_filter_string_length_ = 0;

  // The number of times the text filtering textfield has been cleared of text
  // during this overview mode session.
  size_t num_times_textfield_cleared_ = 0;

  // Tracks whether minimized windows are currently being restored for overview
  // mode.
  bool restoring_minimized_windows_ = false;

  // The distance between the top edge of the screen and the bottom edge of
  // the text filtering textfield.
  int text_filter_bottom_ = 0;

  // The selected item when exiting overview mode. nullptr if no window
  // selected.
  WindowSelectorItem* selected_item_ = nullptr;

  // The drag controller for a window in the overview mode.
  std::unique_ptr<OverviewWindowDragController> window_drag_controller_;

  std::unique_ptr<ScopedHideOverviewWindows> hide_overview_windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelector);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_H_
