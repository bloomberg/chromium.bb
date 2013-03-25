// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/test_system_tray_delegate.h"

#include <string>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/volume_control_delegate.h"
#include "base/utf_string_conversions.h"
#include "base/message_loop.h"
#include "base/string16.h"
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
    : wifi_enabled_(true),
      cellular_enabled_(true),
      bluetooth_enabled_(true),
      caps_lock_enabled_(false),
      volume_control_delegate_(
          ALLOW_THIS_IN_INITIALIZER_LIST(new TestVolumeControlDelegate)) {
}

TestSystemTrayDelegate::~TestSystemTrayDelegate() {
}

void TestSystemTrayDelegate::Initialize() {
}

bool TestSystemTrayDelegate::GetTrayVisibilityOnStartup() {
  return true;
}

// Overridden from SystemTrayDelegate:
const string16 TestSystemTrayDelegate::GetUserDisplayName() const {
  return UTF8ToUTF16("Über tray Über tray Über tray Über tray");
}

const std::string TestSystemTrayDelegate::GetUserEmail() const {
  return "über@tray";
}

const gfx::ImageSkia& TestSystemTrayDelegate::GetUserImage() const {
  return null_image_;
}

user::LoginStatus TestSystemTrayDelegate::GetUserLoginStatus() const {
  // At new user image screen manager->IsUserLoggedIn() would return true
  // but there's no browser session available yet so use SessionStarted().
  if (!Shell::GetInstance()->delegate()->IsSessionStarted())
    return ash::user::LOGGED_IN_NONE;
  if (Shell::GetInstance()->IsScreenLocked())
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

const string16 TestSystemTrayDelegate::GetEnterpriseMessage() const {
  return string16();
}

bool TestSystemTrayDelegate::SystemShouldUpgrade() const {
  return true;
}

base::HourClockType TestSystemTrayDelegate::GetHourClockType() const {
  return base::k24HourClock;
}

PowerSupplyStatus TestSystemTrayDelegate::GetPowerSupplyStatus() const {
  return PowerSupplyStatus();
}

void TestSystemTrayDelegate::RequestStatusUpdate() const {
}

void TestSystemTrayDelegate::ShowSettings() {
}

void TestSystemTrayDelegate::ShowDateSettings() {
}

void TestSystemTrayDelegate::ShowNetworkSettings() {
}

void TestSystemTrayDelegate::ShowBluetoothSettings() {
}

void TestSystemTrayDelegate::ShowDisplaySettings() {
}

void TestSystemTrayDelegate::ShowDriveSettings() {
}

void TestSystemTrayDelegate::ShowIMESettings() {
}

void TestSystemTrayDelegate::ShowHelp() {
}

void TestSystemTrayDelegate::ShowAccessibilityHelp() {
}

void TestSystemTrayDelegate::ShowPublicAccountInfo() {
}

void TestSystemTrayDelegate::ShowEnterpriseInfo() {
}

void TestSystemTrayDelegate::ShutDown() {
  MessageLoop::current()->Quit();
}

void TestSystemTrayDelegate::SignOut() {
  MessageLoop::current()->Quit();
}

void TestSystemTrayDelegate::RequestLockScreen() {
}

void TestSystemTrayDelegate::RequestRestart() {
}

void TestSystemTrayDelegate::GetAvailableBluetoothDevices(
    BluetoothDeviceList* list) {
}

void TestSystemTrayDelegate::BluetoothStartDiscovering() {
}

void TestSystemTrayDelegate::BluetoothStopDiscovering() {
}

void TestSystemTrayDelegate::ToggleBluetoothConnection(
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

void TestSystemTrayDelegate::CancelDriveOperation(const base::FilePath&) {
}

void TestSystemTrayDelegate::GetDriveOperationStatusList(
    ash::DriveOperationStatusList*) {
}

void TestSystemTrayDelegate::GetMostRelevantNetworkIcon(NetworkIconInfo* info,
                                                        bool large) {
}

void TestSystemTrayDelegate::GetVirtualNetworkIcon(ash::NetworkIconInfo* info) {
}

void TestSystemTrayDelegate::GetAvailableNetworks(
    std::vector<NetworkIconInfo>* list) {
}

void TestSystemTrayDelegate::GetVirtualNetworks(
    std::vector<NetworkIconInfo>* list) {
}

void TestSystemTrayDelegate::ConnectToNetwork(const std::string& network_id) {
}

void TestSystemTrayDelegate::GetNetworkAddresses(
    std::string* ip_address,
    std::string* ethernet_mac_address,
    std::string* wifi_mac_address) {
  *ip_address = "127.0.0.1";
  *ethernet_mac_address = "00:11:22:33:44:55";
  *wifi_mac_address = "66:77:88:99:00:11";
}

void TestSystemTrayDelegate::RequestNetworkScan() {
}

void TestSystemTrayDelegate::AddBluetoothDevice() {
}

void TestSystemTrayDelegate::ToggleAirplaneMode() {
}

void TestSystemTrayDelegate::ToggleWifi() {
  wifi_enabled_ = !wifi_enabled_;
}

void TestSystemTrayDelegate::ToggleMobile() {
  cellular_enabled_ = !cellular_enabled_;
}

void TestSystemTrayDelegate::ToggleBluetooth() {
  bluetooth_enabled_ = !bluetooth_enabled_;
}

bool TestSystemTrayDelegate::IsBluetoothDiscovering() {
  return false;
}

void TestSystemTrayDelegate::ShowOtherWifi() {
}

void TestSystemTrayDelegate::ShowOtherVPN() {
}

void TestSystemTrayDelegate::ShowOtherCellular() {
}

bool TestSystemTrayDelegate::IsNetworkConnected() {
  return true;
}

bool TestSystemTrayDelegate::GetWifiAvailable() {
  return true;
}

bool TestSystemTrayDelegate::GetMobileAvailable() {
  return true;
}

bool TestSystemTrayDelegate::GetBluetoothAvailable() {
  return true;
}

bool TestSystemTrayDelegate::GetWifiEnabled() {
  return wifi_enabled_;
}

bool TestSystemTrayDelegate::GetMobileEnabled() {
  return cellular_enabled_;
}

bool TestSystemTrayDelegate::GetBluetoothEnabled() {
  return bluetooth_enabled_;
}

bool TestSystemTrayDelegate::GetMobileScanSupported() {
  return true;
}

bool TestSystemTrayDelegate::GetCellularCarrierInfo(std::string* carrier_id,
                                                    std::string* topup_url,
                                                    std::string* setup_url) {
  return false;
}

bool TestSystemTrayDelegate::GetWifiScanning() {
  return false;
}

bool TestSystemTrayDelegate::GetCellularInitializing() {
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

string16 TestSystemTrayDelegate::FormatTimeDuration(
    const base::TimeDelta& delta) const {
  return string16();
}

void TestSystemTrayDelegate::MaybeSpeak(const std::string& utterance) const {
}

}  // namespace test
}  // namespace ash
