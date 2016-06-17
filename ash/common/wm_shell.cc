// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm_shell.h"

#include "ash/common/focus_cycler.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/wm_system_tray_notifier.h"
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

WmShell::WmShell()
    : focus_cycler_(new FocusCycler),
      system_tray_notifier_(new WmSystemTrayNotifier) {}

WmShell::~WmShell() {}

void WmShell::SetSystemTrayDelegate(
    std::unique_ptr<SystemTrayDelegate> delegate) {
  if (delegate) {
    DCHECK(!system_tray_delegate_);
    // TODO(jamescook): Create via ShellDelegate once it moves to //ash/common.
    system_tray_delegate_ = std::move(delegate);
    system_tray_delegate_->Initialize();
  } else {
    DCHECK(system_tray_delegate_);
    system_tray_delegate_->Shutdown();
    system_tray_delegate_.reset();
  }
}

}  // namespace ash
