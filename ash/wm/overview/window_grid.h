// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_GRID_H_
#define ASH_WM_OVERVIEW_WINDOW_GRID_H_

#include <vector>

#include "ash/wm/overview/window_selector.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
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
//  - Going "top" to "bottom"
//    0, 3, 6, 1, 4, 2, 5
// The selector is switched to the next window grid (if available) or wrapped if
// it reaches the end of its movement sequence.
class ASH_EXPORT WindowGrid : public aura::WindowObserver {
 public:
  WindowGrid(aura::Window* root_window,
             const std::vector<aura::Window*>& window_list,
             WindowSelector* window_selector);
  virtual ~WindowGrid();

  // Prepares the windows in this grid for overview. This will restore all
  // minimized windows and ensure they are visible.
  void PrepareForOverview();

  // Positions all the windows in the grid.
  void PositionWindows(bool animate);

  // Updates |selected_index_| according to the specified |direction| and calls
  // MoveSelectionWidget(). Returns |true| if the new selection index is out of
  // this window grid bounds.
  bool Move(WindowSelector::Direction direction, bool animate);

  // Returns the target selected window, or NULL if there is none selected.
  WindowSelectorItem* SelectedWindow() const;

  // Returns true if a window is contained in any of the WindowSelectorItems
  // this grid owns.
  bool Contains(const aura::Window* window) const;

  // Dims the items whose titles do not contain |pattern| and prevents their
  // selection. The pattern has its accents removed and is converted to
  // lowercase in a l10n sensitive context.
  // If |pattern| is empty, no item is dimmed.
  void FilterItems(const base::string16& pattern);

  // Returns true if the grid has no more windows.
  bool empty() const { return window_list_.empty(); }

  // Returns how many window selector items are in the grid.
  size_t size() const { return window_list_.size(); }

  // Returns true if the selection widget is active.
  bool is_selecting() const { return selection_widget_ != NULL; }

  // Returns the root window in which the grid displays the windows.
  const aura::Window* root_window() const { return root_window_; }

  const std::vector<WindowSelectorItem*>& window_list() const {
    return window_list_.get();
  }

  // aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;
  // TODO(flackr): Handle window bounds changed in WindowSelectorItem.
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;

 private:
  friend class WindowSelectorTest;

  // Internal function to initialize the selection widget.
  void InitSelectionWidget(WindowSelector::Direction direction);

  // Moves the selection widget to the specified |direction|.
  void MoveSelectionWidget(WindowSelector::Direction direction,
                           bool recreate_selection_widget,
                           bool out_of_bounds,
                           bool animate);

  // Moves the selection widget to the targeted window.
  void MoveSelectionWidgetToTarget(bool animate);

  // Returns the target bounds of the currently selected item.
  const gfx::Rect GetSelectionBounds() const;

  // Root window the grid is in.
  aura::Window* root_window_;

  // Pointer to the window selector that spawned this grid.
  WindowSelector* window_selector_;

  // Vector containing all the windows in this grid.
  ScopedVector<WindowSelectorItem> window_list_;

  // Vector containing the observed windows.
  std::set<aura::Window*> observed_windows_;

  // Widget that indicates to the user which is the selected window.
  scoped_ptr<views::Widget> selection_widget_;

  // Current selected window position.
  size_t selected_index_;

  // Number of columns in the grid.
  size_t num_columns_;

  DISALLOW_COPY_AND_ASSIGN(WindowGrid);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_GRID_H_
