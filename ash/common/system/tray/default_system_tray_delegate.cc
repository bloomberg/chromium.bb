// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/default_system_tray_delegate.h"

#include <string>
#include <utility>

#include "ash/common/system/networking_config_delegate.h"
#include "ash/common/system/volume_control_delegate.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"

namespace ash {

DefaultSystemTrayDelegate::DefaultSystemTrayDelegate()
    : bluetooth_enabled_(true) {}

DefaultSystemTrayDelegate::~DefaultSystemTrayDelegate() {}

bool DefaultSystemTrayDelegate::GetTrayVisibilityOnStartup() {
  return true;
}

LoginStatus DefaultSystemTrayDelegate::GetUserLoginStatus() const {
  return LoginStatus::USER;
}

std::string DefaultSystemTrayDelegate::GetSupervisedUserManager() const {
  if (!IsUserSupervised())
    return std::string();
  return "manager@chrome.com";
}

bool DefaultSystemTrayDelegate::IsUserSupervised() const {
  return GetUserLoginStatus() == LoginStatus::SUPERVISED;
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
    std::unique_ptr<VolumeControlDelegate> delegate) {
  volume_control_delegate_ = std::move(delegate);
}

int DefaultSystemTrayDelegate::GetSystemTrayMenuWidth() {
  // This is the default width for English languages.
  return 300;
}

}  // namespace ash
