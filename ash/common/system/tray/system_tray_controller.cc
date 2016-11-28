// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/system_tray_controller.h"

#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"

namespace ash {

SystemTrayController::SystemTrayController()
    : hour_clock_type_(base::GetHourClockType()) {}

SystemTrayController::~SystemTrayController() {}

void SystemTrayController::ShowSettings() {
  if (system_tray_client_)
    system_tray_client_->ShowSettings();
}

void SystemTrayController::ShowDateSettings() {
  if (system_tray_client_)
    system_tray_client_->ShowDateSettings();
}

void SystemTrayController::ShowSetTimeDialog() {
  if (system_tray_client_)
    system_tray_client_->ShowSetTimeDialog();
}

void SystemTrayController::ShowDisplaySettings() {
  if (system_tray_client_)
    system_tray_client_->ShowDisplaySettings();
}

void SystemTrayController::ShowPowerSettings() {
  if (system_tray_client_)
    system_tray_client_->ShowPowerSettings();
}

void SystemTrayController::ShowChromeSlow() {
  if (system_tray_client_)
    system_tray_client_->ShowChromeSlow();
}

void SystemTrayController::ShowIMESettings() {
  if (system_tray_client_)
    system_tray_client_->ShowIMESettings();
}

void SystemTrayController::ShowHelp() {
  if (system_tray_client_)
    system_tray_client_->ShowHelp();
}

void SystemTrayController::ShowAccessibilityHelp() {
  if (system_tray_client_)
    system_tray_client_->ShowAccessibilityHelp();
}

void SystemTrayController::ShowAccessibilitySettings() {
  if (system_tray_client_)
    system_tray_client_->ShowAccessibilitySettings();
}

void SystemTrayController::ShowPaletteHelp() {
  if (system_tray_client_)
    system_tray_client_->ShowPaletteHelp();
}

void SystemTrayController::ShowPaletteSettings() {
  if (system_tray_client_)
    system_tray_client_->ShowPaletteSettings();
}

void SystemTrayController::ShowPublicAccountInfo() {
  if (system_tray_client_)
    system_tray_client_->ShowPublicAccountInfo();
}

void SystemTrayController::ShowNetworkConfigure(const std::string& network_id) {
  if (system_tray_client_)
    system_tray_client_->ShowNetworkConfigure(network_id);
}

void SystemTrayController::ShowNetworkCreate(const std::string& type) {
  if (system_tray_client_)
    system_tray_client_->ShowNetworkCreate(type);
}

void SystemTrayController::ShowThirdPartyVpnCreate(
    const std::string& extension_id) {
  if (system_tray_client_)
    system_tray_client_->ShowThirdPartyVpnCreate(extension_id);
}

void SystemTrayController::ShowNetworkSettings(const std::string& network_id) {
  if (system_tray_client_)
    system_tray_client_->ShowNetworkSettings(network_id);
}

void SystemTrayController::ShowProxySettings() {
  if (system_tray_client_)
    system_tray_client_->ShowProxySettings();
}

void SystemTrayController::SignOut() {
  if (system_tray_client_)
    system_tray_client_->SignOut();
}

void SystemTrayController::RequestRestartForUpdate() {
  if (system_tray_client_)
    system_tray_client_->RequestRestartForUpdate();
}

void SystemTrayController::BindRequest(mojom::SystemTrayRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void SystemTrayController::SetClient(mojom::SystemTrayClientPtr client) {
  system_tray_client_ = std::move(client);
}

void SystemTrayController::SetUse24HourClock(bool use_24_hour) {
  hour_clock_type_ = use_24_hour ? base::k24HourClock : base::k12HourClock;
  WmShell::Get()->system_tray_notifier()->NotifyDateFormatChanged();
}

}  // namespace ash
