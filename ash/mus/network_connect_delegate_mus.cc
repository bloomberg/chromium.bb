// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/network_connect_delegate_mus.h"

#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/wm_shell.h"
#include "base/logging.h"

namespace ash {
namespace mus {

NetworkConnectDelegateMus::NetworkConnectDelegateMus() {}

NetworkConnectDelegateMus::~NetworkConnectDelegateMus() {}

void NetworkConnectDelegateMus::ShowNetworkConfigure(
    const std::string& service_path) {
  WmShell::Get()->system_tray_controller()->ShowNetworkConfigure(service_path);
}

void NetworkConnectDelegateMus::ShowNetworkSettingsForGuid(
    const std::string& network_id) {
  WmShell::Get()->system_tray_controller()->ShowNetworkSettings(network_id);
}

bool NetworkConnectDelegateMus::ShowEnrollNetwork(
    const std::string& network_id) {
  // TODO(mash): http://crbug.com/644355
  NOTIMPLEMENTED();
  return false;
}

void NetworkConnectDelegateMus::ShowMobileSimDialog() {
  // TODO(mash): http://crbug.com/644355
  NOTIMPLEMENTED();
}

void NetworkConnectDelegateMus::ShowMobileSetupDialog(
    const std::string& service_path) {
  // TODO(mash): http://crbug.com/644355
  NOTIMPLEMENTED();
}

}  // namespace mus
}  // namespace ash
