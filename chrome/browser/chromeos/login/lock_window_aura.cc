// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock_window_aura.h"

#include "ui/aura/desktop.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_window_ids.h"

namespace chromeos {

LockWindow* LockWindow::Create() {
  return new LockWindowAura();
}

////////////////////////////////////////////////////////////////////////////////
// LockWindow implementation:
void LockWindowAura::Grab(DOMView* dom_view) {
  // We already have grab from the lock screen container, just call the ready
  // callback immediately.
  if (observer_)
    observer_->OnLockWindowReady();
}

views::Widget* LockWindowAura::GetWidget() {
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
  params.bounds = gfx::Rect(aura::Desktop::GetInstance()->GetHostSize());
  // TODO(flackr): Use a property to specify this container rather than
  // depending on shell implementation.
  params.parent = aura_shell::Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_LockScreenContainer);
  views::Widget::Init(params);
}

}  // namespace chromeos
