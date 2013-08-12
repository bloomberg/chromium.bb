// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_SELECTOR_H_
#define ASH_WM_WINDOW_SELECTOR_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "ui/aura/window_observer.h"
#include "ui/base/events/event_handler.h"
#include "ui/gfx/transform.h"

namespace aura {
class RootWindow;
}

namespace ui {
class LocatedEvent;
}

namespace views {
class Widget;
}

namespace ash {

class WindowSelectorDelegate;
class WindowSelectorWindow;

// The WindowSelector shows a grid of all of your windows and allows selecting
// a window by clicking or tapping on it (OVERVIEW mode) or by alt-tabbing to
// it (CYCLE mode).
class WindowSelector : public ui::EventHandler,
                       public aura::WindowObserver {
 public:
  enum Direction {
    FORWARD,
    BACKWARD
  };
  enum Mode {
    CYCLE,
    OVERVIEW
  };

  typedef std::vector<aura::Window*> WindowList;

  WindowSelector(const WindowList& windows,
                 Mode mode,
                 WindowSelectorDelegate* delegate);
  virtual ~WindowSelector();

  // Step to the next window in |direction|.
  void Step(Direction direction);

  // Select the current window.
  void SelectWindow();

  Mode mode() { return mode_; }

  // ui::EventHandler:
  virtual void OnEvent(ui::Event* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // aura::WindowObserver:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

 private:
  // Returns the target of |event| or NULL if the event is not targeted at
  // any of the windows in the selector.
  WindowSelectorWindow* GetEventTarget(ui::LocatedEvent* event);

  // Handles a selection event for |target|.
  void HandleSelectionEvent(WindowSelectorWindow* target);

  // Position all of the windows based on the current selection mode.
  void PositionWindows();
  // Position all of the windows from |root_window| on |root_window|.
  void PositionWindowsFromRoot(aura::RootWindow* root_window);
  // Position all of the |windows| to fit on the |root_window|.
  void PositionWindowsOnRoot(aura::RootWindow* root_window,
                             const std::vector<WindowSelectorWindow*>& windows);

  void InitializeSelectionWidget();

  // Updates the selection widget's location to the currently selected window.
  // If |animate| the transition to the new location is animated.
  void UpdateSelectionLocation(bool animate);

  // The collection of windows in the overview wrapped by a helper class which
  // restores their state and helps transform them to other root windows.
  ScopedVector<WindowSelectorWindow> windows_;

  // The window selection mode.
  Mode mode_;

  // Weak pointer to the selector delegate which will be called when a
  // selection is made.
  WindowSelectorDelegate* delegate_;

  // Index of the currently selected window if the mode is CYCLE.
  size_t selected_window_;

  // Widget indicating which window is currently selected.
  scoped_ptr<views::Widget> selection_widget_;

  // In CYCLE mode, the root window in which selection is taking place.
  // NULL otherwise.
  aura::RootWindow* selection_root_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelector);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_SELECTOR_H_
