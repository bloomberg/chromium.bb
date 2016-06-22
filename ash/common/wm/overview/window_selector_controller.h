// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_OVERVIEW_WINDOW_SELECTOR_CONTROLLER_H_
#define ASH_COMMON_WM_OVERVIEW_WINDOW_SELECTOR_CONTROLLER_H_

#include <list>
#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/common/wm/overview/window_selector.h"
#include "ash/common/wm/overview/window_selector_delegate.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace ash {
class WindowSelector;
class WindowSelectorTest;

// Manages a window selector which displays an overview of all windows and
// allows selecting a window to activate it.
class ASH_EXPORT WindowSelectorController : public WindowSelectorDelegate {
 public:
  WindowSelectorController();
  ~WindowSelectorController() override;

  // Returns true if selecting windows in an overview is enabled. This is false
  // at certain times, such as when the lock screen is visible.
  static bool CanSelect();

  // Enters overview mode. This is essentially the window cycling mode however
  // not released on releasing the alt key and allows selecting with the mouse
  // or touch rather than keypresses.
  void ToggleOverview();

  // Returns true if window selection mode is active.
  bool IsSelecting();

  // Returns true if overview mode is restoring minimized windows so that they
  // are visible during overview mode.
  bool IsRestoringMinimizedWindows() const;

  // WindowSelectorDelegate:
  void OnSelectionEnded() override;

 private:
  friend class WindowSelectorTest;

  // Dispatched when window selection begins.
  void OnSelectionStarted();

  std::unique_ptr<WindowSelector> window_selector_;
  base::Time last_selection_time_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorController);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_OVERVIEW_WINDOW_SELECTOR_CONTROLLER_H_
