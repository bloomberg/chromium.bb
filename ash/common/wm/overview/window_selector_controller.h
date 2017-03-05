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

  // Attempts to toggle overview mode and returns true if successful (showing
  // overview would be unsuccessful if there are no windows to show).
  bool ToggleOverview();

  // Returns true if window selection mode is active.
  bool IsSelecting() const;

  // Moves the current selection by |increment| items. Positive values of
  // |increment| move the selection forward, negative values move it backward.
  void IncrementSelection(int increment);

  // Accepts current selection if any. Returns true if a selection was made,
  // false otherwise.
  bool AcceptSelection();

  // Returns true if overview mode is restoring minimized windows so that they
  // are visible during overview mode.
  bool IsRestoringMinimizedWindows() const;

  // WindowSelectorDelegate:
  void OnSelectionEnded() override;
  void AddDelayedAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation) override;
  void RemoveAndDestroyAnimationObserver(
      DelayedAnimationObserver* animation) override;

 private:
  friend class WindowSelectorTest;

  // Dispatched when window selection begins.
  void OnSelectionStarted();

  // Collection of DelayedAnimationObserver objects that own widgets that may be
  // still animating after overview mode ends. If shell needs to shut down while
  // those animations are in progress, the animations are shut down and the
  // widgets destroyed.
  std::vector<std::unique_ptr<DelayedAnimationObserver>> delayed_animations_;
  std::unique_ptr<WindowSelector> window_selector_;
  base::Time last_selection_time_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorController);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_OVERVIEW_WINDOW_SELECTOR_CONTROLLER_H_
