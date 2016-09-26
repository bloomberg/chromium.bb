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

void SystemTrayDelegateMus::ConnectToSystemTrayClient() {
  if (system_tray_client_.is_bound())
    return;

  connector_->ConnectToInterface("exe:chrome", &system_tray_client_);

  // Tolerate chrome crashing and coming back up.
  system_tray_client_.set_connection_error_handler(base::Bind(
      &SystemTrayDelegateMus::OnClientConnectionError, base::Unretained(this)));
}

void SystemTrayDelegateMus::OnClientConnectionError() {
  system_tray_client_.reset();
}

base::HourClockType SystemTrayDelegateMus::GetHourClockType() const {
  return hour_clock_type_;
}

void SystemTrayDelegateMus::ShowDateSettings() {
  ConnectToSystemTrayClient();
  system_tray_client_->ShowDateSettings();
}

void SystemTrayDelegateMus::SetUse24HourClock(bool use_24_hour) {
  hour_clock_type_ = use_24_hour ? base::k24HourClock : base::k12HourClock;
  WmShell::Get()->system_tray_notifier()->NotifyDateFormatChanged();
}

}  // namespace ash
