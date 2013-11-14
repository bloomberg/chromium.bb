// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SOLO_WINDOW_TRACKER_H_
#define ASH_WM_SOLO_WINDOW_TRACKER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/dock/docked_window_layout_manager_observer.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/rect.h"

namespace aura {
class RootWindow;
class Window;
}

namespace ash {

// Class which keeps track of the window (if any) which should use the solo
// window header. The solo window header is very transparent and is used when
// there is only one visible window and the window is not maximized or
// fullscreen. The solo window header is not used for either panels or docked
// windows.
class ASH_EXPORT SoloWindowTracker
    : public aura::WindowObserver,
      public internal::DockedWindowLayoutManagerObserver {
 public:
  explicit SoloWindowTracker(aura::RootWindow* root_window);
  virtual ~SoloWindowTracker();

  // Enable/Disable solo headers.
  static void SetSoloHeaderEnabled(bool enabled);

  // Returns the window, if any, which should use the solo window header.
  aura::Window* GetWindowWithSoloHeader();

 private:
  // Updates the window which would use the solo header if the window were not
  // maximized or fullscreen. If |ignore_window| is not NULL, it is ignored for
  // counting valid candidates. This is useful when there is a window which is
  // about to be moved to a different root window or about to be closed.
  void UpdateSoloWindow(aura::Window* ignore_window);

  // Returns true if there is a visible docked window.
  bool AnyVisibleWindowDocked() const;

  // aura::WindowObserver overrides:
  virtual void OnWindowAdded(aura::Window* new_window) OVERRIDE;
  virtual void OnWillRemoveWindow(aura::Window* window) OVERRIDE;
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE;

  // ash::internal::DockedWindowLayoutManagerObserver override:
  virtual void OnDockBoundsChanging(const gfx::Rect& new_bounds,
                                    Reason reason) OVERRIDE;

  // The containers whose children can use the solo header.
  std::vector<aura::Window*> containers_;

  // The dock's bounds.
  gfx::Rect dock_bounds_;

  // The window which would use the solo header if it were not maximized or
  // fullscreen.
  aura::Window* solo_window_;

  // Class which observes changes in |solo_window_|'s show type.
  class SoloWindowObserver;
  scoped_ptr<SoloWindowObserver> solo_window_observer_;

  DISALLOW_COPY_AND_ASSIGN(SoloWindowTracker);
};

}  // namespace ash

#endif // ASH_WM_SOLO_WINDOW_TRACKER_H_
