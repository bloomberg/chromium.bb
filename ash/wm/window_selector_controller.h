// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_SELECTOR_CONTROLLER_H_
#define ASH_WM_WINDOW_SELECTOR_CONTROLLER_H_

#include <list>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/window_selector.h"
#include "ash/wm/window_selector_delegate.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace ui {
class EventHandler;
}

namespace ash {

class WindowSelector;

// Manages a window selector which displays an overview of all windows and
// allows selecting a window to activate it.
class ASH_EXPORT WindowSelectorController
    : public WindowSelectorDelegate {
 public:
  WindowSelectorController();
  virtual ~WindowSelectorController();

  // Returns true if selecting windows in an overview is enabled. This is false
  // at certain times, such as when the lock screen is visible.
  static bool CanSelect();

  // Enters overview mode. This is essentially the window cycling mode however
  // not released on releasing the alt key and allows selecting with the mouse
  // or touch rather than keypresses.
  void ToggleOverview();

  // Cycles between windows in the given |direction|. It is assumed that the
  // alt key is held down and a key filter is installed to watch for alt being
  // released.
  void HandleCycleWindow(WindowSelector::Direction direction);

  // Informs the controller that the Alt key has been released and it can
  // terminate the existing multi-step cycle.
  void AltKeyReleased();

  // Returns true if window selection mode is active.
  bool IsSelecting();

  // WindowSelectorDelegate:
  virtual void OnWindowSelected(aura::Window* window) OVERRIDE;
  virtual void OnSelectionCanceled() OVERRIDE;

 private:
  scoped_ptr<WindowSelector> window_selector_;
  scoped_ptr<ui::EventHandler> event_handler_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorController);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_SELECTOR_CONTROLLER_H_
