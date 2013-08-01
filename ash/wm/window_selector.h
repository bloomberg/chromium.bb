// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_SELECTOR_H_
#define ASH_WM_WINDOW_SELECTOR_H_

#include <vector>

#include "base/compiler_specific.h"
#include "ui/aura/window_observer.h"
#include "ui/base/events/event_handler.h"
#include "ui/gfx/transform.h"

namespace aura {
class RootWindow;
}

namespace ash {

class WindowSelectorDelegate;

// The WindowSelector shows a grid of all of your windows and allows selecting
// a window by clicking or tapping on it.
class WindowSelector : public ui::EventHandler,
                       public aura::WindowObserver {
 public:
  typedef std::vector<aura::Window*> WindowList;

  WindowSelector(const WindowList& windows,
                 WindowSelectorDelegate* delegate);
  virtual ~WindowSelector();

  // ui::EventHandler:
  virtual void OnEvent(ui::Event* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // aura::WindowObserver:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

 private:
  struct WindowDetails {
    WindowDetails() : window(NULL), minimized(false) {}

    bool operator==(const aura::Window* other_window) const {
      return window == other_window;
    }

    // A weak pointer to the window.
    aura::Window* window;

    // If true, the window was minimized and this should be restored if the
    // window was not selected.
    bool minimized;

    // The original transform of the window before entering overview mode.
    gfx::Transform original_transform;
  };

  void HandleSelectionEvent(ui::Event* event);
  void PositionWindows();
  void PositionWindowsOnRoot(aura::RootWindow* root_window);

  void SelectWindow(aura::Window*);

  // List of weak pointers of windows to select from.
  std::vector<WindowDetails> windows_;

  // Weak pointer to the selector delegate which will be called when a
  // selection is made.
  WindowSelectorDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelector);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_SELECTOR_H_
