// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_SHELL_H_
#define ASH_COMMON_WM_SHELL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/common/media_delegate.h"
#include "ash/common/metrics/user_metrics_action.h"
#include "base/observer_list.h"

namespace views {
class PointerWatcher;
}

namespace ash {

class AccessibilityDelegate;
class BrightnessControlDelegate;
class DisplayInfo;
class FocusCycler;
class KeyboardBrightnessControlDelegate;
class KeyboardUI;
class MaximizeModeController;
class MruWindowTracker;
class ScopedDisableInternalMouseAndKeyboard;
class SessionStateDelegate;
class ShellDelegate;
class ShellObserver;
class SystemTrayDelegate;
class SystemTrayNotifier;
class WindowResizer;
class WindowSelectorController;
class WmActivationObserver;
class WmDisplayObserver;
class WmWindow;

namespace wm {
class MaximizeModeEventHandler;
class WindowState;
}

#if defined(OS_CHROMEOS)
class LogoutConfirmationController;
#endif

// Similar to ash::Shell. Eventually the two will be merged.
class ASH_EXPORT WmShell {
 public:
  // This is necessary for a handful of places that is difficult to plumb
  // through context.
  static void Set(WmShell* instance);
  static WmShell* Get();
  static bool HasInstance() { return instance_ != nullptr; }

  ShellDelegate* delegate() { return delegate_.get(); }

  BrightnessControlDelegate* brightness_control_delegate() {
    return brightness_control_delegate_.get();
  }

  FocusCycler* focus_cycler() { return focus_cycler_.get(); }

  KeyboardBrightnessControlDelegate* keyboard_brightness_control_delegate() {
    return keyboard_brightness_control_delegate_.get();
  }

  KeyboardUI* keyboard_ui() { return keyboard_ui_.get(); }

  MaximizeModeController* maximize_mode_controller() {
    return maximize_mode_controller_.get();
  }

  MruWindowTracker* mru_window_tracker() { return mru_window_tracker_.get(); }

  MediaDelegate* media_delegate() { return media_delegate_.get(); }

  SystemTrayNotifier* system_tray_notifier() {
    return system_tray_notifier_.get();
  }

  SystemTrayDelegate* system_tray_delegate() {
    return system_tray_delegate_.get();
  }

  WindowSelectorController* window_selector_controller() {
    return window_selector_controller_.get();
  }

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

  // Retuns the display info associated with |display_id|.
  // TODO(msw): Remove this when DisplayManager has been moved. crbug.com/622480
  virtual const DisplayInfo& GetDisplayInfo(int64_t display_id) const = 0;

  // Matches that of DisplayManager::IsActiveDisplayId().
  // TODO: Remove this when DisplayManager has been moved. crbug.com/622480
  virtual bool IsActiveDisplayId(int64_t display_id) const = 0;

  // Returns true if the first window shown on first run should be
  // unconditionally maximized, overriding the heuristic that normally chooses
  // the window size.
  virtual bool IsForceMaximizeOnFirstRun() = 0;

  // Returns true if a system-modal dialog window is currently open.
  bool IsSystemModalWindowOpen();

  // For testing only: set simulation that a modal window is open
  void SimulateModalWindowOpenForTesting(bool modal_window_open) {
    simulate_modal_window_open_for_testing_ = modal_window_open;
  }

  // Returns true if a window is currently pinned.
  virtual bool IsPinned() = 0;

  // Sets/Unsets the |window| to as a pinned window. If this is called with a
  // window with WINDOW_STATE_TYPE_PINNED state, then this sets the |window|
  // as a pinned window. Otherwise, this unsets it.
  // For setting, a caller needs to guarantee that no windows are set
  // as pinned window. For unsetting, a caller needs to guarantee that the
  // |window| is the one which is currently set as a pinned window via previous
  // this function invocation.
  virtual void SetPinnedWindow(WmWindow* window) = 0;

  // Returns true if |window| can be shown for the current user. This is
  // intended to check if the current user matches the user associated with
  // |window|.
  // TODO(jamescook): Remove this when ShellDelegate has been moved.
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

  virtual std::unique_ptr<wm::MaximizeModeEventHandler>
  CreateMaximizeModeEventHandler() = 0;

  virtual std::unique_ptr<ScopedDisableInternalMouseAndKeyboard>
  CreateScopedDisableInternalMouseAndKeyboard() = 0;

  // Called after maximize mode has started, windows might still animate though.
  void OnMaximizeModeStarted();

  // Called after maximize mode has ended, windows might still be returning to
  // their original position.
  void OnMaximizeModeEnded();

  // Called when the overview mode is about to be started (before the windows
  // get re-arranged).
  virtual void OnOverviewModeStarting() = 0;

  // Called after overview mode has ended.
  virtual void OnOverviewModeEnded() = 0;

  // Notifies |observers_| when entering or exiting pinned mode for
  // |pinned_window|. Entering or exiting can be checked by looking at
  // |pinned_window|'s window state.
  void NotifyPinnedStateChanged(WmWindow* pinned_window);

  // Called when virtual keyboard has been activated/deactivated.
  void OnVirtualKeyboardActivated(bool activated);

  virtual AccessibilityDelegate* GetAccessibilityDelegate() = 0;

  virtual SessionStateDelegate* GetSessionStateDelegate() = 0;

  virtual void AddActivationObserver(WmActivationObserver* observer) = 0;
  virtual void RemoveActivationObserver(WmActivationObserver* observer) = 0;

  virtual void AddDisplayObserver(WmDisplayObserver* observer) = 0;
  virtual void RemoveDisplayObserver(WmDisplayObserver* observer) = 0;

  void AddShellObserver(ShellObserver* observer);
  void RemoveShellObserver(ShellObserver* observer);

  virtual void AddPointerWatcher(views::PointerWatcher* watcher) = 0;
  virtual void RemovePointerWatcher(views::PointerWatcher* watcher) = 0;

#if defined(OS_CHROMEOS)
  LogoutConfirmationController* logout_confirmation_controller() {
    return logout_confirmation_controller_.get();
  }

  // TODO(jamescook): Remove this when VirtualKeyboardController has been moved.
  virtual void ToggleIgnoreExternalKeyboard() = 0;
#endif

 protected:
  explicit WmShell(ShellDelegate* delegate);
  virtual ~WmShell();

  base::ObserverList<ShellObserver>* shell_observers() {
    return &shell_observers_;
  }

  void SetKeyboardUI(std::unique_ptr<KeyboardUI> keyboard_ui);

  // Helpers to set (and initialize) or destroy various delegates.
  // TODO(msw|jamescook): Remove these once ShellDelegate, etc. are ported.
  void SetMediaDelegate(std::unique_ptr<MediaDelegate> delegate);
  void SetSystemTrayDelegate(std::unique_ptr<SystemTrayDelegate> delegate);
  void DeleteSystemTrayDelegate();

  void DeleteWindowSelectorController();

  void CreateMaximizeModeController();
  void DeleteMaximizeModeController();

  void CreateMruWindowTracker();
  void DeleteMruWindowTracker();

 private:
  friend class AcceleratorControllerTest;
  friend class Shell;

  static WmShell* instance_;

  base::ObserverList<ShellObserver> shell_observers_;
  std::unique_ptr<ShellDelegate> delegate_;

  std::unique_ptr<BrightnessControlDelegate> brightness_control_delegate_;
  std::unique_ptr<FocusCycler> focus_cycler_;
  std::unique_ptr<KeyboardBrightnessControlDelegate>
      keyboard_brightness_control_delegate_;
  std::unique_ptr<KeyboardUI> keyboard_ui_;
  std::unique_ptr<MaximizeModeController> maximize_mode_controller_;
  std::unique_ptr<MediaDelegate> media_delegate_;
  std::unique_ptr<MruWindowTracker> mru_window_tracker_;
  std::unique_ptr<SystemTrayNotifier> system_tray_notifier_;
  std::unique_ptr<SystemTrayDelegate> system_tray_delegate_;
  std::unique_ptr<WindowSelectorController> window_selector_controller_;

  bool simulate_modal_window_open_for_testing_ = false;

#if defined(OS_CHROMEOS)
  std::unique_ptr<LogoutConfirmationController> logout_confirmation_controller_;
#endif
};

}  // namespace ash

#endif  // ASH_COMMON_WM_SHELL_H_
