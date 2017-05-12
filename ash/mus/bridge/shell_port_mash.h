// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_SHELL_PORT_MASH_H_
#define ASH_MUS_BRIDGE_SHELL_PORT_MASH_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/display/window_tree_host_manager.h"
#include "ash/shell_port.h"
#include "base/macros.h"

namespace aura {
class WindowTreeClient;
}

namespace views {
class PointerWatcherEventRouter;
}

namespace ash {

class AcceleratorControllerDelegateAura;
class PointerWatcherAdapter;
class RootWindowController;

namespace mus {

class AcceleratorControllerDelegateMus;
class AcceleratorControllerRegistrar;
class ImmersiveHandlerFactoryMus;
class WindowManager;
class ShellPortMashTestApi;

// ShellPort implementation for mash/mus. See ash/README.md for more.
class ShellPortMash : public ShellPort, public WindowTreeHostManager::Observer {
 public:
  // If |create_session_state_delegate_stub| is true SessionStateDelegateStub is
  // created. If false, the SessionStateDelegate from Shell is used.
  ShellPortMash(WmWindow* primary_root_window,
                WindowManager* window_manager,
                views::PointerWatcherEventRouter* pointer_watcher_event_router,
                bool create_session_state_delegate_stub);
  ~ShellPortMash() override;

  static ShellPortMash* Get();

  ash::RootWindowController* GetRootWindowControllerWithDisplayId(int64_t id);

  AcceleratorControllerDelegateAura* accelerator_controller_delegate_mus() {
    return mus_state_->accelerator_controller_delegate.get();
  }

  aura::WindowTreeClient* window_tree_client();

  WindowManager* window_manager() { return window_manager_; }

  // ShellPort:
  void Shutdown() override;
  Config GetAshConfig() const override;
  WmWindow* GetPrimaryRootWindow() override;
  WmWindow* GetRootWindowForDisplayId(int64_t display_id) override;
  const display::ManagedDisplayInfo& GetDisplayInfo(
      int64_t display_id) const override;
  bool IsActiveDisplayId(int64_t display_id) const override;
  display::Display GetFirstDisplay() const override;
  bool IsInUnifiedMode() const override;
  bool IsInUnifiedModeIgnoreMirroring() const override;
  void SetDisplayWorkAreaInsets(WmWindow* window,
                                const gfx::Insets& insets) override;
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
  std::unique_ptr<KeyboardUI> CreateKeyboardUI() override;
  std::unique_ptr<KeyEventWatcher> CreateKeyEventWatcher() override;
  SessionStateDelegate* GetSessionStateDelegate() override;
  void AddDisplayObserver(WmDisplayObserver* observer) override;
  void RemoveDisplayObserver(WmDisplayObserver* observer) override;
  void AddPointerWatcher(views::PointerWatcher* watcher,
                         views::PointerWatcherEventTypes events) override;
  void RemovePointerWatcher(views::PointerWatcher* watcher) override;
  bool IsTouchDown() override;
  void ToggleIgnoreExternalKeyboard() override;
  void SetLaserPointerEnabled(bool enabled) override;
  void SetPartialMagnifierEnabled(bool enabled) override;
  void CreatePointerWatcherAdapter() override;
  std::unique_ptr<AshWindowTreeHost> CreateAshWindowTreeHost(
      const AshWindowTreeHostInitParams& init_params) override;
  void OnCreatedRootWindowContainers(
      RootWindowController* root_window_controller) override;
  void CreatePrimaryHost() override;
  void InitHosts(const ShellInitParams& init_params) override;
  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override;
  std::unique_ptr<AcceleratorController> CreateAcceleratorController() override;

 private:
  friend class ShellPortMashTestApi;

  struct MashSpecificState {
    MashSpecificState();
    ~MashSpecificState();

    views::PointerWatcherEventRouter* pointer_watcher_event_router = nullptr;
    std::unique_ptr<AcceleratorControllerDelegateMus>
        accelerator_controller_delegate;
    std::unique_ptr<AcceleratorControllerRegistrar>
        accelerator_controller_registrar;
    std::unique_ptr<ImmersiveHandlerFactoryMus> immersive_handler_factory;
  };

  struct MusSpecificState {
    MusSpecificState();
    ~MusSpecificState();

    std::unique_ptr<PointerWatcherAdapter> pointer_watcher_adapter;
    std::unique_ptr<AcceleratorControllerDelegateAura>
        accelerator_controller_delegate;
  };

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanging() override;
  void OnDisplayConfigurationChanged() override;

  WindowManager* window_manager_;

  // TODO(sky): remove this once mash supports simple display management.
  WmWindow* primary_root_window_;

  // Only one of |mash_state_| or |mus_state_| is created, depending upon
  // Config.
  std::unique_ptr<MashSpecificState> mash_state_;
  std::unique_ptr<MusSpecificState> mus_state_;

  std::unique_ptr<SessionStateDelegate> session_state_delegate_;

  bool added_display_observer_ = false;
  base::ObserverList<WmDisplayObserver> display_observers_;

  DISALLOW_COPY_AND_ASSIGN(ShellPortMash);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_SHELL_PORT_MASH_H_
