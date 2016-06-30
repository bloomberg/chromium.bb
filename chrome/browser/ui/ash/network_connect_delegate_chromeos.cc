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
  return ash::WmShell::HasInstance() &&
         !ash::WmShell::Get()->GetSessionStateDelegate()->IsScreenLocked();
}

gfx::NativeWindow GetNativeWindow() {
  ash::WmShell* wm_shell = ash::WmShell::Get();
  const bool session_started =
      wm_shell->GetSessionStateDelegate()->IsActiveUserSessionStarted();
  const ash::LoginStatus login_status =
      wm_shell->system_tray_delegate()->GetUserLoginStatus();
  const bool is_in_secondary_login_screen =
      wm_shell->GetSessionStateDelegate()->IsInSecondaryLoginScreen();

  int container_id =
      (!session_started || login_status == ash::LoginStatus::NOT_LOGGED_IN ||
       login_status == ash::LoginStatus::LOCKED || is_in_secondary_login_screen)
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
