// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AURA_SHELL_PORT_CLASSIC_H_
#define ASH_AURA_SHELL_PORT_CLASSIC_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/shell_port.h"
#include "base/macros.h"

namespace ash {

class AcceleratorControllerDelegateAura;
class PointerWatcherAdapter;

// Implementation of ShellPort for classic ash/aura. See ash/README.md for more
// details.
class ASH_EXPORT ShellPortClassic : public ShellPort {
 public:
  ShellPortClassic();
  ~ShellPortClassic() override;

  static ShellPortClassic* Get();

  AcceleratorControllerDelegateAura* accelerator_controller_delegate() {
    return accelerator_controller_delegate_.get();
  }

  // ShellPort:
  void Shutdown() override;
  Config GetAshConfig() const override;
  std::unique_ptr<display::TouchTransformSetter> CreateTouchTransformDelegate()
      override;
  void LockCursor() override;
  void UnlockCursor() override;
  void ShowCursor() override;
  void HideCursor() override;
  void SetGlobalOverrideCursor(base::Optional<ui::CursorData> cursor) override;
  bool IsMouseEventsEnabled() override;
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
      aura::Window* workspace_window) override;
  std::unique_ptr<ImmersiveFullscreenController>
  CreateImmersiveFullscreenController() override;
  std::unique_ptr<KeyboardUI> CreateKeyboardUI() override;
  std::unique_ptr<KeyEventWatcher> CreateKeyEventWatcher() override;
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
  std::unique_ptr<PointerWatcherAdapter> pointer_watcher_adapter_;

  std::unique_ptr<AcceleratorControllerDelegateAura>
      accelerator_controller_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ShellPortClassic);
};

}  // namespace ash

#endif  // ASH_AURA_SHELL_PORT_CLASSIC_H_
