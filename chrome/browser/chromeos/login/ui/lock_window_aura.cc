// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/lock_window_aura.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"

#if defined(USE_ATHENA)
#include "athena/screen/public/screen_manager.h"
#include "athena/util/container_priorities.h"
#include "athena/util/fill_layout_manager.h"
#endif

namespace chromeos {

LockWindow* LockWindow::Create() {
  LockWindowAura* lock_window = new LockWindowAura();
  // Cancel existing touch events when screen is locked.
  ui::GestureRecognizer::Get()->TransferEventsTo(
      lock_window->GetNativeWindow(), NULL);
  return lock_window;
}

////////////////////////////////////////////////////////////////////////////////
// LockWindow implementation:
void LockWindowAura::Grab() {
  // We already have grab from the lock screen container, just call the ready
  // callback immediately.
  if (observer_)
    observer_->OnLockWindowReady();
}

views::Widget* LockWindowAura::GetWidget() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// views::WidgetDelegate implementation:
views::View* LockWindowAura::GetInitiallyFocusedView() {
  return initially_focused_view_;
}

const views::Widget* LockWindowAura::GetWidget() const {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// LockWindowAura private:
LockWindowAura::LockWindowAura() {
  Init();
}

LockWindowAura::~LockWindowAura() {
}

void LockWindowAura::Init() {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = this;
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
#if defined(USE_ATHENA)
  // Don't set TRANSLUCENT_WINDOW because we don't have wallpaper manager yet.
  // TODO(dpolukhin): fix this code when crbug.com/408734 fixed.
  athena::ScreenManager::ContainerParams container_params(
      "LoginScreen", athena::CP_LOGIN_SCREEN);
  container_params.can_activate_children = true;
  container_params.block_events = true;
  container_params.modal_container_priority =
      athena::CP_LOGIN_SCREEN_SYSTEM_MODAL;
  lock_screen_container_.reset(
      athena::ScreenManager::Get()->CreateContainer(container_params));
  params.parent = lock_screen_container_.get();
  lock_screen_container_->SetLayoutManager(
      new athena::FillLayoutManager(lock_screen_container_.get()));
#else
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  // TODO(oshima): move the lock screen harness to ash.
  params.parent =
      ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                               ash::kShellWindowId_LockScreenContainer);
#endif
  views::Widget::Init(params);
  SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
}

}  // namespace chromeos
