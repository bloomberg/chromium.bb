// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/default_system_tray_delegate.h"

#include <string>

namespace ash {

DefaultSystemTrayDelegate::DefaultSystemTrayDelegate()
    : bluetooth_enabled_(true) {}

DefaultSystemTrayDelegate::~DefaultSystemTrayDelegate() {}

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

bool DefaultSystemTrayDelegate::ShouldShowSettings() const {
  return true;
}

bool DefaultSystemTrayDelegate::ShouldShowNotificationTray() const {
  return true;
}

void DefaultSystemTrayDelegate::ToggleBluetooth() {
  bluetooth_enabled_ = !bluetooth_enabled_;
}

bool DefaultSystemTrayDelegate::IsBluetoothDiscovering() const {
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

int DefaultSystemTrayDelegate::GetSystemTrayMenuWidth() {
  // This is the default width for English languages.
  return 300;
}

}  // namespace ash
