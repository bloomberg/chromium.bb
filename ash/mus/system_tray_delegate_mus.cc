// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/system_tray_delegate_mus.h"

#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "services/shell/public/cpp/connector.h"

namespace ash {
namespace {

SystemTrayDelegateMus* g_instance = nullptr;

}  // namespace

SystemTrayDelegateMus::SystemTrayDelegateMus(shell::Connector* connector)
    : connector_(connector), hour_clock_type_(base::GetHourClockType()) {
  // Don't make an initial connection to exe:chrome. Do it on demand.
  DCHECK(!g_instance);
  g_instance = this;
}

SystemTrayDelegateMus::~SystemTrayDelegateMus() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
SystemTrayDelegateMus* SystemTrayDelegateMus::Get() {
  return g_instance;
}

mojom::SystemTrayClient* SystemTrayDelegateMus::ConnectToSystemTrayClient() {
  if (!system_tray_client_.is_bound()) {
    // Connect (or reconnect) to the interface.
    connector_->ConnectToInterface("exe:chrome", &system_tray_client_);

    // Tolerate chrome crashing and coming back up.
    system_tray_client_.set_connection_error_handler(
        base::Bind(&SystemTrayDelegateMus::OnClientConnectionError,
                   base::Unretained(this)));
  }
  return system_tray_client_.get();
}

void SystemTrayDelegateMus::OnClientConnectionError() {
  system_tray_client_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// SystemTrayDelegate:

base::HourClockType SystemTrayDelegateMus::GetHourClockType() const {
  return hour_clock_type_;
}

void SystemTrayDelegateMus::ShowSettings() {
  ConnectToSystemTrayClient()->ShowSettings();
}

void SystemTrayDelegateMus::ShowDateSettings() {
  ConnectToSystemTrayClient()->ShowDateSettings();
}

void SystemTrayDelegateMus::ShowNetworkSettingsForGuid(
    const std::string& guid) {
  // http://crbug.com/647412
  NOTIMPLEMENTED();
}

void SystemTrayDelegateMus::ShowDisplaySettings() {
  ConnectToSystemTrayClient()->ShowDisplaySettings();
}

void SystemTrayDelegateMus::ShowPowerSettings() {
  // http://crbug.com/647412
  NOTIMPLEMENTED();
}

void SystemTrayDelegateMus::ShowChromeSlow() {
  ConnectToSystemTrayClient()->ShowChromeSlow();
}

void SystemTrayDelegateMus::ShowIMESettings() {
  ConnectToSystemTrayClient()->ShowIMESettings();
}

void SystemTrayDelegateMus::ShowHelp() {
  ConnectToSystemTrayClient()->ShowHelp();
}

void SystemTrayDelegateMus::ShowAccessibilityHelp() {
  ConnectToSystemTrayClient()->ShowAccessibilityHelp();
}

void SystemTrayDelegateMus::ShowAccessibilitySettings() {
  ConnectToSystemTrayClient()->ShowAccessibilitySettings();
}

void SystemTrayDelegateMus::ShowPaletteHelp() {
  ConnectToSystemTrayClient()->ShowPaletteHelp();
}

void SystemTrayDelegateMus::ShowPaletteSettings() {
  ConnectToSystemTrayClient()->ShowPaletteSettings();
}

void SystemTrayDelegateMus::ShowPublicAccountInfo() {
  ConnectToSystemTrayClient()->ShowPublicAccountInfo();
}

void SystemTrayDelegateMus::ShowEnterpriseInfo() {
  // http://crbug.com/647412
  NOTIMPLEMENTED();
}

void SystemTrayDelegateMus::ShowProxySettings() {
  ConnectToSystemTrayClient()->ShowProxySettings();
}

////////////////////////////////////////////////////////////////////////////////
// mojom::SystemTray:

void SystemTrayDelegateMus::SetUse24HourClock(bool use_24_hour) {
  hour_clock_type_ = use_24_hour ? base::k24HourClock : base::k12HourClock;
  WmShell::Get()->system_tray_notifier()->NotifyDateFormatChanged();
}

}  // namespace ash
