// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/default_system_tray_delegate.h"

#include <string>

#include "ash/networking_config_delegate.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/volume_control_delegate.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"

namespace ash {

DefaultSystemTrayDelegate::DefaultSystemTrayDelegate()
    : bluetooth_enabled_(true) {
}

DefaultSystemTrayDelegate::~DefaultSystemTrayDelegate() {
}

bool DefaultSystemTrayDelegate::GetTrayVisibilityOnStartup() {
  return true;
}

user::LoginStatus DefaultSystemTrayDelegate::GetUserLoginStatus() const {
  return user::LOGGED_IN_USER;
}

std::string DefaultSystemTrayDelegate::GetSupervisedUserManager() const {
  if (!IsUserSupervised())
    return std::string();
  return "manager@chrome.com";
}

bool DefaultSystemTrayDelegate::IsUserSupervised() const {
  return GetUserLoginStatus() == ash::user::LOGGED_IN_SUPERVISED;
}

void DefaultSystemTrayDelegate::GetSystemUpdateInfo(UpdateInfo* info) const {
  DCHECK(info);
  info->severity = UpdateInfo::UPDATE_NORMAL;
  info->update_required = true;
  info->factory_reset_required = false;
}

bool DefaultSystemTrayDelegate::ShouldShowSettings() {
  return true;
}

bool DefaultSystemTrayDelegate::ShouldShowDisplayNotification() {
  return false;
}

void DefaultSystemTrayDelegate::ToggleBluetooth() {
  bluetooth_enabled_ = !bluetooth_enabled_;
}

bool DefaultSystemTrayDelegate::IsBluetoothDiscovering() {
  return false;
}

bool DefaultSystemTrayDelegate::GetBluetoothAvailable() {
  return true;
}

bool DefaultSystemTrayDelegate::GetBluetoothEnabled() {
  return bluetooth_enabled_;
}

bool DefaultSystemTrayDelegate::GetBluetoothDiscovering() {
  return false;
}

VolumeControlDelegate* DefaultSystemTrayDelegate::GetVolumeControlDelegate()
    const {
  return volume_control_delegate_.get();
}

void DefaultSystemTrayDelegate::SetVolumeControlDelegate(
    scoped_ptr<VolumeControlDelegate> delegate) {
  volume_control_delegate_ = delegate.Pass();
}

int DefaultSystemTrayDelegate::GetSystemTrayMenuWidth() {
  // This is the default width for English languages.
  return 300;
}

}  // namespace ash
