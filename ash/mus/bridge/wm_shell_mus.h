// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WM_SHELL_MUS_H_
#define ASH_MUS_BRIDGE_WM_SHELL_MUS_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/common/wm_shell.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "services/ui/public/cpp/window_tree_client_observer.h"

namespace ui {
class WindowTreeClient;
}

namespace views {
class PointerWatcherEventRouter;
}

namespace ash {
namespace mus {

class AcceleratorControllerDelegateMus;
class AcceleratorControllerRegistrar;
class ImmersiveHandlerFactoryMus;
class WindowManager;
class WmRootWindowControllerMus;
class WmShellMusTestApi;
class WmWindowMus;

// WmShell implementation for mus.
class WmShellMus : public WmShell, public ui::WindowTreeClientObserver {
 public:
  WmShellMus(std::unique_ptr<ShellDelegate> shell_delegate,
             WindowManager* window_manager,
             views::PointerWatcherEventRouter* pointer_watcher_event_router);
  ~WmShellMus() override;

  static WmShellMus* Get();

  void AddRootWindowController(WmRootWindowControllerMus* controller);
  void RemoveRootWindowController(WmRootWindowControllerMus* controller);

  // Returns the ancestor of |window| (including |window|) that is considered
  // toplevel. |window| may be null.
  static WmWindowMus* GetToplevelAncestor(ui::Window* window);

  WmRootWindowControllerMus* GetRootWindowControllerWithDisplayId(int64_t id);

  AcceleratorControllerDelegateMus* accelerator_controller_delegate() {
    return accelerator_controller_delegate_.get();
  }

  // WmShell:
  bool IsRunningInMash() const override;
  WmWindow* NewWindow(ui::wm::WindowType window_type,
                      ui::LayerType layer_type) override;
  WmWindow* GetFocusedWindow() override;
  WmWindow* GetActiveWindow() override;
  WmWindow* GetCaptureWindow() override;
  WmWindow* GetPrimaryRootWindow() override;
  WmWindow* GetRootWindowForDisplayId(int64_t display_id) override;
  const display::ManagedDisplayInfo& GetDisplayInfo(
      int64_t display_id) const override;
  bool IsActiveDisplayId(int64_t display_id) const override;
  display::Display GetFirstDisplay() const override;
  bool IsInUnifiedMode() const override;
  bool IsInUnifiedModeIgnoreMirroring() const override;
  bool IsForceMaximizeOnFirstRun() override;
  void SetDisplayWorkAreaInsets(WmWindow* window,
                                const gfx::Insets& insets) override;
  bool IsPinned() override;
  void SetPinnedWindow(WmWindow* window) override;
  void LockCursor() override;
  void UnlockCursor() override;
  bool IsMouseEventsEnabled() override;
  std::vector<WmWindow*> GetAllRootWindows() override;
  void RecordGestureAction(GestureActionType action) override;
  void RecordUserMetricsAction(UserMetricsAction action) override;
  void RecordTaskSwitchMetric(TaskSwitchSource source) override;
  std::unique_ptr<WindowResizer> CreateDragWindowResizer(
      std::unique_ptr<WindowResizer> next_window_resizer,
      wm::WindowState* window_state) override;
  std::unique_ptr<WindowCycleEventFilter> CreateWindowCycleEventFilter()
      override;
  std::unique_ptr<wm::MaximizeModeEventHandler> CreateMaximizeModeEventHandler()
      override;
  std::unique_ptr<WorkspaceEventHandler> CreateWorkspaceEventHandler(
      WmWindow* workspace_window) override;
  std::unique_ptr<ScopedDisableInternalMouseAndKeyboard>
  CreateScopedDisableInternalMouseAndKeyboard() override;
  std::unique_ptr<ImmersiveFullscreenController>
  CreateImmersiveFullscreenController() override;
  std::unique_ptr<KeyEventWatcher> CreateKeyEventWatcher() override;
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnded() override;
  SessionStateDelegate* GetSessionStateDelegate() override;
  void AddActivationObserver(WmActivationObserver* observer) override;
  void RemoveActivationObserver(WmActivationObserver* observer) override;
  void AddDisplayObserver(WmDisplayObserver* observer) override;
  void RemoveDisplayObserver(WmDisplayObserver* observer) override;
  void AddPointerWatcher(views::PointerWatcher* watcher,
                         views::PointerWatcherEventTypes events) override;
  void RemovePointerWatcher(views::PointerWatcher* watcher) override;
  void RequestShutdown() override;
  bool IsTouchDown() override;
#if defined(OS_CHROMEOS)
  void ToggleIgnoreExternalKeyboard() override;
  void SetLaserPointerEnabled(bool enabled) override;
#endif

 private:
  friend class WmShellMusTestApi;

  ui::WindowTreeClient* window_tree_client();

  // Returns true if |window| is a window that can have active children.
  static bool IsActivationParent(ui::Window* window);

  // ui::WindowTreeClientObserver:
  void OnWindowTreeFocusChanged(ui::Window* gained_focus,
                                ui::Window* lost_focus) override;
  void OnDidDestroyClient(ui::WindowTreeClient* client) override;

  WindowManager* window_manager_;

  views::PointerWatcherEventRouter* pointer_watcher_event_router_;

  std::vector<WmRootWindowControllerMus*> root_window_controllers_;

  std::unique_ptr<AcceleratorControllerDelegateMus>
      accelerator_controller_delegate_;
  std::unique_ptr<AcceleratorControllerRegistrar>
      accelerator_controller_registrar_;
  std::unique_ptr<ImmersiveHandlerFactoryMus> immersive_handler_factory_;
  std::unique_ptr<SessionStateDelegate> session_state_delegate_;

  base::ObserverList<WmActivationObserver> activation_observers_;

  DISALLOW_COPY_AND_ASSIGN(WmShellMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_SHELL_MUS_H_
