// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_CYCLE_CONTROLLER_H_
#define ASH_WM_WINDOW_CYCLE_CONTROLLER_H_

#include <list>
#include <vector>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class RootWindow;
class Window;
namespace client {
class ActivationClient;
}
}

namespace ui {
class EventHandler;
}

namespace ash {

class WindowCycleList;

// Controls cycling through windows with the keyboard, for example, via alt-tab.
// Windows are sorted primarily by most recently used, and then by screen order.
// We activate windows as you cycle through them, so the order on the screen
// may change during the gesture, but the most recently used list isn't updated
// until the cycling ends.  Thus we maintain the state of the windows
// at the beginning of the gesture so you can cycle through in a consistent
// order.
class ASH_EXPORT WindowCycleController
    : public aura::client::ActivationChangeObserver,
      public aura::WindowObserver {
 public:
  enum Direction {
    FORWARD,
    BACKWARD
  };
  explicit WindowCycleController(
      aura::client::ActivationClient* activation_client);
  virtual ~WindowCycleController();

  // Returns true if cycling through windows is enabled. This is false at
  // certain times, such as when the lock screen is visible.
  static bool CanCycle();

  // Cycles between windows in the given |direction|. If |is_alt_down| then
  // interprets this call as the start of a multi-step cycle sequence and
  // installs a key filter to watch for alt being released.
  void HandleCycleWindow(Direction direction, bool is_alt_down);

  // Cycles between windows without maintaining a multi-step cycle sequence
  // (see above).
  void HandleLinearCycleWindow();

  // Informs the controller that the Alt key has been released and it can
  // terminate the existing multi-step cycle.
  void AltKeyReleased();

  // Returns true if we are in the middle of a window cycling gesture.
  bool IsCycling() const { return windows_.get() != NULL; }

  // Returns the WindowCycleList. Really only useful for testing.
  const WindowCycleList* windows() const { return windows_.get(); }

  // Set up the observers to handle window changes for the containers we care
  // about.  Called when a new root window is added.
  void OnRootWindowAdded(aura::RootWindow* root_window);

  // Returns the set of windows to cycle through. This method creates
  // the vector based on the current set of windows across all valid root
  // windows. As a result it is not necessarily the same as the set of
  // windows being iterated over.
  // If |mru_windows| is not NULL, windows in this list are put at the head of
  // the window list.
  // If |top_most_at_end| the window list will return in ascending order instead
  // of the default descending.
  static std::vector<aura::Window*> BuildWindowList(
      const std::list<aura::Window*>* mru_windows,
      bool top_most_at_end);

 private:
  // Call to start cycling windows.  You must call StopCycling() when done.
  void StartCycling();

  // Cycles to the next or previous window based on |direction|.
  void Step(Direction direction);

  // Installs an event filter to watch for release of the alt key.
  void InstallEventFilter();

  // Stops the current window cycle and cleans up the event filter.
  void StopCycling();

  // Checks if the window represents a container whose children we track.
  static bool IsTrackedContainer(aura::Window* window);

  // Overridden from aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

  // Overridden from WindowObserver:
  virtual void OnWindowAdded(aura::Window* window) OVERRIDE;
  virtual void OnWillRemoveWindow(aura::Window* window) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  scoped_ptr<WindowCycleList> windows_;

  // Event handler to watch for release of alt key.
  scoped_ptr<ui::EventHandler> event_handler_;

  // List of windows that have been activated in containers that we cycle
  // through, sorted by most recently used.
  std::list<aura::Window*> mru_windows_;

  aura::client::ActivationClient* activation_client_;

  DISALLOW_COPY_AND_ASSIGN(WindowCycleController);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_CYCLE_CONTROLLER_H_
