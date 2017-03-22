// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm_shell.h"

#include <utility>

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/session/session_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/app_list_shelf_item_delegate.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/shutdown_controller.h"
#include "ash/common/system/chromeos/network/vpn_list.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm/root_window_finder.h"
#include "ash/common/wm/system_modal_container_layout_manager.h"
#include "ash/common/wm/window_cycle_controller.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/display/display.h"

namespace ash {

// static
WmShell* WmShell::instance_ = nullptr;

WmShell::~WmShell() {
  DCHECK_EQ(this, instance_);
  instance_ = nullptr;
}

// static
WmShell* WmShell::Get() {
  return instance_;
}

void WmShell::Shutdown() {
}

void WmShell::ShowContextMenu(const gfx::Point& location_in_screen,
                              ui::MenuSourceType source_type) {
  // Bail if there is no active user session or if the screen is locked.
  if (Shell::Get()->session_controller()->NumberOfLoggedInUsers() < 1 ||
      Shell::Get()->session_controller()->IsScreenLocked()) {
    return;
  }

  WmWindow* root = wm::GetRootWindowAt(location_in_screen);
  root->GetRootWindowController()->ShowContextMenu(location_in_screen,
                                                   source_type);
}

void WmShell::OnLockStateEvent(LockStateObserver::EventType event) {
  for (auto& observer : lock_state_observers_)
    observer.OnLockStateEvent(event);
}

void WmShell::AddLockStateObserver(LockStateObserver* observer) {
  lock_state_observers_.AddObserver(observer);
}

void WmShell::RemoveLockStateObserver(LockStateObserver* observer) {
  lock_state_observers_.RemoveObserver(observer);
}

WmShell::WmShell()
    : shutdown_controller_(base::MakeUnique<ShutdownController>()),
      system_tray_notifier_(base::MakeUnique<SystemTrayNotifier>()),
      vpn_list_(base::MakeUnique<VpnList>()),
      window_cycle_controller_(base::MakeUnique<WindowCycleController>()),
      window_selector_controller_(
          base::MakeUnique<WindowSelectorController>()) {
  DCHECK(!instance_);
  instance_ = this;
}

RootWindowController* WmShell::GetPrimaryRootWindowController() {
  return GetPrimaryRootWindow()->GetRootWindowController();
}

bool WmShell::IsForceMaximizeOnFirstRun() {
  return Shell::Get()->shell_delegate()->IsForceMaximizeOnFirstRun();
}

bool WmShell::IsSystemModalWindowOpen() {
  if (simulate_modal_window_open_for_testing_)
    return true;

  // Traverse all system modal containers, and find its direct child window
  // with "SystemModal" setting, and visible.
  for (WmWindow* root : GetAllRootWindows()) {
    WmWindow* system_modal =
        root->GetChildByShellWindowId(kShellWindowId_SystemModalContainer);
    if (!system_modal)
      continue;
    for (const WmWindow* child : system_modal->GetChildren()) {
      if (child->IsSystemModal() && child->GetTargetVisibility()) {
        return true;
      }
    }
  }
  return false;
}

void WmShell::CreateModalBackground(WmWindow* window) {
  for (WmWindow* root_window : GetAllRootWindows()) {
    root_window->GetRootWindowController()
        ->GetSystemModalLayoutManager(window)
        ->CreateModalBackground();
  }
}

void WmShell::OnModalWindowRemoved(WmWindow* removed) {
  WmWindow::Windows root_windows = GetAllRootWindows();
  for (WmWindow* root_window : root_windows) {
    if (root_window->GetRootWindowController()
            ->GetSystemModalLayoutManager(removed)
            ->ActivateNextModalWindow()) {
      return;
    }
  }
  for (WmWindow* root_window : root_windows) {
    root_window->GetRootWindowController()
        ->GetSystemModalLayoutManager(removed)
        ->DestroyModalBackground();
  }
}

void WmShell::DeleteWindowCycleController() {
  window_cycle_controller_.reset();
}

void WmShell::DeleteWindowSelectorController() {
  window_selector_controller_.reset();
}

}  // namespace ash
