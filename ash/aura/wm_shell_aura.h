// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AURA_WM_SHELL_AURA_H_
#define ASH_AURA_WM_SHELL_AURA_H_

#include "ash/ash_export.h"
#include "ash/aura/wm_lookup_aura.h"
#include "ash/common/wm_shell.h"
#include "ash/display/window_tree_host_manager.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

class ASH_EXPORT WmShellAura : public WmShell,
                               public aura::client::ActivationChangeObserver,
                               public WindowTreeHostManager::Observer {
 public:
  explicit WmShellAura(ShellDelegate* delegate);
  ~WmShellAura() override;

  static WmShellAura* Get();

  // Called early in shutdown sequence.
  void PrepareForShutdown();

  // WmShell:
  WmWindow* NewContainerWindow() override;
  WmWindow* GetFocusedWindow() override;
  WmWindow* GetActiveWindow() override;
  WmWindow* GetPrimaryRootWindow() override;
  WmWindow* GetRootWindowForDisplayId(int64_t display_id) override;
  WmWindow* GetRootWindowForNewWindows() override;
  const DisplayInfo& GetDisplayInfo(int64_t display_id) const override;
  bool IsActiveDisplayId(int64_t display_id) const override;
  bool IsForceMaximizeOnFirstRun() override;
  bool IsPinned() override;
  void SetPinnedWindow(WmWindow* window) override;
  bool CanShowWindowForUser(WmWindow* window) override;
  void LockCursor() override;
  void UnlockCursor() override;
  std::vector<WmWindow*> GetAllRootWindows() override;
  void RecordUserMetricsAction(UserMetricsAction action) override;
  std::unique_ptr<WindowResizer> CreateDragWindowResizer(
      std::unique_ptr<WindowResizer> next_window_resizer,
      wm::WindowState* window_state) override;
  std::unique_ptr<wm::MaximizeModeEventHandler> CreateMaximizeModeEventHandler()
      override;
  std::unique_ptr<ScopedDisableInternalMouseAndKeyboard>
  CreateScopedDisableInternalMouseAndKeyboard() override;
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnded() override;
  AccessibilityDelegate* GetAccessibilityDelegate() override;
  SessionStateDelegate* GetSessionStateDelegate() override;
  void AddActivationObserver(WmActivationObserver* observer) override;
  void RemoveActivationObserver(WmActivationObserver* observer) override;
  void AddDisplayObserver(WmDisplayObserver* observer) override;
  void RemoveDisplayObserver(WmDisplayObserver* observer) override;
  void AddPointerWatcher(views::PointerWatcher* watcher) override;
  void RemovePointerWatcher(views::PointerWatcher* watcher) override;
#if defined(OS_CHROMEOS)
  void ToggleIgnoreExternalKeyboard() override;
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
  bool added_activation_observer_ = false;
  base::ObserverList<WmActivationObserver> activation_observers_;

  bool added_display_observer_ = false;
  base::ObserverList<WmDisplayObserver> display_observers_;

  DISALLOW_COPY_AND_ASSIGN(WmShellAura);
};

}  // namespace ash

#endif  // ASH_AURA_WM_SHELL_AURA_H_
