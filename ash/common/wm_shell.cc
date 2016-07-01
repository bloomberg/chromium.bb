// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm_shell.h"

#include "ash/common/focus_cycler.h"
#include "ash/common/keyboard/keyboard_ui.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/system/chromeos/session/logout_confirmation_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm_window.h"
#include "base/bind.h"
#include "base/logging.h"

namespace ash {

// static
WmShell* WmShell::instance_ = nullptr;

// static
void WmShell::Set(WmShell* instance) {
  instance_ = instance;
}

// static
WmShell* WmShell::Get() {
  return instance_;
}

void WmShell::NotifyPinnedStateChanged(WmWindow* pinned_window) {
  FOR_EACH_OBSERVER(ShellObserver, shell_observers_,
                    OnPinnedStateChanged(pinned_window));
}

void WmShell::AddShellObserver(ShellObserver* observer) {
  shell_observers_.AddObserver(observer);
}

void WmShell::RemoveShellObserver(ShellObserver* observer) {
  shell_observers_.RemoveObserver(observer);
}

WmShell::WmShell()
    : focus_cycler_(new FocusCycler),
      system_tray_notifier_(new SystemTrayNotifier),
      window_selector_controller_(new WindowSelectorController()) {}

WmShell::~WmShell() {}

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

void WmShell::SetKeyboardUI(std::unique_ptr<KeyboardUI> keyboard_ui) {
  keyboard_ui_ = std::move(keyboard_ui);
}

void WmShell::SetMediaDelegate(std::unique_ptr<MediaDelegate> delegate) {
  media_delegate_ = std::move(delegate);
}

void WmShell::SetSystemTrayDelegate(
    std::unique_ptr<SystemTrayDelegate> delegate) {
  DCHECK(delegate);
  DCHECK(!system_tray_delegate_);
  // TODO(jamescook): Create via ShellDelegate once it moves to //ash/common.
  system_tray_delegate_ = std::move(delegate);
  system_tray_delegate_->Initialize();
#if defined(OS_CHROMEOS)
  logout_confirmation_controller_.reset(new LogoutConfirmationController(
      base::Bind(&SystemTrayDelegate::SignOut,
                 base::Unretained(system_tray_delegate_.get()))));
#endif
}

void WmShell::DeleteSystemTrayDelegate() {
  DCHECK(system_tray_delegate_);
#if defined(OS_CHROMEOS)
  logout_confirmation_controller_.reset();
#endif
  system_tray_delegate_.reset();
}

void WmShell::DeleteWindowSelectorController() {
  window_selector_controller_.reset();
}

void WmShell::CreateMruWindowTracker() {
  mru_window_tracker_.reset(new MruWindowTracker);
}

void WmShell::DeleteMruWindowTracker() {
  mru_window_tracker_.reset();
}

}  // namespace ash
