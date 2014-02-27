// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_WINDOW_MANAGER_H_
#define ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_WINDOW_MANAGER_H_

#include <map>
#include <set>

#include "ash/ash_export.h"
#include "ash/wm/window_state.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/display_observer.h"

namespace ash {
class Shell;

namespace internal{

// A window manager which - when created - will force all windows into maximized
// mode. Exception are panels and windows which cannot be maximized.
// Windows which cannot be maximized / resized are centered with a layer placed
// behind the window so that no other windows are visible and/or obscured.
// With the destruction of the manager all windows will be restored to their
// original state.
class ASH_EXPORT MaximizeModeWindowManager : public aura::WindowObserver,
                                             public gfx::DisplayObserver {
 public:
  // This should only be deleted by the creator (ash::Shell).
  virtual ~MaximizeModeWindowManager();

 // Returns the number of maximized & tracked windows by this manager.
  int GetNumberOfManagedWindows();

  // Overridden from WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;
  virtual void OnWindowAdded(aura::Window* window) OVERRIDE;
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  // aura::DisplayObserver overrides:
  virtual void OnDisplayBoundsChanged(
      const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayAdded(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& display) OVERRIDE;

 protected:
  friend class ash::Shell;

  // The object should only be created by the ash::Shell.
  MaximizeModeWindowManager();

 private:
  typedef std::map<aura::Window*, wm::WindowStateType> WindowToStateType;

  // Maximize all windows and restore their current state.
  void MaximizeAllWindows();

  // Restore all windows to their previous state.
  void RestoreAllWindows();

  // If the given window should be handled by us, this function will maximize it
  // and add it to the list of known windows (remembering the initial show
  // state).
  // Note: If the given window cannot be handled by us the function will return
  // immediately.
  void MaximizeAndTrackWindow(aura::Window* window);

  // Restore the window to its original state and remove it from our tracking.
  void RestoreAndForgetWindow(aura::Window* window);

  // Remove a window from our tracking list.
  wm::WindowStateType ForgetWindow(aura::Window* window);

  // Returns true when the given window should be modified in any way by us.
  bool ShouldHandleWindow(aura::Window* window);

  // Returns true when the given window can be maximized.
  bool CanMaximize(aura::Window* window);

  // Maximize the window on the screen's workspace.
  void CenterWindow(aura::Window* window);

  // Add window creation observers to track creation of new windows.
  void AddWindowCreationObservers();

  // Remove Window creation observers.
  void RemoveWindowCreationObservers();

  // Change the internal state (e.g. observers) when the display configuration
  // changes.
  void DisplayConfigurationChanged();

  // Returns true when the |window| is a container window.
  bool IsContainerWindow(aura::Window* window);

  // Every window which got touched by our window manager gets added here.
  WindowToStateType initial_state_type_;

  // All container windows which have to be tracked.
  std::set<aura::Window*> observed_container_windows_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeModeWindowManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_WINDOW_MANAGER_H_
