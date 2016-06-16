// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_SHELL_H_
#define ASH_COMMON_WM_SHELL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/common/metrics/user_metrics_action.h"

namespace gfx {
class Rect;
}

namespace ash {

class AccessibilityDelegate;
class MruWindowTracker;
class SessionStateDelegate;
class ShellObserver;
class SystemTrayDelegate;
class WindowResizer;
class WmActivationObserver;
class WmDisplayObserver;
class WmSystemTrayNotifier;
class WmWindow;

namespace wm {
class WindowState;
}

// Similar to ash::Shell. Eventually the two will be merged.
class ASH_EXPORT WmShell {
 public:
  // This is necessary for a handful of places that is difficult to plumb
  // through context.
  static void Set(WmShell* instance);
  static WmShell* Get();
  static bool HasInstance() { return instance_ != nullptr; }

  WmSystemTrayNotifier* system_tray_notifier() {
    return system_tray_notifier_.get();
  }

  SystemTrayDelegate* system_tray_delegate() {
    return system_tray_delegate_.get();
  }

  virtual MruWindowTracker* GetMruWindowTracker() = 0;

  // Creates a new window used as a container of other windows. No painting is
  // done to the created window.
  virtual WmWindow* NewContainerWindow() = 0;

  virtual WmWindow* GetFocusedWindow() = 0;
  virtual WmWindow* GetActiveWindow() = 0;

  virtual WmWindow* GetPrimaryRootWindow() = 0;

  // Returns the root window for the specified display.
  virtual WmWindow* GetRootWindowForDisplayId(int64_t display_id) = 0;

  // Returns the root window that newly created windows should be added to.
  // NOTE: this returns the root, newly created window should be added to the
  // appropriate container in the returned window.
  virtual WmWindow* GetRootWindowForNewWindows() = 0;

  // Returns true if the first window shown on first run should be
  // unconditionally maximized, overriding the heuristic that normally chooses
  // the window size.
  virtual bool IsForceMaximizeOnFirstRun() = 0;

  // Returns true if |window| can be shown for the current user. This is
  // intended to check if the current user matches the user associated with
  // |window|.
  // TODO(jamescook): Remove this when ShellDelegate is accessible via this
  // interface.
  virtual bool CanShowWindowForUser(WmWindow* window) = 0;

  // See aura::client::CursorClient for details on these.
  virtual void LockCursor() = 0;
  virtual void UnlockCursor() = 0;

  virtual std::vector<WmWindow*> GetAllRootWindows() = 0;

  virtual void RecordUserMetricsAction(UserMetricsAction action) = 0;

  // Returns a WindowResizer to handle dragging. |next_window_resizer| is
  // the next WindowResizer in the WindowResizer chain. This may return
  // |next_window_resizer|.
  virtual std::unique_ptr<WindowResizer> CreateDragWindowResizer(
      std::unique_ptr<WindowResizer> next_window_resizer,
      wm::WindowState* window_state) = 0;

  // Called when the overview mode is about to be started (before the windows
  // get re-arranged).
  virtual void OnOverviewModeStarting() = 0;

  // Called after overview mode has ended.
  virtual void OnOverviewModeEnded() = 0;

  // TODO(sky): if WindowSelectorController can't be moved over, move these
  // onto their own local class.
  virtual bool IsOverviewModeSelecting() = 0;
  virtual bool IsOverviewModeRestoringMinimizedWindows() = 0;

  virtual AccessibilityDelegate* GetAccessibilityDelegate() = 0;

  virtual SessionStateDelegate* GetSessionStateDelegate() = 0;

  virtual void AddActivationObserver(WmActivationObserver* observer) = 0;
  virtual void RemoveActivationObserver(WmActivationObserver* observer) = 0;

  virtual void AddDisplayObserver(WmDisplayObserver* observer) = 0;
  virtual void RemoveDisplayObserver(WmDisplayObserver* observer) = 0;

  virtual void AddShellObserver(ShellObserver* observer) = 0;
  virtual void RemoveShellObserver(ShellObserver* observer) = 0;

  // Notifies |observers_| when entering or exiting pinned mode for
  // |pinned_window|. Entering or exiting can be checked by looking at
  // |pinned_window|'s window state.
  virtual void NotifyPinnedStateChanged(WmWindow* pinned_window) = 0;

 protected:
  WmShell();
  virtual ~WmShell();

  // If |delegate| is not null, sets and initializes the delegate. If |delegate|
  // is null, shuts down and destroys the delegate.
  void SetSystemTrayDelegate(std::unique_ptr<SystemTrayDelegate> delegate);

 private:
  friend class Shell;

  static WmShell* instance_;

  std::unique_ptr<WmSystemTrayNotifier> system_tray_notifier_;
  std::unique_ptr<SystemTrayDelegate> system_tray_delegate_;
};

}  // namespace ash

#endif  // ASH_COMMON_WM_SHELL_H_
