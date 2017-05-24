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
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/wm/public/activation_change_observer.h"

namespace views {
class Textfield;
class Widget;
}

namespace ash {
class WindowSelectorDelegate;
class WindowSelectorItem;
class WindowSelectorTest;
class WindowGrid;

// The WindowSelector shows a grid of all of your windows, allowing to select
// one by clicking or tapping on it.
class ASH_EXPORT WindowSelector : public display::DisplayObserver,
                                  public aura::WindowObserver,
                                  public aura::client::ActivationChangeObserver,
                                  public views::TextfieldController {
 public:
  // Returns true if the window can be selected in overview mode.
  static bool IsSelectable(aura::Window* window);

  enum Direction { LEFT, UP, RIGHT, DOWN };

  using WindowList = std::vector<aura::Window*>;

  explicit WindowSelector(WindowSelectorDelegate* delegate);
  ~WindowSelector() override;

  // Initialize with the windows that can be selected.
  void Init(const WindowList& windows);

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

  WindowSelectorDelegate* delegate() { return delegate_; }

  bool restoring_minimized_windows() const {
    return restoring_minimized_windows_;
  }

  int text_filter_bottom() const { return text_filter_bottom_; }

  bool is_shut_down() const { return is_shut_down_; }

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& display) override;
  void OnDisplayRemoved(const display::Display& display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowDestroying(aura::Window* window) override;

  // aura::client::ActivationChangeObserver:
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

 private:
  friend class WindowSelectorTest;

  // Returns the aura::Window for |text_filter_widget_|.
  aura::Window* GetTextFilterWidgetWindow();

  // Position all of the windows in the overview.
  void PositionWindows(bool animate);

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
  bool ignore_activations_;

  // List of all the window overview grids, one for each root window.
  std::vector<std::unique_ptr<WindowGrid>> grid_list_;

  // Tracks the index of the root window the selection widget is in.
  size_t selected_grid_index_;

  // The following variables are used for metric collection purposes. All of
  // them refer to this particular overview session and are not cumulative:
  // The time when overview was started.
  base::Time overview_start_time_;

  // The number of arrow key presses.
  size_t num_key_presses_;

  // The number of items in the overview.
  size_t num_items_;

  // Indicates if the text filter is shown on screen (rather than above it).
  bool showing_text_filter_;

  // Window text filter widget. As the user writes on it, we filter the items
  // in the overview. It is also responsible for handling overview key events,
  // such as enter key to select.
  std::unique_ptr<views::Widget> text_filter_widget_;

  // Image used for text filter textfield.
  gfx::ImageSkia search_image_;

  // The current length of the string entered into the text filtering textfield.
  size_t text_filter_string_length_;

  // The number of times the text filtering textfield has been cleared of text
  // during this overview mode session.
  size_t num_times_textfield_cleared_;

  // Tracks whether minimized windows are currently being restored for overview
  // mode.
  bool restoring_minimized_windows_;

  // The distance between the top edge of the screen and the bottom edge of
  // the text filtering textfield.
  int text_filter_bottom_;

  bool is_shut_down_ = false;

  DISALLOW_COPY_AND_ASSIGN(WindowSelector);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_H_
