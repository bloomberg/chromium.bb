// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AURA_WM_SHELL_AURA_H_
#define ASH_AURA_WM_SHELL_AURA_H_

#include <set>

#include "ash/ash_export.h"
#include "ash/aura/wm_lookup_aura.h"
#include "ash/common/wm_shell.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

class ASH_EXPORT WmShellAura : public WmShell,
                               public aura::client::ActivationChangeObserver,
                               public WindowTreeHostManager::Observer,
                               public ShellObserver {
 public:
  WmShellAura();
  ~WmShellAura() override;

  static WmShellAura* Get();

  // WmShell:
  WmWindow* NewContainerWindow() override;
  WmWindow* GetFocusedWindow() override;
  WmWindow* GetActiveWindow() override;
  WmWindow* GetPrimaryRootWindow() override;
  WmWindow* GetRootWindowForDisplayId(int64_t display_id) override;
  WmWindow* GetRootWindowForNewWindows() override;
  std::vector<WmWindow*> GetMruWindowList() override;
  std::vector<WmWindow*> GetMruWindowListIgnoreModals() override;
  bool IsForceMaximizeOnFirstRun() override;
  bool IsUserSessionBlocked() override;
  bool IsScreenLocked() override;
  void LockCursor() override;
  void UnlockCursor() override;
  std::vector<WmWindow*> GetAllRootWindows() override;
  void RecordUserMetricsAction(wm::WmUserMetricsAction action) override;
  std::unique_ptr<WindowResizer> CreateDragWindowResizer(
      std::unique_ptr<WindowResizer> next_window_resizer,
      wm::WindowState* window_state) override;
  bool IsOverviewModeSelecting() override;
  bool IsOverviewModeRestoringMinimizedWindows() override;
  void AddActivationObserver(WmActivationObserver* observer) override;
  void RemoveActivationObserver(WmActivationObserver* observer) override;
  void AddDisplayObserver(WmDisplayObserver* observer) override;
  void RemoveDisplayObserver(WmDisplayObserver* observer) override;
  void AddOverviewModeObserver(WmOverviewModeObserver* observer) override;
  void RemoveOverviewModeObserver(WmOverviewModeObserver* observer) override;

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

  // ShellObserver:
  void OnOverviewModeEnded() override;

  WmLookupAura wm_lookup_;
  bool added_activation_observer_ = false;
  base::ObserverList<WmActivationObserver> activation_observers_;

  bool added_display_observer_ = false;
  base::ObserverList<WmDisplayObserver> display_observers_;

  base::ObserverList<WmOverviewModeObserver> overview_mode_observers_;

  DISALLOW_COPY_AND_ASSIGN(WmShellAura);
};

}  // namespace ash

#endif  // ASH_AURA_WM_SHELL_AURA_H_
