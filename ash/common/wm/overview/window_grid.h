// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_OVERVIEW_WINDOW_GRID_H_
#define ASH_COMMON_WM_OVERVIEW_WINDOW_GRID_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <vector>

#include "ash/common/wm/overview/window_selector.h"
#include "ash/common/wm/window_state_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/aura/window_observer.h"

namespace views {
class Widget;
}

namespace wm {
class Shadow;
}

namespace ash {

class WindowSelectorItem;

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
class ASH_EXPORT WindowGrid : public aura::WindowObserver,
                              public wm::WindowStateObserver {
 public:
  WindowGrid(WmWindow* root_window,
             const std::vector<WmWindow*>& window_list,
             WindowSelector* window_selector);
  ~WindowGrid() override;

  // Exits overview mode, fading out the |shield_widget_| if necessary.
  void Shutdown();

  // Prepares the windows in this grid for overview. This will restore all
  // minimized windows and ensure they are visible.
  void PrepareForOverview();

  // Positions all the windows in rows of equal height scaling each window to
  // fit that height.
  // Layout is done in 2 stages maintaining fixed MRU ordering.
  // 1. Optimal height is determined. In this stage |height| is bisected to find
  //    maximum height which still allows all the windows to fit.
  // 2. Row widths are balanced. In this stage the available width is reduced
  //    until some windows are no longer fitting or until the difference between
  //    the narrowest and the widest rows starts growing.
  // Overall this achieves the goals of maximum size for previews (or maximum
  // row height which is equivalent assuming fixed height), balanced rows and
  // minimal wasted space.
  // Optionally animates the windows to their targets when |animate| is true.
  void PositionWindows(bool animate);

  // Updates |selected_index_| according to the specified |direction| and calls
  // MoveSelectionWidget(). Returns |true| if the new selection index is out of
  // this window grid bounds.
  bool Move(WindowSelector::Direction direction, bool animate);

  // Returns the target selected window, or NULL if there is none selected.
  WindowSelectorItem* SelectedWindow() const;

  // Returns true if a window is contained in any of the WindowSelectorItems
  // this grid owns.
  bool Contains(const WmWindow* window) const;

  // Dims the items whose titles do not contain |pattern| and prevents their
  // selection. The pattern has its accents removed and is converted to
  // lowercase in a l10n sensitive context.
  // If |pattern| is empty, no item is dimmed.
  void FilterItems(const base::string16& pattern);

  // Called when |window| is about to get closed. If the |window| is currently
  // selected the implementation fades out |selection_widget_| to transparent
  // opacity, effectively hiding the selector widget.
  void WindowClosing(WindowSelectorItem* window);

  // Returns true if the grid has no more windows.
  bool empty() const { return window_list_.empty(); }

  // Returns how many window selector items are in the grid.
  size_t size() const { return window_list_.size(); }

  // Returns true if the selection widget is active.
  bool is_selecting() const { return selection_widget_ != nullptr; }

  // Returns the root window in which the grid displays the windows.
  const WmWindow* root_window() const { return root_window_; }

  const std::vector<std::unique_ptr<WindowSelectorItem>>& window_list() const {
    return window_list_;
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  // TODO(flackr): Handle window bounds changed in WindowSelectorItem.
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;

  // wm::WindowStateObserver:
  void OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                   wm::WindowStateType old_type) override;

 private:
  friend class WindowSelectorTest;

  // Initializes the screen shield widget.
  void InitShieldWidget();

  // Internal function to initialize the selection widget.
  void InitSelectionWidget(WindowSelector::Direction direction);

  // Moves the selection widget to the specified |direction|.
  void MoveSelectionWidget(WindowSelector::Direction direction,
                           bool recreate_selection_widget,
                           bool out_of_bounds,
                           bool animate);

  // Moves the selection widget to the targeted window.
  void MoveSelectionWidgetToTarget(bool animate);

  // Attempts to fit all |rects| inside |bounds|. The method ensures that
  // the |rects| vector has appropriate size and populates it with the values
  // placing Rects next to each other left-to-right in rows of equal |height|.
  // While fitting |rects| several metrics are collected that can be used by the
  // caller. |max_bottom| specifies the bottom that the rects are extending to.
  // |min_right| and |max_right| report the right bound of the narrowest and the
  // widest rows respectively. In-values of the |max_bottom|, |min_right| and
  // |max_right| parameters are ignored and their values are always initialized
  // inside this method. Returns true on success and false otherwise.
  bool FitWindowRectsInBounds(const gfx::Rect& bounds,
                              int height,
                              std::vector<gfx::Rect>* rects,
                              int* max_bottom,
                              int* min_right,
                              int* max_right);

  // Root window the grid is in.
  WmWindow* root_window_;

  // Pointer to the window selector that spawned this grid.
  WindowSelector* window_selector_;

  // Vector containing all the windows in this grid.
  std::vector<std::unique_ptr<WindowSelectorItem>> window_list_;

  ScopedObserver<aura::Window, WindowGrid> window_observer_;
  ScopedObserver<wm::WindowState, WindowGrid> window_state_observer_;

  // Widget that darkens the screen background.
  std::unique_ptr<views::Widget> shield_widget_;

  // Widget that indicates to the user which is the selected window.
  std::unique_ptr<views::Widget> selection_widget_;

  // Shadow around the selector.
  std::unique_ptr<::wm::Shadow> selector_shadow_;

  // Current selected window position.
  size_t selected_index_;

  // Number of columns in the grid.
  size_t num_columns_;

  // True only after all windows have been prepared for overview.
  bool prepared_for_overview_;

  DISALLOW_COPY_AND_ASSIGN(WindowGrid);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_OVERVIEW_WINDOW_GRID_H_
