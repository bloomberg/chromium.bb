// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_CONTROLLER_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_CONTROLLER_H_

#include <list>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace ash {
class WindowSelector;
class WindowSelectorTest;

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

  // Returns true if window selection mode is active.
  bool IsSelecting();

  // WindowSelectorDelegate:
  virtual void OnSelectionEnded() OVERRIDE;

 private:
  friend class WindowSelectorTest;

  // Dispatched when window selection begins.
  void OnSelectionStarted();

  scoped_ptr<WindowSelector> window_selector_;
  base::Time last_selection_time_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorController);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_CONTROLLER_H_
