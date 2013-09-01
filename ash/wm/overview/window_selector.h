// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/timer/timer.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class RootWindow;
}

namespace ui {
class LocatedEvent;
}

namespace ash {

namespace internal {
class WindowSelectorTest;
}

class WindowOverview;
class WindowSelectorDelegate;
class WindowSelectorWindow;

// The WindowSelector allows selecting a window by alt-tabbing (CYCLE mode) or
// by clicking or tapping on it (OVERVIEW mode). A WindowOverview will be shown
// in OVERVIEW mode or if the user lingers on a window while alt tabbing.
class ASH_EXPORT WindowSelector
    : public aura::WindowObserver,
      public aura::client::ActivationChangeObserver {
 public:
  enum Direction {
    FORWARD,
    BACKWARD
  };
  enum Mode {
    CYCLE,
    OVERVIEW
  };

  typedef std::vector<aura::Window*> WindowList;

  WindowSelector(const WindowList& windows,
                 Mode mode,
                 WindowSelectorDelegate* delegate);
  virtual ~WindowSelector();

  // Step to the next window in |direction|.
  void Step(Direction direction);

  // Choose the currently selected window.
  void SelectWindow();

  // Choose |window| from the available windows to select.
  void SelectWindow(aura::Window* window);

  // Cancels window selection.
  void CancelSelection();

  Mode mode() { return mode_; }

  // aura::WindowObserver:
  virtual void OnWindowAdded(aura::Window* new_window) OVERRIDE;
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

  // Overridden from aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;
  virtual void OnAttemptToReactivateWindow(
      aura::Window* request_active,
      aura::Window* actual_active) OVERRIDE;

 private:
  friend class internal::WindowSelectorTest;

  // Begins positioning windows such that all windows are visible on the screen.
  void StartOverview();

  // Stores the currently focused window and removes focus from it.
  void RemoveFocusAndSetRestoreWindow();

  // Resets the stored window from RemoveFocusAndSetRestoreWindow to NULL. If
  // |focus|, restores focus to the stored window.
  void ResetFocusRestoreWindow(bool focus);

  // The collection of windows in the overview wrapped by a helper class which
  // restores their state and helps transform them to other root windows.
  ScopedVector<WindowSelectorWindow> windows_;

  // The window selection mode.
  Mode mode_;

  base::DelayTimer<WindowSelector> start_overview_timer_;
  scoped_ptr<WindowOverview> window_overview_;

  // Weak pointer to the selector delegate which will be called when a
  // selection is made.
  WindowSelectorDelegate* delegate_;

  // Index of the currently selected window if the mode is CYCLE.
  size_t selected_window_;

  // A weak pointer to the window which was focused on entering overview mode.
  // If overview mode is canceled the focus should be restored to this window.
  aura::Window* restore_focus_window_;

  // True when restoring focus to the window. This is used to prevent handling
  // the resulting activation.
  bool restoring_focus_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelector);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_H_
