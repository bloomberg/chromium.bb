// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/system_tray_controller.h"

#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace ash {

SystemTrayController::SystemTrayController(
    service_manager::Connector* connector)
    : connector_(connector), hour_clock_type_(base::GetHourClockType()) {}

SystemTrayController::~SystemTrayController() {}

void SystemTrayController::ShowSettings() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowSettings();
}

void SystemTrayController::ShowDateSettings() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowDateSettings();
}

void SystemTrayController::ShowSetTimeDialog() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowSetTimeDialog();
}

void SystemTrayController::ShowDisplaySettings() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowDisplaySettings();
}

void SystemTrayController::ShowPowerSettings() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowPowerSettings();
}

void SystemTrayController::ShowChromeSlow() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowChromeSlow();
}

void SystemTrayController::ShowIMESettings() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowIMESettings();
}

void SystemTrayController::ShowHelp() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowHelp();
}

void SystemTrayController::ShowAccessibilityHelp() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowAccessibilityHelp();
}

void SystemTrayController::ShowAccessibilitySettings() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowAccessibilitySettings();
}

void SystemTrayController::ShowPaletteHelp() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowPaletteHelp();
}

void SystemTrayController::ShowPaletteSettings() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowPaletteSettings();
}

void SystemTrayController::ShowPublicAccountInfo() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowPublicAccountInfo();
}

void SystemTrayController::ShowNetworkConfigure(const std::string& network_id) {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowNetworkConfigure(network_id);
}

void SystemTrayController::ShowNetworkCreate(const std::string& type) {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowNetworkCreate(type);
}

void SystemTrayController::ShowThirdPartyVpnCreate(
    const std::string& extension_id) {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowThirdPartyVpnCreate(extension_id);
}

void SystemTrayController::ShowNetworkSettings(const std::string& network_id) {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowNetworkSettings(network_id);
}

void SystemTrayController::ShowProxySettings() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->ShowProxySettings();
}

void SystemTrayController::SignOut() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->SignOut();
}

void SystemTrayController::RequestRestartForUpdate() {
  if (ConnectToSystemTrayClient())
    system_tray_client_->RequestRestartForUpdate();
}

void SystemTrayController::BindRequest(mojom::SystemTrayRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

bool SystemTrayController::ConnectToSystemTrayClient() {
  // Unit tests may not have a connector.
  if (!connector_)
    return false;

#if defined(OS_CHROMEOS)
  // If already connected, nothing to do.
  if (system_tray_client_.is_bound())
    return true;

  // Connect (or reconnect) to the interface.
  connector_->ConnectToInterface(content::mojom::kBrowserServiceName,
                                 &system_tray_client_);

  // Handle chrome crashes by forcing a reconnect on the next request.
  system_tray_client_.set_connection_error_handler(base::Bind(
      &SystemTrayController::OnClientConnectionError, base::Unretained(this)));
  return true;
#else
  // The SystemTrayClient interface in the browser is only implemented for
  // Chrome OS, so don't try to connect on other platforms.
  return false;
#endif  // defined(OS_CHROMEOS)
}

void SystemTrayController::OnClientConnectionError() {
  system_tray_client_.reset();
}

void SystemTrayController::SetUse24HourClock(bool use_24_hour) {
  hour_clock_type_ = use_24_hour ? base::k24HourClock : base::k12HourClock;
  WmShell::Get()->system_tray_notifier()->NotifyDateFormatChanged();
}

}  // namespace ash
