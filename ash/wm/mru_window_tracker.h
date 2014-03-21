// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MRU_WINDOW_TRACKER_H_
#define ASH_WM_MRU_WINDOW_TRACKER_H_

#include <list>
#include <vector>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class RootWindow;
class Window;
namespace client {
class ActivationClient;
}
}

namespace ash {

// Maintains a most recently used list of windows. This is used for window
// cycling using Alt+Tab and overview mode.
class ASH_EXPORT MruWindowTracker
    : public aura::client::ActivationChangeObserver,
      public aura::WindowObserver {
 public:
  typedef std::vector<aura::Window*> WindowList;

  explicit MruWindowTracker(
      aura::client::ActivationClient* activation_client);
  virtual ~MruWindowTracker();

  // Returns the set of windows which can be cycled through. This method creates
  // the vector based on the current set of windows across all valid root
  // windows. As a result it is not necessarily the same as the set of
  // windows being iterated over.
  // If |top_most_at_end| the window list will return in ascending (lowest
  // window in stacking order first) order instead of the default descending
  // (top most window first) order.
  static WindowList BuildWindowList(bool top_most_at_end);

  // Returns the set of windows which can be cycled through using the tracked
  // list of most recently used windows.
  WindowList BuildMruWindowList();

  // Starts or stops ignoring window activations. If no longer ignoring
  // activations the currently active window is moved to the front of the
  // MRU window list. Used by WindowCycleList to avoid adding all cycled
  // windows to the front of the MRU window list.
  void SetIgnoreActivations(bool ignore);

 private:
  // Updates the mru_windows_ list to insert/move |active_window| at/to the
  // front.
  void SetActiveWindow(aura::Window* active_window);

  // Overridden from aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

  // Overridden from WindowObserver:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

  // List of windows that have been activated in containers that we cycle
  // through, sorted by most recently used.
  std::list<aura::Window*> mru_windows_;

  aura::client::ActivationClient* activation_client_;

  bool ignore_window_activations_;

  DISALLOW_COPY_AND_ASSIGN(MruWindowTracker);
};

}  // namespace ash

#endif  // ASH_WM_MRU_WINDOW_TRACKER_H_
