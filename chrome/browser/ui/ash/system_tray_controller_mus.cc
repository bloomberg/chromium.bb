// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_controller_mus.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/system/system_clock.h"
#include "content/public/common/mojo_shell_connection.h"
#include "services/shell/public/cpp/connector.h"

SystemTrayControllerMus::SystemTrayControllerMus() {
  shell::Connector* connector =
      content::MojoShellConnection::GetForProcess()->GetConnector();
  connector->ConnectToInterface("mojo:ash", &system_tray_);

  g_browser_process->platform_part()->GetSystemClock()->AddObserver(this);
}

SystemTrayControllerMus::~SystemTrayControllerMus() {
  g_browser_process->platform_part()->GetSystemClock()->RemoveObserver(this);
}

void SystemTrayControllerMus::OnSystemClockChanged(
    chromeos::system::SystemClock* clock) {
  system_tray_->SetUse24HourClock(clock->ShouldUse24HourClock());
}
