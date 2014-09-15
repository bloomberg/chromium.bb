// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/default_system_tray_delegate.h"

#include <string>

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/volume_control_delegate.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"

namespace ash {

namespace {

class DefaultVolumnControlDelegate : public VolumeControlDelegate {
 public:
  DefaultVolumnControlDelegate() {}
  virtual ~DefaultVolumnControlDelegate() {}

  virtual bool HandleVolumeMute(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }
  virtual bool HandleVolumeDown(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }
  virtual bool HandleVolumeUp(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultVolumnControlDelegate);
};

}  // namespace

DefaultSystemTrayDelegate::DefaultSystemTrayDelegate()
    : bluetooth_enabled_(true),
      volume_control_delegate_(new DefaultVolumnControlDelegate) {
}

DefaultSystemTrayDelegate::~DefaultSystemTrayDelegate() {
}

void DefaultSystemTrayDelegate::Initialize() {
}

void DefaultSystemTrayDelegate::Shutdown() {
}

bool DefaultSystemTrayDelegate::GetTrayVisibilityOnStartup() {
  return true;
}

user::LoginStatus DefaultSystemTrayDelegate::GetUserLoginStatus() const {
  return user::LOGGED_IN_USER;
}

void DefaultSystemTrayDelegate::ChangeProfilePicture() {
}

const std::string DefaultSystemTrayDelegate::GetEnterpriseDomain() const {
  return std::string();
}

const base::string16 DefaultSystemTrayDelegate::GetEnterpriseMessage() const {
  return base::string16();
}

const std::string
DefaultSystemTrayDelegate::GetSupervisedUserManager() const {
  return std::string();
}

const base::string16
DefaultSystemTrayDelegate::GetSupervisedUserManagerName()
    const {
  return base::string16();
}

const base::string16 DefaultSystemTrayDelegate::GetSupervisedUserMessage()
    const {
  return base::string16();
}

bool DefaultSystemTrayDelegate::SystemShouldUpgrade() const {
  return true;
}

base::HourClockType DefaultSystemTrayDelegate::GetHourClockType() const {
  return base::k24HourClock;
}

void DefaultSystemTrayDelegate::ShowSettings() {
}

bool DefaultSystemTrayDelegate::ShouldShowSettings() {
  return true;
}

void DefaultSystemTrayDelegate::ShowDateSettings() {
}

void DefaultSystemTrayDelegate::ShowSetTimeDialog() {
}

void DefaultSystemTrayDelegate::ShowNetworkSettings(
    const std::string& service_path) {
}

void DefaultSystemTrayDelegate::ShowBluetoothSettings() {
}

void DefaultSystemTrayDelegate::ShowDisplaySettings() {
}

void DefaultSystemTrayDelegate::ShowChromeSlow() {
}

bool DefaultSystemTrayDelegate::ShouldShowDisplayNotification() {
  return false;
}

void DefaultSystemTrayDelegate::ShowIMESettings() {
}

void DefaultSystemTrayDelegate::ShowHelp() {
}

void DefaultSystemTrayDelegate::ShowAccessibilityHelp() {
}

void DefaultSystemTrayDelegate::ShowAccessibilitySettings() {
}

void DefaultSystemTrayDelegate::ShowPublicAccountInfo() {
}

void DefaultSystemTrayDelegate::ShowEnterpriseInfo() {
}

void DefaultSystemTrayDelegate::ShowSupervisedUserInfo() {
}

void DefaultSystemTrayDelegate::ShowUserLogin() {
}

bool DefaultSystemTrayDelegate::ShowSpringChargerReplacementDialog() {
  return false;
}

bool DefaultSystemTrayDelegate::IsSpringChargerReplacementDialogVisible() {
  return false;
}

bool DefaultSystemTrayDelegate::HasUserConfirmedSafeSpringCharger() {
  return false;
}

void DefaultSystemTrayDelegate::ShutDown() {
}

void DefaultSystemTrayDelegate::SignOut() {
}

void DefaultSystemTrayDelegate::RequestLockScreen() {
}

void DefaultSystemTrayDelegate::RequestRestartForUpdate() {
}

void DefaultSystemTrayDelegate::GetAvailableBluetoothDevices(
    BluetoothDeviceList* list) {
}

void DefaultSystemTrayDelegate::BluetoothStartDiscovering() {
}

void DefaultSystemTrayDelegate::BluetoothStopDiscovering() {
}

void DefaultSystemTrayDelegate::ConnectToBluetoothDevice(
    const std::string& address) {
}

void DefaultSystemTrayDelegate::GetCurrentIME(IMEInfo* info) {
}

void DefaultSystemTrayDelegate::GetAvailableIMEList(IMEInfoList* list) {
}

void DefaultSystemTrayDelegate::GetCurrentIMEProperties(
    IMEPropertyInfoList* list) {
}

void DefaultSystemTrayDelegate::SwitchIME(const std::string& ime_id) {
}

void DefaultSystemTrayDelegate::ActivateIMEProperty(const std::string& key) {
}

void DefaultSystemTrayDelegate::ShowNetworkConfigure(
    const std::string& network_id,
    gfx::NativeWindow parent_window) {
}

bool DefaultSystemTrayDelegate::EnrollNetwork(const std::string& network_id,
                                              gfx::NativeWindow parent_window) {
  return true;
}

void DefaultSystemTrayDelegate::ManageBluetoothDevices() {
}

void DefaultSystemTrayDelegate::ToggleBluetooth() {
  bluetooth_enabled_ = !bluetooth_enabled_;
}

bool DefaultSystemTrayDelegate::IsBluetoothDiscovering() {
  return false;
}

void DefaultSystemTrayDelegate::ShowMobileSimDialog() {
}

void DefaultSystemTrayDelegate::ShowMobileSetupDialog(
    const std::string& service_path) {
}

void DefaultSystemTrayDelegate::ShowOtherNetworkDialog(
    const std::string& type) {
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

void DefaultSystemTrayDelegate::ChangeProxySettings() {
}

VolumeControlDelegate* DefaultSystemTrayDelegate::GetVolumeControlDelegate()
    const {
  return volume_control_delegate_.get();
}

void DefaultSystemTrayDelegate::SetVolumeControlDelegate(
    scoped_ptr<VolumeControlDelegate> delegate) {
  volume_control_delegate_ = delegate.Pass();
}

bool DefaultSystemTrayDelegate::GetSessionStartTime(
    base::TimeTicks* session_start_time) {
  return false;
}

bool DefaultSystemTrayDelegate::GetSessionLengthLimit(
     base::TimeDelta* session_length_limit) {
  return false;
}

int DefaultSystemTrayDelegate::GetSystemTrayMenuWidth() {
  // This is the default width for English languages.
  return 300;
}

void DefaultSystemTrayDelegate::ActiveUserWasChanged() {
}

bool DefaultSystemTrayDelegate::IsSearchKeyMappedToCapsLock() {
  return false;
}

tray::UserAccountsDelegate* DefaultSystemTrayDelegate::GetUserAccountsDelegate(
    const std::string& user_id) {
  return NULL;
}

}  // namespace ash
