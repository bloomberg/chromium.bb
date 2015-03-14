// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_delegate.h"

namespace ash {

NetworkIconInfo::NetworkIconInfo()
    : connecting(false),
      connected(false),
      tray_icon_visible(true),
      is_cellular(false) {
}

NetworkIconInfo::~NetworkIconInfo() {
}

BluetoothDeviceInfo::BluetoothDeviceInfo()
    : connected(false), connecting(false), paired(false) {
}

BluetoothDeviceInfo::~BluetoothDeviceInfo() {
}

IMEInfo::IMEInfo() : selected(false), third_party(false) {
}

IMEInfo::~IMEInfo() {
}

IMEPropertyInfo::IMEPropertyInfo() : selected(false) {
}

IMEPropertyInfo::~IMEPropertyInfo() {
}

UpdateInfo::UpdateInfo()
    : severity(UPDATE_NORMAL),
      update_required(false),
      factory_reset_required(false) {
}

UpdateInfo::~UpdateInfo() {
}

SystemTrayDelegate::SystemTrayDelegate() {
}

SystemTrayDelegate::~SystemTrayDelegate() {
}

void SystemTrayDelegate::Initialize() {
}

void SystemTrayDelegate::Shutdown() {
}

bool SystemTrayDelegate::GetTrayVisibilityOnStartup() {
  return false;
}

user::LoginStatus SystemTrayDelegate::GetUserLoginStatus() const {
  return user::LOGGED_IN_NONE;
}

void SystemTrayDelegate::ChangeProfilePicture() {
}

const std::string SystemTrayDelegate::GetEnterpriseDomain() const {
  return std::string();
}

const base::string16 SystemTrayDelegate::GetEnterpriseMessage() const {
  return base::string16();
}

const std::string SystemTrayDelegate::GetSupervisedUserManager() const {
  return std::string();
}

const base::string16 SystemTrayDelegate::GetSupervisedUserManagerName() const {
  return base::string16();
}

const base::string16 SystemTrayDelegate::GetSupervisedUserMessage() const {
  return base::string16();
}

bool SystemTrayDelegate::IsUserSupervised() const {
  return false;
}

bool SystemTrayDelegate::IsUserChild() const {
  return false;
}

void SystemTrayDelegate::GetSystemUpdateInfo(UpdateInfo* info) const {
  info->severity = UpdateInfo::UPDATE_NORMAL;
  info->update_required = false;
  info->factory_reset_required = false;
}

base::HourClockType SystemTrayDelegate::GetHourClockType() const {
  return base::k24HourClock;
}

void SystemTrayDelegate::ShowSettings() {
}

bool SystemTrayDelegate::ShouldShowSettings() {
  return false;
}

void SystemTrayDelegate::ShowDateSettings() {
}

void SystemTrayDelegate::ShowSetTimeDialog() {
}

void SystemTrayDelegate::ShowNetworkSettingsForGuid(const std::string& guid) {
}

void SystemTrayDelegate::ShowBluetoothSettings() {
}

void SystemTrayDelegate::ShowDisplaySettings() {
}

void SystemTrayDelegate::ShowChromeSlow() {
}

bool SystemTrayDelegate::ShouldShowDisplayNotification() {
  return false;
}

void SystemTrayDelegate::ShowIMESettings() {
}

void SystemTrayDelegate::ShowHelp() {
}

void SystemTrayDelegate::ShowAccessibilityHelp() {
}

void SystemTrayDelegate::ShowAccessibilitySettings() {
}

void SystemTrayDelegate::ShowPublicAccountInfo() {
}

void SystemTrayDelegate::ShowEnterpriseInfo() {
}

void SystemTrayDelegate::ShowSupervisedUserInfo() {
}

void SystemTrayDelegate::ShowUserLogin() {
}

void SystemTrayDelegate::SignOut() {
}

void SystemTrayDelegate::RequestLockScreen() {
}

void SystemTrayDelegate::RequestRestartForUpdate() {
}

void SystemTrayDelegate::GetAvailableBluetoothDevices(
    BluetoothDeviceList* list) {
}

void SystemTrayDelegate::BluetoothStartDiscovering() {
}

void SystemTrayDelegate::BluetoothStopDiscovering() {
}

void SystemTrayDelegate::ConnectToBluetoothDevice(const std::string& address) {
}

void SystemTrayDelegate::GetCurrentIME(IMEInfo* info) {
}

void SystemTrayDelegate::GetAvailableIMEList(IMEInfoList* list) {
}

void SystemTrayDelegate::GetCurrentIMEProperties(IMEPropertyInfoList* list) {
}

void SystemTrayDelegate::SwitchIME(const std::string& ime_id) {
}

void SystemTrayDelegate::ActivateIMEProperty(const std::string& key) {
}

void SystemTrayDelegate::ManageBluetoothDevices() {
}

void SystemTrayDelegate::ToggleBluetooth() {
}

bool SystemTrayDelegate::IsBluetoothDiscovering() {
  return false;
}

void SystemTrayDelegate::ShowOtherNetworkDialog(const std::string& type) {
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

void SystemTrayDelegate::ChangeProxySettings() {
}

NetworkingConfigDelegate* SystemTrayDelegate::GetNetworkingConfigDelegate()
    const {
  return nullptr;
}

VolumeControlDelegate* SystemTrayDelegate::GetVolumeControlDelegate() const {
  return nullptr;
}

void SystemTrayDelegate::SetVolumeControlDelegate(
    scoped_ptr<VolumeControlDelegate> delegate) {
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

void SystemTrayDelegate::ActiveUserWasChanged() {
}

bool SystemTrayDelegate::IsSearchKeyMappedToCapsLock() {
  return false;
}

tray::UserAccountsDelegate* SystemTrayDelegate::GetUserAccountsDelegate(
    const std::string& user_id) {
  return nullptr;
}

void SystemTrayDelegate::AddCustodianInfoTrayObserver(
    CustodianInfoTrayObserver* observer) {
}

void SystemTrayDelegate::RemoveCustodianInfoTrayObserver(
    CustodianInfoTrayObserver* observer) {
}

void SystemTrayDelegate::AddShutdownPolicyObserver(
    ShutdownPolicyObserver* observer) {
}

void SystemTrayDelegate::RemoveShutdownPolicyObserver(
    ShutdownPolicyObserver* observer) {
}

void SystemTrayDelegate::ShouldRebootOnShutdown(
    const RebootOnShutdownCallback& callback) {
}

}  // namespace ash
