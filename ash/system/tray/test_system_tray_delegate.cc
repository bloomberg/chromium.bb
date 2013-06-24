// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/test_system_tray_delegate.h"

#include <string>

#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/volume_control_delegate.h"
#include "base/message_loop.h"
#include "base/time.h"

namespace ash {
namespace test {

namespace {

class TestVolumeControlDelegate : public VolumeControlDelegate {
 public:
  TestVolumeControlDelegate() {}
  virtual ~TestVolumeControlDelegate() {}

  virtual bool HandleVolumeMute(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }
  virtual bool HandleVolumeDown(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }
  virtual bool HandleVolumeUp(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }
  virtual void SetVolumePercent(double percent) OVERRIDE {
  }
  virtual bool IsAudioMuted() const OVERRIDE {
    return true;
  }
  virtual void SetAudioMuted(bool muted) OVERRIDE {
  }
  virtual float GetVolumeLevel() const OVERRIDE {
    return 0.0;
  }
  virtual void SetVolumeLevel(float level) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestVolumeControlDelegate);
};

}  // namespace

TestSystemTrayDelegate::TestSystemTrayDelegate()
    : bluetooth_enabled_(true),
      caps_lock_enabled_(false),
      should_show_display_notification_(false),
      volume_control_delegate_(new TestVolumeControlDelegate) {
}

TestSystemTrayDelegate::~TestSystemTrayDelegate() {
}

void TestSystemTrayDelegate::Initialize() {
}

void TestSystemTrayDelegate::Shutdown() {
}

bool TestSystemTrayDelegate::GetTrayVisibilityOnStartup() {
  return true;
}

// Overridden from SystemTrayDelegate:
user::LoginStatus TestSystemTrayDelegate::GetUserLoginStatus() const {
  // At new user image screen manager->IsUserLoggedIn() would return true
  // but there's no browser session available yet so use SessionStarted().
  SessionStateDelegate* delegate =
      Shell::GetInstance()->session_state_delegate();

  if (!delegate->IsActiveUserSessionStarted())
    return ash::user::LOGGED_IN_NONE;
  if (delegate->IsScreenLocked())
    return user::LOGGED_IN_LOCKED;
  // TODO(nkostylev): Support LOGGED_IN_OWNER, LOGGED_IN_GUEST, LOGGED_IN_KIOSK,
  //                  LOGGED_IN_PUBLIC.
  return user::LOGGED_IN_USER;
}

bool TestSystemTrayDelegate::IsOobeCompleted() const {
  return true;
}

void TestSystemTrayDelegate::ChangeProfilePicture() {
}

const std::string TestSystemTrayDelegate::GetEnterpriseDomain() const {
  return std::string();
}

const base::string16 TestSystemTrayDelegate::GetEnterpriseMessage() const {
  return string16();
}

const std::string TestSystemTrayDelegate::GetLocallyManagedUserManager() const {
  return std::string();
}

const base::string16 TestSystemTrayDelegate::GetLocallyManagedUserManagerName()
    const {
  return string16();
}

const base::string16 TestSystemTrayDelegate::GetLocallyManagedUserMessage()
    const {
  return string16();
}

bool TestSystemTrayDelegate::SystemShouldUpgrade() const {
  return true;
}

base::HourClockType TestSystemTrayDelegate::GetHourClockType() const {
  return base::k24HourClock;
}

void TestSystemTrayDelegate::ShowSettings() {
}

void TestSystemTrayDelegate::ShowDateSettings() {
}

void TestSystemTrayDelegate::ShowNetworkSettings(
    const std::string& service_path) {
}

void TestSystemTrayDelegate::ShowBluetoothSettings() {
}

void TestSystemTrayDelegate::ShowDisplaySettings() {
}

bool TestSystemTrayDelegate::ShouldShowDisplayNotification() {
  return should_show_display_notification_;
}

void TestSystemTrayDelegate::ShowDriveSettings() {
}

void TestSystemTrayDelegate::ShowIMESettings() {
}

void TestSystemTrayDelegate::ShowHelp() {
}

void TestSystemTrayDelegate::ShowAccessibilityHelp() {
}

void TestSystemTrayDelegate::ShowAccessibilitySettings() {
}

void TestSystemTrayDelegate::ShowPublicAccountInfo() {
}

void TestSystemTrayDelegate::ShowEnterpriseInfo() {
}

void TestSystemTrayDelegate::ShowLocallyManagedUserInfo() {
}

void TestSystemTrayDelegate::ShowUserLogin() {
}

void TestSystemTrayDelegate::ShutDown() {
  base::MessageLoop::current()->Quit();
}

void TestSystemTrayDelegate::SignOut() {
  base::MessageLoop::current()->Quit();
}

void TestSystemTrayDelegate::RequestLockScreen() {
}

void TestSystemTrayDelegate::RequestRestartForUpdate() {
}

void TestSystemTrayDelegate::GetAvailableBluetoothDevices(
    BluetoothDeviceList* list) {
}

void TestSystemTrayDelegate::BluetoothStartDiscovering() {
}

void TestSystemTrayDelegate::BluetoothStopDiscovering() {
}

void TestSystemTrayDelegate::ConnectToBluetoothDevice(
    const std::string& address) {
}

void TestSystemTrayDelegate::GetCurrentIME(IMEInfo* info) {
}

void TestSystemTrayDelegate::GetAvailableIMEList(IMEInfoList* list) {
}

void TestSystemTrayDelegate::GetCurrentIMEProperties(
    IMEPropertyInfoList* list) {
}

void TestSystemTrayDelegate::SwitchIME(const std::string& ime_id) {
}

void TestSystemTrayDelegate::ActivateIMEProperty(const std::string& key) {
}

void TestSystemTrayDelegate::CancelDriveOperation(int32 operation_id) {
}

void TestSystemTrayDelegate::GetDriveOperationStatusList(
    ash::DriveOperationStatusList*) {
}

void TestSystemTrayDelegate::ConfigureNetwork(const std::string& network_id) {
}

void TestSystemTrayDelegate::ConnectToNetwork(const std::string& network_id) {
}

void TestSystemTrayDelegate::AddBluetoothDevice() {
}

void TestSystemTrayDelegate::ToggleBluetooth() {
  bluetooth_enabled_ = !bluetooth_enabled_;
}

bool TestSystemTrayDelegate::IsBluetoothDiscovering() {
  return false;
}

void TestSystemTrayDelegate::ShowMobileSimDialog() {
}

void TestSystemTrayDelegate::ShowOtherWifi() {
}

void TestSystemTrayDelegate::ShowOtherVPN() {
}

void TestSystemTrayDelegate::ShowOtherCellular() {
}

bool TestSystemTrayDelegate::GetBluetoothAvailable() {
  return true;
}

bool TestSystemTrayDelegate::GetBluetoothEnabled() {
  return bluetooth_enabled_;
}

bool TestSystemTrayDelegate::GetCellularCarrierInfo(std::string* carrier_id,
                                                    std::string* topup_url,
                                                    std::string* setup_url) {
  return false;
}

void TestSystemTrayDelegate::ShowCellularURL(const std::string& url) {
}

void TestSystemTrayDelegate::ChangeProxySettings() {
}

VolumeControlDelegate* TestSystemTrayDelegate::GetVolumeControlDelegate()
    const {
  return volume_control_delegate_.get();
}

void TestSystemTrayDelegate::SetVolumeControlDelegate(
    scoped_ptr<VolumeControlDelegate> delegate) {
  volume_control_delegate_ = delegate.Pass();
}

bool TestSystemTrayDelegate::GetSessionStartTime(
    base::TimeTicks* session_start_time) {
  return false;
}

bool TestSystemTrayDelegate::GetSessionLengthLimit(
     base::TimeDelta* session_length_limit) {
  return false;
}

int TestSystemTrayDelegate::GetSystemTrayMenuWidth() {
  // This is the default width for English languages.
  return 300;
}

base::string16 TestSystemTrayDelegate::FormatTimeDuration(
    const base::TimeDelta& delta) const {
  return base::string16();
}

void TestSystemTrayDelegate::MaybeSpeak(const std::string& utterance) const {
}

}  // namespace test
}  // namespace ash
