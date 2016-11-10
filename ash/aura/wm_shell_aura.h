// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AURA_WM_SHELL_AURA_H_
#define ASH_AURA_WM_SHELL_AURA_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/aura/wm_lookup_aura.h"
#include "ash/common/wm_shell.h"
#include "ash/display/window_tree_host_manager.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

class PointerWatcherAdapter;

class ASH_EXPORT WmShellAura : public WmShell,
                               public aura::client::ActivationChangeObserver,
                               public WindowTreeHostManager::Observer {
 public:
  explicit WmShellAura(std::unique_ptr<ShellDelegate> shell_delegate);
  ~WmShellAura() override;

  static WmShellAura* Get();

  void CreatePointerWatcherAdapter();

  // WmShell:
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
  // aura::client::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;
  void OnAttemptToReactivateWindow(aura::Window* request_active,
                                   aura::Window* actual_active) override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanging() override;
  void OnDisplayConfigurationChanged() override;

  WmLookupAura wm_lookup_;
  std::unique_ptr<PointerWatcherAdapter> pointer_watcher_adapter_;

  bool added_activation_observer_ = false;
  base::ObserverList<WmActivationObserver> activation_observers_;

  bool added_display_observer_ = false;
  base::ObserverList<WmDisplayObserver> display_observers_;

  DISALLOW_COPY_AND_ASSIGN(WmShellAura);
};

}  // namespace ash

#endif  // ASH_AURA_WM_SHELL_AURA_H_
