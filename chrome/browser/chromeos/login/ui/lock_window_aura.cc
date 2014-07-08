// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/lock_window_aura.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"

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
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  // TODO(oshima): move the lock screen harness to ash.
  params.parent =
      ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                               ash::kShellWindowId_LockScreenContainer);
  views::Widget::Init(params);
  wm::SetWindowVisibilityAnimationTransition(
      GetNativeView(), wm::ANIMATE_NONE);
}

}  // namespace chromeos
