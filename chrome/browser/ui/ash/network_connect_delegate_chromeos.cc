// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/network_connect_delegate_chromeos.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/user/login_status.h"
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
  ash::user::LoginStatus login_status =
      ash::Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus();
  bool isUserAddingRunning = ash::Shell::GetInstance()
                                 ->session_state_delegate()
                                 ->IsInSecondaryLoginScreen();

  int container_id =
      (!session_started || login_status == ash::user::LOGGED_IN_NONE ||
       login_status == ash::user::LOGGED_IN_LOCKED || isUserAddingRunning)
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

void NetworkConnectDelegateChromeOS::ShowNetworkSettings(
    const std::string& service_path) {
  if (!IsUIAvailable())
    return;
  ash::Shell::GetInstance()->system_tray_delegate()->ShowNetworkSettings(
      service_path);
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
