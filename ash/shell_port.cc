// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_port.h"

#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/app_list_shelf_item_delegate.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "base/bind.h"
#include "base/logging.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"

namespace ash {

// static
ShellPort* ShellPort::instance_ = nullptr;

ShellPort::~ShellPort() {
  DCHECK_EQ(this, instance_);
  instance_ = nullptr;
}

// static
ShellPort* ShellPort::Get() {
  return instance_;
}

void ShellPort::Shutdown() {}

void ShellPort::ShowContextMenu(const gfx::Point& location_in_screen,
                                ui::MenuSourceType source_type) {
  // Bail with no active user session, in the lock screen, or in app/kiosk mode.
  if (Shell::Get()->session_controller()->NumberOfLoggedInUsers() < 1 ||
      Shell::Get()->session_controller()->IsScreenLocked() ||
      Shell::Get()->session_controller()->IsRunningInAppMode()) {
    return;
  }

  aura::Window* root = wm::GetRootWindowAt(location_in_screen);
  RootWindowController::ForWindow(root)->ShowContextMenu(location_in_screen,
                                                         source_type);
}

void ShellPort::OnLockStateEvent(LockStateObserver::EventType event) {
  for (auto& observer : lock_state_observers_)
    observer.OnLockStateEvent(event);
}

void ShellPort::AddLockStateObserver(LockStateObserver* observer) {
  lock_state_observers_.AddObserver(observer);
}

void ShellPort::RemoveLockStateObserver(LockStateObserver* observer) {
  lock_state_observers_.RemoveObserver(observer);
}

ShellPort::ShellPort() {
  DCHECK(!instance_);
  instance_ = this;
}

bool ShellPort::IsForceMaximizeOnFirstRun() {
  return Shell::Get()->shell_delegate()->IsForceMaximizeOnFirstRun();
}

bool ShellPort::IsSystemModalWindowOpen() {
  if (simulate_modal_window_open_for_testing_)
    return true;

  // Traverse all system modal containers, and find its direct child window
  // with "SystemModal" setting, and visible.
  constexpr int modal_window_ids[] = {kShellWindowId_SystemModalContainer,
                                      kShellWindowId_LockSystemModalContainer};
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    for (int modal_window_id : modal_window_ids) {
      aura::Window* system_modal = root->GetChildById(modal_window_id);
      if (!system_modal)
        continue;
      for (const aura::Window* child : system_modal->children()) {
        if (child->GetProperty(aura::client::kModalKey) ==
                ui::MODAL_TYPE_SYSTEM &&
            child->layer()->GetTargetVisibility()) {
          return true;
        }
      }
    }
  }
  return false;
}

void ShellPort::CreateModalBackground(aura::Window* window) {
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    RootWindowController::ForWindow(root_window)
        ->GetSystemModalLayoutManager(window)
        ->CreateModalBackground();
  }
}

void ShellPort::OnModalWindowRemoved(aura::Window* removed) {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window* root_window : root_windows) {
    if (RootWindowController::ForWindow(root_window)
            ->GetSystemModalLayoutManager(removed)
            ->ActivateNextModalWindow()) {
      return;
    }
  }
  for (aura::Window* root_window : root_windows) {
    RootWindowController::ForWindow(root_window)
        ->GetSystemModalLayoutManager(removed)
        ->DestroyModalBackground();
  }
}

}  // namespace ash
