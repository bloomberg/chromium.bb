// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_WINDOW_MANAGER_H_
#define ASH_COMMON_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_WINDOW_MANAGER_H_

#include <stdint.h>

#include <map>
#include <unordered_set>

#include "ash/ash_export.h"
#include "ash/common/shell_observer.h"
#include "ash/common/wm/window_state.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"

namespace ash {
class MaximizeModeController;
class MaximizeModeWindowState;

namespace wm {
class MaximizeModeEventHandler;
}

// A window manager which - when created - will force all windows into maximized
// mode. Exception are panels and windows which cannot be maximized.
// Windows which cannot be maximized / resized are centered with a layer placed
// behind the window so that no other windows are visible and/or obscured.
// With the destruction of the manager all windows will be restored to their
// original state.
class ASH_EXPORT MaximizeModeWindowManager : public aura::WindowObserver,
                                             public display::DisplayObserver,
                                             public ShellObserver {
 public:
  // This should only be deleted by the creator (ash::Shell).
  ~MaximizeModeWindowManager() override;

  // Returns the number of maximized & tracked windows by this manager.
  int GetNumberOfManagedWindows();

  // Adds a window which needs to be maximized. This is used by other window
  // managers for windows which needs to get tracked due to (upcoming) state
  // changes.
  // The call gets ignored if the window was already or should not be handled.
  void AddWindow(WmWindow* window);

  // Called from a window state object when it gets destroyed.
  void WindowStateDestroyed(WmWindow* window);

  // ShellObserver overrides:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnded() override;

  // Overridden from WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  // display::DisplayObserver overrides:
  void OnDisplayAdded(const display::Display& display) override;
  void OnDisplayRemoved(const display::Display& display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

 protected:
  friend class MaximizeModeController;

  // The object should only be created by the ash::Shell.
  MaximizeModeWindowManager();

 private:
  using WindowToState = std::map<WmWindow*, MaximizeModeWindowState*>;

  // Maximize all windows and restore their current state.
  void MaximizeAllWindows();

  // Restore all windows to their previous state.
  void RestoreAllWindows();

  // Set whether to defer bounds updates on all tracked windows. When set to
  // false bounds will be updated as they may be stale.
  void SetDeferBoundsUpdates(bool defer_bounds_updates);

  // If the given window should be handled by us, this function will maximize it
  // and add it to the list of known windows (remembering the initial show
  // state).
  // Note: If the given window cannot be handled by us the function will return
  // immediately.
  void MaximizeAndTrackWindow(WmWindow* window);

  // Remove a window from our tracking list.
  void ForgetWindow(WmWindow* window);

  // Returns true when the given window should be modified in any way by us.
  bool ShouldHandleWindow(WmWindow* window);

  // Add window creation observers to track creation of new windows.
  void AddWindowCreationObservers();

  // Remove Window creation observers.
  void RemoveWindowCreationObservers();

  // Change the internal state (e.g. observers) when the display configuration
  // changes.
  void DisplayConfigurationChanged();

  // Returns true when the |window| is a container window.
  bool IsContainerWindow(aura::Window* window);

  // Add a backdrop behind the currently active window on each desktop.
  void EnableBackdropBehindTopWindowOnEachDisplay(bool enable);

  // Every window which got touched by our window manager gets added here.
  WindowToState window_state_map_;

  // All container windows which have to be tracked.
  std::unordered_set<aura::Window*> observed_container_windows_;

  // Windows added to the container, but not yet shown.
  std::unordered_set<aura::Window*> added_windows_;

  // True if all backdrops are hidden.
  bool backdrops_hidden_;

  std::unique_ptr<wm::MaximizeModeEventHandler> event_handler_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeModeWindowManager);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_WINDOW_MANAGER_H_
