// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shutdown_controller.h"

#include <utility>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shutdown_reason.h"
#include "ash/wm/lock_state_controller.h"
#include "base/metrics/user_metrics.h"
#include "base/sys_info.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace ash {

ShutdownController::ShutdownController() {}

ShutdownController::~ShutdownController() {}

void ShutdownController::ShutDownOrReboot(ShutdownReason reason) {
  // For developers on Linux desktop just exit the app.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    Shell::Get()->shell_delegate()->Exit();
    return;
  }

  if (reason == ShutdownReason::POWER_BUTTON)
    base::RecordAction(base::UserMetricsAction("Accel_ShutDown_PowerButton"));

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

void ShutdownController::RequestShutdownFromLoginScreen() {
  Shell::Get()->lock_state_controller()->RequestShutdown(
      ShutdownReason::LOGIN_SHUT_DOWN_BUTTON);
}

void ShutdownController::BindRequest(mojom::ShutdownControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace ash
