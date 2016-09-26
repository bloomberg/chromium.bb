// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/system/system_clock.h"
#include "chrome/browser/ui/ash/system_tray_common.h"
#include "content/public/common/mojo_shell_connection.h"
#include "services/shell/public/cpp/connector.h"

namespace {

SystemTrayClient* g_instance = nullptr;

}  // namespace

SystemTrayClient::SystemTrayClient() {
  // If this observes clock setting changes before ash comes up the IPCs will
  // be queued on |system_tray_|.
  g_browser_process->platform_part()->GetSystemClock()->AddObserver(this);

  DCHECK(!g_instance);
  g_instance = this;
}

SystemTrayClient::~SystemTrayClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;

  g_browser_process->platform_part()->GetSystemClock()->RemoveObserver(this);
}

// static
SystemTrayClient* SystemTrayClient::Get() {
  return g_instance;
}

void SystemTrayClient::ConnectToSystemTray() {
  if (system_tray_.is_bound())
    return;

  content::MojoShellConnection::GetForProcess()
      ->GetConnector()
      ->ConnectToInterface("mojo:ash", &system_tray_);

  // Tolerate ash crashing and coming back up.
  system_tray_.set_connection_error_handler(base::Bind(
      &SystemTrayClient::OnClientConnectionError, base::Unretained(this)));
}

void SystemTrayClient::OnClientConnectionError() {
  system_tray_.reset();
}

void SystemTrayClient::ShowDateSettings() {
  SystemTrayCommon::ShowDateSettings();
}

void SystemTrayClient::OnSystemClockChanged(
    chromeos::system::SystemClock* clock) {
  ConnectToSystemTray();
  system_tray_->SetUse24HourClock(clock->ShouldUse24HourClock());
}
