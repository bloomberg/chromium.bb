// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_PORT_H_
#define ASH_SHELL_PORT_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/metrics/gesture_action_type.h"
#include "ash/metrics/user_metrics_action.h"
#include "ash/wm/lock_state_observer.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "ui/aura/client/window_types.h"
#include "ui/base/cursor/cursor_data.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class Window;
}

namespace display {
class NativeDisplayDelegate;
class TouchTransformSetter;
}

namespace gfx {
class Point;
}

namespace views {
class PointerWatcher;
enum class PointerWatcherEventTypes;
}

namespace ash {
class AcceleratorController;
class AshWindowTreeHost;
struct AshWindowTreeHostInitParams;
class ImmersiveFullscreenController;
class KeyEventWatcher;
class KeyboardUI;
class RootWindowController;
class WindowCycleEventFilter;
class WindowResizer;
class WorkspaceEventHandler;

enum class Config;
enum class TaskSwitchSource;

namespace wm {
class MaximizeModeEventHandler;
class WindowState;
}

// Porting layer for Shell. This class contains the part of Shell that are
// different in classic ash and mus/mash.
class ASH_EXPORT ShellPort {
 public:
  virtual ~ShellPort();

  static ShellPort* Get();
  static bool HasInstance() { return instance_ != nullptr; }

  virtual void Shutdown();

  virtual Config GetAshConfig() const = 0;

  // Returns true if the first window shown on first run should be
  // unconditionally maximized, overriding the heuristic that normally chooses
  // the window size.
  bool IsForceMaximizeOnFirstRun();

  // Returns true if a system-modal dialog window is currently open.
  bool IsSystemModalWindowOpen();

  // Creates a modal background (a partially-opaque fullscreen window) on all
  // displays for |window|.
  void CreateModalBackground(aura::Window* window);

  // Called when a modal window is removed. It will activate another modal
  // window if any, or remove modal screens on all displays.
  void OnModalWindowRemoved(aura::Window* removed);

  // The return value from this is supplied to AshTouchTransformController; see
  // it and TouchTransformSetter for details.
  virtual std::unique_ptr<display::TouchTransformSetter>
  CreateTouchTransformDelegate() = 0;

  // For testing only: set simulation that a modal window is open
  void SimulateModalWindowOpenForTesting(bool modal_window_open) {
    simulate_modal_window_open_for_testing_ = modal_window_open;
  }

  // See aura::client::CursorClient for details on these.
  virtual void LockCursor() = 0;
  virtual void UnlockCursor() = 0;
  virtual void ShowCursor() = 0;
  virtual void HideCursor() = 0;
  virtual void SetGlobalOverrideCursor(
      base::Optional<ui::CursorData> cursor) = 0;
  virtual bool IsMouseEventsEnabled() = 0;

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
      aura::Window* workspace_window) = 0;

  virtual std::unique_ptr<ImmersiveFullscreenController>
  CreateImmersiveFullscreenController() = 0;

  // Creates the KeyboardUI. This is called early on.
  virtual std::unique_ptr<KeyboardUI> CreateKeyboardUI() = 0;

  virtual std::unique_ptr<KeyEventWatcher> CreateKeyEventWatcher() = 0;

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

  // Creates an AshWindowTreeHost. A return value of null results in a platform
  // specific AshWindowTreeHost being created.
  virtual std::unique_ptr<AshWindowTreeHost> CreateAshWindowTreeHost(
      const AshWindowTreeHostInitParams& init_params) = 0;

  // Called after the containers of |root_window_controller| have been created.
  // Allows ShellPort to install any additional state on the containers.
  virtual void OnCreatedRootWindowContainers(
      RootWindowController* root_window_controller) = 0;

 protected:
  ShellPort();

  // Called after WindowTreeHostManager::InitHosts().
  virtual void OnHostsInitialized() = 0;

  virtual std::unique_ptr<display::NativeDisplayDelegate>
  CreateNativeDisplayDelegate() = 0;

  // Called during startup to create the AcceleratorController.
  virtual std::unique_ptr<AcceleratorController>
  CreateAcceleratorController() = 0;

 private:
  friend class AcceleratorControllerTest;
  friend class Shell;

  static ShellPort* instance_;

  base::ObserverList<LockStateObserver> lock_state_observers_;

  bool simulate_modal_window_open_for_testing_ = false;
};

}  // namespace ash

#endif  // ASH_SHELL_PORT_H_
