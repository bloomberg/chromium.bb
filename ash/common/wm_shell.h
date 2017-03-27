// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_SHELL_H_
#define ASH_COMMON_WM_SHELL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/common/metrics/gesture_action_type.h"
#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/wm/lock_state_observer.h"
#include "base/observer_list.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer_type.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/window_types.h"

namespace display {
class Display;
class ManagedDisplayInfo;
}

namespace gfx {
class Insets;
class Point;
}

namespace views {
class PointerWatcher;
enum class PointerWatcherEventTypes;
}

namespace ash {
class AcceleratorController;
class ImmersiveFullscreenController;
class KeyEventWatcher;
class KeyboardUI;
class RootWindowController;
class ScopedDisableInternalMouseAndKeyboard;
class SessionStateDelegate;
struct ShellInitParams;
class WindowCycleEventFilter;
class WindowResizer;
class WmDisplayObserver;
class WmWindow;
class WorkspaceEventHandler;

enum class LoginStatus;
enum class TaskSwitchSource;

namespace wm {
class MaximizeModeEventHandler;
class WindowState;
}

// Similar to ash::Shell. Eventually the two will be merged.
class ASH_EXPORT WmShell {
 public:
  virtual ~WmShell();

  static WmShell* Get();
  static bool HasInstance() { return instance_ != nullptr; }

  virtual void Shutdown();

  // Returns true when ash is running as a service_manager::Service.
  virtual bool IsRunningInMash() const = 0;

  virtual WmWindow* GetFocusedWindow() = 0;
  virtual WmWindow* GetActiveWindow() = 0;

  virtual WmWindow* GetCaptureWindow() = 0;

  // Convenience for GetPrimaryRootWindow()->GetRootWindowController().
  RootWindowController* GetPrimaryRootWindowController();

  virtual WmWindow* GetPrimaryRootWindow() = 0;

  // Returns the root window for the specified display.
  virtual WmWindow* GetRootWindowForDisplayId(int64_t display_id) = 0;

  // Retuns the display info associated with |display_id|.
  // TODO(mash): Remove when DisplayManager has been moved. crbug.com/622480
  virtual const display::ManagedDisplayInfo& GetDisplayInfo(
      int64_t display_id) const = 0;

  // Matches that of DisplayManager::IsActiveDisplayId().
  // TODO(mash): Remove when DisplayManager has been moved. crbug.com/622480
  virtual bool IsActiveDisplayId(int64_t display_id) const = 0;

  // Returns true if the desktop is in unified mode and there are no mirroring
  // displays.
  // TODO(mash): Remove when DisplayManager has been moved. crbug.com/622480
  virtual bool IsInUnifiedMode() const = 0;

  // Returns true if the desktop is in unified mode.
  // TODO(mash): Remove when DisplayManager has been moved. crbug.com/622480
  virtual bool IsInUnifiedModeIgnoreMirroring() const = 0;

  // Returns the first display; this is the first display listed by hardware,
  // which corresponds to internal displays on devices with integrated displays.
  // TODO(mash): Remove when DisplayManager has been moved. crbug.com/622480
  virtual display::Display GetFirstDisplay() const = 0;

  // Returns true if the first window shown on first run should be
  // unconditionally maximized, overriding the heuristic that normally chooses
  // the window size.
  bool IsForceMaximizeOnFirstRun();

  // Sets work area insets of the display containing |window|, pings observers.
  virtual void SetDisplayWorkAreaInsets(WmWindow* window,
                                        const gfx::Insets& insets) = 0;

  // Returns true if a system-modal dialog window is currently open.
  bool IsSystemModalWindowOpen();

  // Creates a modal background (a partially-opaque fullscreen window) on all
  // displays for |window|.
  void CreateModalBackground(WmWindow* window);

  // Called when a modal window is removed. It will activate another modal
  // window if any, or remove modal screens on all displays.
  void OnModalWindowRemoved(WmWindow* removed);

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

  // See aura::client::CursorClient for details on these.
  virtual void LockCursor() = 0;
  virtual void UnlockCursor() = 0;
  virtual bool IsMouseEventsEnabled() = 0;

  virtual std::vector<WmWindow*> GetAllRootWindows() = 0;

  virtual void RecordGestureAction(GestureActionType action) = 0;
  virtual void RecordUserMetricsAction(UserMetricsAction action) = 0;
  virtual void RecordTaskSwitchMetric(TaskSwitchSource source) = 0;

  // Shows the context menu for the wallpaper or shelf at |location_in_screen|.
  void ShowContextMenu(const gfx::Point& location_in_screen,
                       ui::MenuSourceType source_type);

  // Returns a WindowResizer to handle dragging. |next_window_resizer| is
  // the next WindowResizer in the WindowResizer chain. This may return
  // |next_window_resizer|.
  virtual std::unique_ptr<WindowResizer> CreateDragWindowResizer(
      std::unique_ptr<WindowResizer> next_window_resizer,
      wm::WindowState* window_state) = 0;

  virtual std::unique_ptr<WindowCycleEventFilter>
  CreateWindowCycleEventFilter() = 0;

  virtual std::unique_ptr<wm::MaximizeModeEventHandler>
  CreateMaximizeModeEventHandler() = 0;

  virtual std::unique_ptr<WorkspaceEventHandler> CreateWorkspaceEventHandler(
      WmWindow* workspace_window) = 0;

  virtual std::unique_ptr<ScopedDisableInternalMouseAndKeyboard>
  CreateScopedDisableInternalMouseAndKeyboard() = 0;

  virtual std::unique_ptr<ImmersiveFullscreenController>
  CreateImmersiveFullscreenController() = 0;

  // Creates the KeyboardUI. This is called early on.
  virtual std::unique_ptr<KeyboardUI> CreateKeyboardUI() = 0;

  virtual std::unique_ptr<KeyEventWatcher> CreateKeyEventWatcher() = 0;

  virtual SessionStateDelegate* GetSessionStateDelegate() = 0;

  virtual void AddDisplayObserver(WmDisplayObserver* observer) = 0;
  virtual void RemoveDisplayObserver(WmDisplayObserver* observer) = 0;

  // If |events| is PointerWatcherEventTypes::MOVES,
  // PointerWatcher::OnPointerEventObserved() is called for pointer move events.
  // If |events| is PointerWatcherEventTypes::DRAGS,
  // PointerWatcher::OnPointerEventObserved() is called for pointer drag events.
  // Requesting pointer moves or drags may incur a performance hit and should be
  // avoided if possible.
  virtual void AddPointerWatcher(views::PointerWatcher* watcher,
                                 views::PointerWatcherEventTypes events) = 0;
  virtual void RemovePointerWatcher(views::PointerWatcher* watcher) = 0;

  // TODO: Move these back to LockStateController when that has been moved.
  void OnLockStateEvent(LockStateObserver::EventType event);
  void AddLockStateObserver(LockStateObserver* observer);
  void RemoveLockStateObserver(LockStateObserver* observer);

  // True if any touch points are down.
  virtual bool IsTouchDown() = 0;

  // TODO(jamescook): Remove this when VirtualKeyboardController has been moved.
  virtual void ToggleIgnoreExternalKeyboard() = 0;

  // Enable or disable the laser pointer.
  virtual void SetLaserPointerEnabled(bool enabled) = 0;

  // Enable or disable the partial magnifier.
  virtual void SetPartialMagnifierEnabled(bool enabled) = 0;

  virtual void CreatePointerWatcherAdapter() = 0;

 protected:
  WmShell();

  // Called during startup to create the primary WindowTreeHost and
  // the corresponding RootWindowController.
  virtual void CreatePrimaryHost() = 0;
  virtual void InitHosts(const ShellInitParams& init_params) = 0;

  // Called during startup to create the AcceleratorController.
  virtual std::unique_ptr<AcceleratorController>
  CreateAcceleratorController() = 0;

 private:
  friend class AcceleratorControllerTest;
  friend class Shell;

  static WmShell* instance_;

  base::ObserverList<LockStateObserver> lock_state_observers_;

  bool simulate_modal_window_open_for_testing_ = false;
};

}  // namespace ash

#endif  // ASH_COMMON_WM_SHELL_H_
