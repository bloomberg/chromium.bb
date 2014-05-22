// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_H_

#include <set>
#include <vector>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/display_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class RootWindow;
class Window;
namespace client {
class CursorClient;
}  // namespace client
}  // namespace aura

namespace gfx {
class Rect;
}

namespace ui {
class LocatedEvent;
}

namespace ash {
class WindowSelectorDelegate;
class WindowSelectorItem;
class WindowSelectorTest;

// The WindowSelector shows a grid of all of your windows, allowing to select
// one by clicking or tapping on it.
class ASH_EXPORT WindowSelector
    : public ui::EventHandler,
      public gfx::DisplayObserver,
      public aura::WindowObserver,
      public aura::client::ActivationChangeObserver {
 public:
  typedef std::vector<aura::Window*> WindowList;
  typedef ScopedVector<WindowSelectorItem> WindowSelectorItemList;

  WindowSelector(const WindowList& windows,
                 WindowSelectorDelegate* delegate);
  virtual ~WindowSelector();

  // Choose |window| from the available windows to select.
  void SelectWindow(aura::Window* window);

  // Cancels window selection.
  void CancelSelection();

  // ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

  // gfx::DisplayObserver:
  virtual void OnDisplayAdded(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayMetricsChanged(const gfx::Display& display,
                                       uint32_t metrics) OVERRIDE;

  // aura::WindowObserver:
  virtual void OnWindowAdded(aura::Window* new_window) OVERRIDE;
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;
  virtual void OnAttemptToReactivateWindow(
      aura::Window* request_active,
      aura::Window* actual_active) OVERRIDE;

 private:
  friend class WindowSelectorTest;

  // Begins positioning windows such that all windows are visible on the screen.
  void StartOverview();

  // Position all of the windows based on the current selection mode.
  void PositionWindows(bool animate);
  // Position all of the windows from |root_window| on |root_window|.
  void PositionWindowsFromRoot(aura::Window* root_window, bool animate);

  // Resets the stored window from RemoveFocusAndSetRestoreWindow to NULL. If
  // Hide and track all hidden windows not in overview.
  void HideAndTrackNonOverviewWindows();

  // |focus|, restores focus to the stored window.
  void ResetFocusRestoreWindow(bool focus);

  // Returns the target of |event| or NULL if the event is not targeted at
  // any of the windows in the selector.
  aura::Window* GetEventTarget(ui::LocatedEvent* event);

  // Returns the top-level window selected by targeting |window| or NULL if
  // no overview window was found for |window|.
  aura::Window* GetTargetedWindow(aura::Window* window);

  // The collection of items in the overview wrapped by a helper class which
  // restores their state and helps transform them to other root windows.
  ScopedVector<WindowSelectorItem> windows_;

  // Tracks observed windows.
  std::set<aura::Window*> observed_windows_;

  // Weak pointer to the selector delegate which will be called when a
  // selection is made.
  WindowSelectorDelegate* delegate_;

  // A weak pointer to the window which was focused on beginning window
  // selection. If window selection is canceled the focus should be restored to
  // this window.
  aura::Window* restore_focus_window_;

  // True when performing operations that may cause window activations. This is
  // used to prevent handling the resulting expected activation.
  bool ignore_activations_;

  // The cursor client used to lock the current cursor during overview.
  aura::client::CursorClient* cursor_client_;

  // The time when overview was started.
  base::Time overview_start_time_;

  // Tracks windows which were hidden because they were not part of the
  // overview.
  aura::WindowTracker hidden_windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelector);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_H_
