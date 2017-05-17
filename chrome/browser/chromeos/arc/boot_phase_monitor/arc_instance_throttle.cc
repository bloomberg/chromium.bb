// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_instance_throttle.h"

#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_util.h"
#include "ui/wm/public/activation_client.h"

namespace arc {

namespace {

void OnDBusReply(login_manager::ContainerCpuRestrictionState state,
                 bool success) {
  if (success)
    return;
  const char* message =
      (state == login_manager::CONTAINER_CPU_RESTRICTION_BACKGROUND)
          ? "unprioritize"
          : "prioritize";
  LOG(WARNING) << "Failed to " << message << " the instance";
}

void ThrottleInstanceIfNeeded(aura::Window* active) {
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  if (!session_manager_client) {
    LOG(WARNING) << "SessionManagerClient is not available";
    return;
  }
  const login_manager::ContainerCpuRestrictionState state =
      !IsArcAppWindow(active)
          ? login_manager::CONTAINER_CPU_RESTRICTION_BACKGROUND
          : login_manager::CONTAINER_CPU_RESTRICTION_FOREGROUND;
  session_manager_client->SetArcCpuRestriction(state,
                                               base::Bind(OnDBusReply, state));
}

}  // namespace

ArcInstanceThrottle::ArcInstanceThrottle() {
  ash::Shell::Get()->activation_client()->AddObserver(this);
  ThrottleInstanceIfNeeded(ash::wm::GetActiveWindow());
}

ArcInstanceThrottle::~ArcInstanceThrottle() {
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->activation_client()->RemoveObserver(this);
}

void ArcInstanceThrottle::OnWindowActivated(ActivationReason reason,
                                            aura::Window* gained_active,
                                            aura::Window* lost_active) {
  ThrottleInstanceIfNeeded(gained_active);
}

}  // namespace arc
