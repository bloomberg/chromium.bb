// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shutdown_controller.h"

#include "ash/common/shell_delegate.h"
#include "ash/common/wm_shell.h"
#include "base/sys_info.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace ash {

ShutdownController::ShutdownController() {}

ShutdownController::~ShutdownController() {}

void ShutdownController::ShutDownOrReboot() {
  // For developers on Linux desktop just exit the app.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    WmShell::Get()->delegate()->Exit();
    return;
  }

  // On real Chrome OS hardware the power manager handles shutdown.
  using chromeos::DBusThreadManager;
  if (reboot_on_shutdown_)
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
  else
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestShutdown();
}

void ShutdownController::SetRebootOnShutdown(bool reboot_on_shutdown) {
  reboot_on_shutdown_ = reboot_on_shutdown;
}

void ShutdownController::BindRequest(mojom::ShutdownControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace ash
