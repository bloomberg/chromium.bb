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

namespace aura {
class WindowTreeClient;
}

namespace views {
class PointerWatcherEventRouter;
}

namespace ash {

class RootWindowController;

namespace mus {

class AcceleratorControllerDelegateMus;
class AcceleratorControllerRegistrar;
class ImmersiveHandlerFactoryMus;
class WindowManager;
class WmShellMusTestApi;

// WmShell implementation for mus.
class WmShellMus : public WmShell {
 public:
  // If |create_session_state_delegate_stub| is true SessionStateDelegateStub is
  // created. If false, the SessionStateDelegate from Shell is used.
  WmShellMus(WmWindow* primary_root_window,
             std::unique_ptr<ShellDelegate> shell_delegate,
             WindowManager* window_manager,
             views::PointerWatcherEventRouter* pointer_watcher_event_router,
             bool create_session_state_delegate_stub);
  ~WmShellMus() override;

  static WmShellMus* Get();

  ash::RootWindowController* GetRootWindowControllerWithDisplayId(int64_t id);

  AcceleratorControllerDelegateMus* accelerator_controller_delegate() {
    return accelerator_controller_delegate_.get();
  }

  aura::WindowTreeClient* window_tree_client();

  WindowManager* window_manager() { return window_manager_; }

  // WmShell:
  void Initialize(
      const scoped_refptr<base::SequencedWorkerPool>& pool) override;
  void Shutdown() override;
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
  void CreatePrimaryHost() override;
  void InitHosts(const ShellInitParams& init_params) override;

 private:
  friend class WmShellMusTestApi;

  WindowManager* window_manager_;

  WmWindow* primary_root_window_;
  views::PointerWatcherEventRouter* pointer_watcher_event_router_;

  std::unique_ptr<AcceleratorControllerDelegateMus>
      accelerator_controller_delegate_;
  std::unique_ptr<AcceleratorControllerRegistrar>
      accelerator_controller_registrar_;
  std::unique_ptr<ImmersiveHandlerFactoryMus> immersive_handler_factory_;
  std::unique_ptr<SessionStateDelegate> session_state_delegate_;

  DISALLOW_COPY_AND_ASSIGN(WmShellMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_SHELL_MUS_H_
