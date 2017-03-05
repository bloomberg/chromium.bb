// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_MRU_WINDOW_TRACKER_H_
#define ASH_COMMON_WM_MRU_WINDOW_TRACKER_H_

#include <list>
#include <vector>

#include "ash/ash_export.h"
#include "ash/common/wm_activation_observer.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"

namespace ash {

class WmWindow;

// Maintains a most recently used list of windows. This is used for window
// cycling using Alt+Tab and overview mode.
class ASH_EXPORT MruWindowTracker : public WmActivationObserver,
                                    public aura::WindowObserver {
 public:
  using WindowList = std::vector<WmWindow*>;

  MruWindowTracker();
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
  void SetActiveWindow(WmWindow* active_window);

  // Overridden from WmActivationObserver:
  void OnWindowActivated(WmWindow* gained_active,
                         WmWindow* lost_active) override;

  // Overridden from aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  // List of windows that have been activated in containers that we cycle
  // through, sorted by most recently used.
  std::list<WmWindow*> mru_windows_;

  bool ignore_window_activations_;

  DISALLOW_COPY_AND_ASSIGN(MruWindowTracker);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_MRU_WINDOW_TRACKER_H_
