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

namespace wm {

class AshFocusRules;

}  // namespace wm


// Maintains a most recently used list of windows. This is used for window
// cycling using Alt+Tab and overview mode.
class ASH_EXPORT MruWindowTracker
    : public aura::client::ActivationChangeObserver,
      public aura::WindowObserver {
 public:
  typedef std::vector<aura::Window*> WindowList;

  MruWindowTracker(
      aura::client::ActivationClient* activation_client,
      ash::wm::AshFocusRules* focus_rules);
  ~MruWindowTracker() override;

  // Returns the set of windows which can be cycled through using the tracked
  // list of most recently used windows.
  WindowList BuildMruWindowList() const;

  // This does the same thing as the above, but ignores the system modal dialog
  // state and hence the returned list could contain more windows if a system
  // modal dialog window is present.
  WindowList BuildWindowListIgnoreModal() const;

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
  void OnWindowActivated(
      aura::client::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // Overridden from WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  // Uses the focus rules to check whether the window can be activateable,
  // regardless of the state of the System modal dialog.
  bool IsWindowConsideredActivateable(aura::Window* window) const;

  // List of windows that have been activated in containers that we cycle
  // through, sorted by most recently used.
  std::list<aura::Window*> mru_windows_;

  aura::client::ActivationClient* activation_client_;  // Not owned.

  wm::AshFocusRules* focus_rules_;  // Not owned.

  bool ignore_window_activations_;

  DISALLOW_COPY_AND_ASSIGN(MruWindowTracker);
};

}  // namespace ash

#endif  // ASH_WM_MRU_WINDOW_TRACKER_H_
