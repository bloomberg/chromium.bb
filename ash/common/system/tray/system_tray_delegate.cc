// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/system_tray_delegate.h"

#include "ash/common/system/tray/ime_info.h"
#include "ash/common/system/tray/system_tray_item.h"

namespace ash {

BluetoothDeviceInfo::BluetoothDeviceInfo()
    : connected(false), connecting(false), paired(false) {}

BluetoothDeviceInfo::BluetoothDeviceInfo(const BluetoothDeviceInfo& other) =
    default;

BluetoothDeviceInfo::~BluetoothDeviceInfo() {}

UpdateInfo::UpdateInfo()
    : severity(UPDATE_NONE),
      update_required(false),
      factory_reset_required(false) {}

UpdateInfo::~UpdateInfo() {}

SystemTrayDelegate::SystemTrayDelegate() {}

SystemTrayDelegate::~SystemTrayDelegate() {}

void SystemTrayDelegate::Initialize() {}

LoginStatus SystemTrayDelegate::GetUserLoginStatus() const {
  return LoginStatus::NOT_LOGGED_IN;
}

std::string SystemTrayDelegate::GetEnterpriseDomain() const {
  return std::string();
}

std::string SystemTrayDelegate::GetEnterpriseRealm() const {
  return std::string();
}

base::string16 SystemTrayDelegate::GetEnterpriseMessage() const {
  return base::string16();
}

std::string SystemTrayDelegate::GetSupervisedUserManager() const {
  return std::string();
}

base::string16 SystemTrayDelegate::GetSupervisedUserManagerName() const {
  return base::string16();
}

base::string16 SystemTrayDelegate::GetSupervisedUserMessage() const {
  return base::string16();
}

bool SystemTrayDelegate::IsUserSupervised() const {
  return false;
}

bool SystemTrayDelegate::IsUserChild() const {
  return false;
}

void SystemTrayDelegate::GetSystemUpdateInfo(UpdateInfo* info) const {
  info->severity = UpdateInfo::UPDATE_NONE;
  info->update_required = false;
  info->factory_reset_required = false;
}

bool SystemTrayDelegate::ShouldShowSettings() const {
  return false;
}

bool SystemTrayDelegate::ShouldShowNotificationTray() const {
  return false;
}

void SystemTrayDelegate::ShowEnterpriseInfo() {}

void SystemTrayDelegate::ShowUserLogin() {}

void SystemTrayDelegate::GetAvailableBluetoothDevices(
    BluetoothDeviceList* list) {}

void SystemTrayDelegate::BluetoothStartDiscovering() {}

void SystemTrayDelegate::BluetoothStopDiscovering() {}

void SystemTrayDelegate::ConnectToBluetoothDevice(const std::string& address) {}

void SystemTrayDelegate::GetCurrentIME(IMEInfo* info) {}

void SystemTrayDelegate::GetAvailableIMEList(IMEInfoList* list) {}

void SystemTrayDelegate::GetCurrentIMEProperties(IMEPropertyInfoList* list) {}

void SystemTrayDelegate::SwitchIME(const std::string& ime_id) {}

void SystemTrayDelegate::ActivateIMEProperty(const std::string& key) {}

void SystemTrayDelegate::ManageBluetoothDevices() {}

void SystemTrayDelegate::ToggleBluetooth() {}

bool SystemTrayDelegate::IsBluetoothDiscovering() const {
  return false;
}

bool SystemTrayDelegate::GetBluetoothAvailable() {
  return false;
}

bool SystemTrayDelegate::GetBluetoothEnabled() {
  return false;
}

bool SystemTrayDelegate::GetBluetoothDiscovering() {
  return false;
}

CastConfigDelegate* SystemTrayDelegate::GetCastConfigDelegate() {
  return nullptr;
}

NetworkingConfigDelegate* SystemTrayDelegate::GetNetworkingConfigDelegate()
    const {
  return nullptr;
}

bool SystemTrayDelegate::GetSessionStartTime(
    base::TimeTicks* session_start_time) {
  return false;
}

bool SystemTrayDelegate::GetSessionLengthLimit(
    base::TimeDelta* session_length_limit) {
  return false;
}

int SystemTrayDelegate::GetSystemTrayMenuWidth() {
  return 0;
}

void SystemTrayDelegate::ActiveUserWasChanged() {}

bool SystemTrayDelegate::IsSearchKeyMappedToCapsLock() {
  return false;
}

void SystemTrayDelegate::AddCustodianInfoTrayObserver(
    CustodianInfoTrayObserver* observer) {}

void SystemTrayDelegate::RemoveCustodianInfoTrayObserver(
    CustodianInfoTrayObserver* observer) {}

std::unique_ptr<SystemTrayItem> SystemTrayDelegate::CreateRotationLockTrayItem(
    SystemTray* tray) {
  return nullptr;
}

}  // namespace ash
