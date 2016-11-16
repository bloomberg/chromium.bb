// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/shutdown_policy_forwarder.h"

#include "ash/public/interfaces/shutdown.mojom.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {

ShutdownPolicyForwarder::ShutdownPolicyForwarder()
    : shutdown_policy_handler_(CrosSettings::Get(), this) {
  // Request the initial setting.
  shutdown_policy_handler_.NotifyDelegateWithShutdownPolicy();
}

ShutdownPolicyForwarder::~ShutdownPolicyForwarder() {}

void ShutdownPolicyForwarder::OnShutdownPolicyChanged(bool reboot_on_shutdown) {
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();

  // Shutdown policy changes rarely so don't bother caching the connection.
  ash::mojom::ShutdownControllerPtr shutdown_controller;

  // Under mash the ShutdownController interface is in the ash process. In
  // classic ash the browser provides it to itself.
  if (chrome::IsRunningInMash())
    connector->ConnectToInterface("ash", &shutdown_controller);
  else
    connector->ConnectToInterface("content_browser", &shutdown_controller);

  // Forward the setting to ash.
  shutdown_controller->SetRebootOnShutdown(reboot_on_shutdown);
}

}  // namespace chromeos
