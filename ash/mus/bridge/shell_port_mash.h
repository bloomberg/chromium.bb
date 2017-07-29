// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_SHELL_PORT_MASH_H_
#define ASH_MUS_BRIDGE_SHELL_PORT_MASH_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/shell_port.h"
#include "base/macros.h"

namespace aura {
class WindowTreeClient;
}

namespace views {
class PointerWatcherEventRouter;
}

namespace ash {

class AcceleratorControllerDelegateClassic;
class DisplaySynchronizer;
class PointerWatcherAdapterClassic;
class RootWindowController;

namespace mus {

class AcceleratorControllerDelegateMus;
class AcceleratorControllerRegistrar;
class ImmersiveHandlerFactoryMus;
class WindowManager;
class ShellPortMashTestApi;

// ShellPort implementation for mash/mus. See ash/README.md for more.
class ShellPortMash : public ShellPort {
 public:
  ShellPortMash(WindowManager* window_manager,
                views::PointerWatcherEventRouter* pointer_watcher_event_router);
  ~ShellPortMash() override;

  static ShellPortMash* Get();

  ash::RootWindowController* GetRootWindowControllerWithDisplayId(int64_t id);

  AcceleratorControllerDelegateClassic* accelerator_controller_delegate_mus() {
    return mus_state_->accelerator_controller_delegate.get();
  }

  aura::WindowTreeClient* window_tree_client();

  WindowManager* window_manager() { return window_manager_; }

  // Called when the window server has changed the mouse enabled state.
  void OnCursorTouchVisibleChanged(bool enabled);

  // ShellPort:
  void Shutdown() override;
  Config GetAshConfig() const override;
  std::unique_ptr<display::TouchTransformSetter> CreateTouchTransformDelegate()
      override;
  void LockCursor() override;
  void UnlockCursor() override;
  void ShowCursor() override;
  void HideCursor() override;
  void SetCursorSize(ui::CursorSize cursor_size) override;
  void SetGlobalOverrideCursor(base::Optional<ui::CursorData> cursor) override;
  bool IsMouseEventsEnabled() override;
  void SetCursorTouchVisible(bool enabled) override;
  std::unique_ptr<WindowResizer> CreateDragWindowResizer(
      std::unique_ptr<WindowResizer> next_window_resizer,
      wm::WindowState* window_state) override;
  std::unique_ptr<WindowCycleEventFilter> CreateWindowCycleEventFilter()
      override;
  std::unique_ptr<wm::TabletModeEventHandler> CreateTabletModeEventHandler()
      override;
  std::unique_ptr<WorkspaceEventHandler> CreateWorkspaceEventHandler(
      aura::Window* workspace_window) override;
  std::unique_ptr<ImmersiveFullscreenController>
  CreateImmersiveFullscreenController() override;
  std::unique_ptr<KeyboardUI> CreateKeyboardUI() override;
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
  void OnHostsInitialized() override;
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

    std::unique_ptr<PointerWatcherAdapterClassic> pointer_watcher_adapter;
    std::unique_ptr<AcceleratorControllerDelegateClassic>
        accelerator_controller_delegate;
  };

  WindowManager* window_manager_;

  // Only one of |mash_state_| or |mus_state_| is created, depending upon
  // Config.
  std::unique_ptr<MashSpecificState> mash_state_;
  std::unique_ptr<MusSpecificState> mus_state_;

  std::unique_ptr<DisplaySynchronizer> display_synchronizer_;

  bool cursor_touch_visible_ = true;

  DISALLOW_COPY_AND_ASSIGN(ShellPortMash);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_SHELL_PORT_MASH_H_
