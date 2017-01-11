// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_instance_throttle.h"

#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/shared/app_types.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"

namespace arc {

namespace {

void OnDBusReply(bool is_prioritize, bool success) {
  if (success)
    return;
  const char* message = is_prioritize ? "unprioritize" : "prioritize";
  LOG(WARNING) << "Failed to " << message << " the instance";
}

bool IsArcAppWindow(ash::WmWindow* active) {
  DCHECK(active);
  return active->GetAppType() == static_cast<int>(ash::AppType::ARC_APP);
}

void ThrottleInstanceIfNeeded(ash::WmWindow* active) {
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  if (!session_manager_client) {
    LOG(WARNING) << "SessionManagerClient is not available";
    return;
  }
  if (!active || !IsArcAppWindow(active)) {
    // TODO(yusukes): Call session_manager_client->UnprioritizeArcInstance()
    // once it's ready.
  } else {
    session_manager_client->PrioritizeArcInstance(
        base::Bind(OnDBusReply, true));
  }
}

}  // namespace

ArcInstanceThrottle::ArcInstanceThrottle() {
  ash::WmShell* shell = ash::WmShell::Get();
  DCHECK(shell);
  shell->AddActivationObserver(this);
  ThrottleInstanceIfNeeded(shell->GetActiveWindow());
}

ArcInstanceThrottle::~ArcInstanceThrottle() {
  ash::WmShell* shell = ash::WmShell::Get();
  if (shell)
    shell->RemoveActivationObserver(this);
}

void ArcInstanceThrottle::OnWindowActivated(ash::WmWindow* gained_active,
                                            ash::WmWindow* lost_active) {
  ThrottleInstanceIfNeeded(gained_active);
}

}  // namespace arc
