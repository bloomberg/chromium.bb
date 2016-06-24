// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network_connect_delegate_chromeos.h"

#include "ash/common/login_status.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "chrome/browser/chromeos/enrollment_dialog_view.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_dialog.h"

namespace {

bool IsUIAvailable() {
  return ash::Shell::GetInstance() &&
         !ash::Shell::GetInstance()->session_state_delegate()->IsScreenLocked();
}

gfx::NativeWindow GetNativeWindow() {
  bool session_started = ash::Shell::GetInstance()
                             ->session_state_delegate()
                             ->IsActiveUserSessionStarted();
  ash::LoginStatus login_status =
      ash::WmShell::Get()->system_tray_delegate()->GetUserLoginStatus();
  bool isUserAddingRunning = ash::Shell::GetInstance()
                                 ->session_state_delegate()
                                 ->IsInSecondaryLoginScreen();

  int container_id =
      (!session_started || login_status == ash::LoginStatus::NOT_LOGGED_IN ||
       login_status == ash::LoginStatus::LOCKED || isUserAddingRunning)
          ? ash::kShellWindowId_LockSystemModalContainer
          : ash::kShellWindowId_SystemModalContainer;
  return ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                  container_id);
}

}  // namespace

namespace chromeos {

NetworkConnectDelegateChromeOS::NetworkConnectDelegateChromeOS() {
}

NetworkConnectDelegateChromeOS::~NetworkConnectDelegateChromeOS() {
}

void NetworkConnectDelegateChromeOS::ShowNetworkConfigure(
    const std::string& network_id) {
  if (!IsUIAvailable())
    return;
  NetworkConfigView::Show(network_id, GetNativeWindow());
}

void NetworkConnectDelegateChromeOS::ShowNetworkSettingsForGuid(
    const std::string& network_id) {
  if (!IsUIAvailable())
    return;
  ash::WmShell::Get()->system_tray_delegate()->ShowNetworkSettingsForGuid(
      network_id);
}

bool NetworkConnectDelegateChromeOS::ShowEnrollNetwork(
    const std::string& network_id) {
  if (!IsUIAvailable())
    return false;
  return enrollment::CreateDialog(network_id, GetNativeWindow());
}

void NetworkConnectDelegateChromeOS::ShowMobileSimDialog() {
  if (!IsUIAvailable())
    return;
  SimDialogDelegate::ShowDialog(GetNativeWindow(),
                                SimDialogDelegate::SIM_DIALOG_UNLOCK);
}

void NetworkConnectDelegateChromeOS::ShowMobileSetupDialog(
    const std::string& service_path) {
  if (!IsUIAvailable())
    return;
  MobileSetupDialog::Show(service_path);
}

}  // namespace chromeos
