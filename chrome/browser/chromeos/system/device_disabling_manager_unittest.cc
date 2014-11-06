// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/device_disabling_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/browser/chromeos/policy/stub_enterprise_install_attributes.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/chromeos_switches.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace system {

namespace {

const char kDisabledMessage[] = "Device disabled.";

}

class DeviceDisablingManagerTest : public testing::Test {
 public:
  DeviceDisablingManagerTest();

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  bool device_disabled() const { return device_disabled_; }
  const std::string& GetDisabledMessage() const;

  void CheckWhetherDeviceDisabledDuringOOBE();

  void SetDeviceDisabled(bool disabled);
  void SetDeviceMode(policy::DeviceMode device_mode);

 private:
  void OnDeviceDisabledChecked(bool device_disabled);

  policy::ScopedStubEnterpriseInstallAttributes install_attributes_;
  TestingPrefServiceSimple local_state_;

  scoped_ptr<DeviceDisablingManager> device_disabling_manager_;

  base::RunLoop run_loop_;
  bool device_disabled_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDisablingManagerTest);
};

DeviceDisablingManagerTest::DeviceDisablingManagerTest()
    : install_attributes_("", "", "", policy::DEVICE_MODE_NOT_SET),
      device_disabled_(false) {
}

void DeviceDisablingManagerTest::SetUp() {
  TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
  policy::DeviceCloudPolicyManagerChromeOS::RegisterPrefs(
      local_state_.registry());

  device_disabling_manager_.reset(new DeviceDisablingManager(
      TestingBrowserProcess::GetGlobal()->platform_part()->
          browser_policy_connector_chromeos()));
}

void DeviceDisablingManagerTest::TearDown() {
  TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
}

const std::string& DeviceDisablingManagerTest::GetDisabledMessage() const {
  return device_disabling_manager_->disabled_message();

}

void DeviceDisablingManagerTest::CheckWhetherDeviceDisabledDuringOOBE() {
  device_disabling_manager_->CheckWhetherDeviceDisabledDuringOOBE(
      base::Bind(&DeviceDisablingManagerTest::OnDeviceDisabledChecked,
                 base::Unretained(this)));
  run_loop_.Run();
}

void DeviceDisablingManagerTest::SetDeviceDisabled(bool disabled) {
  DictionaryPrefUpdate dict(&local_state_, prefs::kServerBackedDeviceState);
  if (disabled) {
    dict->SetString(policy::kDeviceStateRestoreMode,
                    policy::kDeviceStateRestoreModeDisabled);
  } else {
    dict->Remove(policy::kDeviceStateRestoreMode, nullptr);
  }
  dict->SetString(policy::kDeviceStateDisabledMessage, kDisabledMessage);
}

void DeviceDisablingManagerTest::SetDeviceMode(policy::DeviceMode device_mode) {
  reinterpret_cast<policy::StubEnterpriseInstallAttributes*>(
      TestingBrowserProcess::GetGlobal()->platform_part()->
          browser_policy_connector_chromeos()->GetInstallAttributes())->
              SetMode(device_mode);
}

void DeviceDisablingManagerTest::OnDeviceDisabledChecked(bool device_disabled) {
  device_disabled_ = device_disabled;
  run_loop_.Quit();
}

// Verifies that the device is not considered disabled during OOBE by default.
TEST_F(DeviceDisablingManagerTest, NotDisabledByDefault) {
  CheckWhetherDeviceDisabledDuringOOBE();
  EXPECT_FALSE(device_disabled());
}

// Verifies that the device is not considered disabled during OOBE when it is
// explicitly marked as not disabled.
TEST_F(DeviceDisablingManagerTest, NotDisabledWhenExplicitlyNotDisabled) {
  SetDeviceDisabled(false);
  CheckWhetherDeviceDisabledDuringOOBE();
  EXPECT_FALSE(device_disabled());
}

// Verifies that the device is not considered disabled during OOBE when device
// disabling is turned off by flag, even if the device is marked as disabled.
TEST_F(DeviceDisablingManagerTest, NotDisabledWhenTurnedOffByFlag) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDeviceDisabling);
  SetDeviceDisabled(true);
  CheckWhetherDeviceDisabledDuringOOBE();
  EXPECT_FALSE(device_disabled());
}

// Verifies that the device is not considered disabled during OOBE when it is
// already enrolled, even if the device is marked as disabled.
TEST_F(DeviceDisablingManagerTest, DoNotShowWhenEnterpriseOwned) {
  SetDeviceMode(policy::DEVICE_MODE_ENTERPRISE);
  SetDeviceDisabled(true);
  CheckWhetherDeviceDisabledDuringOOBE();
  EXPECT_FALSE(device_disabled());
}

// Verifies that the device is not considered disabled during OOBE when it is
// already owned by a consumer, even if the device is marked as disabled.
TEST_F(DeviceDisablingManagerTest, DoNotShowWhenConsumerOwned) {
  SetDeviceMode(policy::DEVICE_MODE_CONSUMER);
  SetDeviceDisabled(true);
  CheckWhetherDeviceDisabledDuringOOBE();
  EXPECT_FALSE(device_disabled());
}

// Verifies that the device is considered disabled during OOBE when it is marked
// as disabled, device disabling is not turned off by flag and the device is not
// owned yet.
TEST_F(DeviceDisablingManagerTest, ShowWhenDisabledAndNotOwned) {
  SetDeviceDisabled(true);
  CheckWhetherDeviceDisabledDuringOOBE();
  EXPECT_TRUE(device_disabled());
  EXPECT_EQ(kDisabledMessage, GetDisabledMessage());
}

}  // namespace system
}  // namespace chromeos
