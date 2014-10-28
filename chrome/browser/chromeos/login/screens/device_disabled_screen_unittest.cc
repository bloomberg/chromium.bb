// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/device_disabled_screen.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/mock_device_disabled_screen_actor.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/browser/chromeos/policy/stub_enterprise_install_attributes.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/chromeos_switches.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace chromeos {

namespace {

const char kDisabledMessage[] = "Device disabled.";

}

class DeviceDisabledScreenTest : public testing::Test,
                                 public BaseScreenDelegate {
 public:
  DeviceDisabledScreenTest();
  ~DeviceDisabledScreenTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // BaseScreenDelegate:
  MOCK_METHOD1(OnExit, void(ExitCodes));
  void ShowCurrentScreen() override;
  ErrorScreen* GetErrorScreen() override;
  void ShowErrorScreen() override;
  void HideErrorScreen(BaseScreen* parent_screen) override;

  void SetDeviceDisabled(bool disabled);
  void SetDeviceMode(policy::DeviceMode device_mode);

  void ExpectScreenToNotShow();
  void ExpectScreenToShow();

  void TryToShowScreen();

 private:
  scoped_ptr<DeviceDisabledScreen> screen_;
  scoped_ptr<MockDeviceDisabledScreenActor> actor_;
  TestingPrefServiceSimple local_state_;
  policy::ScopedStubEnterpriseInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDisabledScreenTest);
};

DeviceDisabledScreenTest::DeviceDisabledScreenTest()
    : install_attributes_("", "", "", policy::DEVICE_MODE_NOT_SET) {
}

DeviceDisabledScreenTest::~DeviceDisabledScreenTest() {
}

void DeviceDisabledScreenTest::SetUp() {
  TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
  policy::DeviceCloudPolicyManagerChromeOS::RegisterPrefs(
      local_state_.registry());

  actor_.reset(new MockDeviceDisabledScreenActor);
  screen_.reset(new DeviceDisabledScreen(this, actor_.get()));
}

void DeviceDisabledScreenTest::TearDown() {
  TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
}

void DeviceDisabledScreenTest::ShowCurrentScreen() {
}

ErrorScreen* DeviceDisabledScreenTest::GetErrorScreen() {
  return nullptr;
}

void DeviceDisabledScreenTest::ShowErrorScreen() {
}

void DeviceDisabledScreenTest::HideErrorScreen(BaseScreen* parent_screen) {
}

void DeviceDisabledScreenTest::SetDeviceDisabled(bool disabled) {
  DictionaryPrefUpdate dict(&local_state_, prefs::kServerBackedDeviceState);
  dict->SetBoolean(policy::kDeviceStateDisabled, disabled);
  if (disabled)
    dict->SetString(policy::kDeviceStateDisabledMessage, kDisabledMessage);
}

void DeviceDisabledScreenTest::SetDeviceMode(policy::DeviceMode device_mode) {
  reinterpret_cast<policy::StubEnterpriseInstallAttributes*>(
      TestingBrowserProcess::GetGlobal()->platform_part()->
          browser_policy_connector_chromeos()->GetInstallAttributes())->
              SetMode(device_mode);
}

void DeviceDisabledScreenTest::ExpectScreenToNotShow() {
  EXPECT_CALL(*actor_, Show(_)).Times(0);
  EXPECT_CALL(*this, OnExit(BaseScreenDelegate::DEVICE_NOT_DISABLED)).Times(1);
}

void DeviceDisabledScreenTest::ExpectScreenToShow() {
  EXPECT_CALL(*actor_, Show(kDisabledMessage)).Times(1);
  EXPECT_CALL(*this, OnExit(BaseScreenDelegate::DEVICE_NOT_DISABLED)).Times(0);
}

void DeviceDisabledScreenTest::TryToShowScreen() {
  screen_->Show();
}

// Verifies that the device disabled screen is not shown by default.
TEST_F(DeviceDisabledScreenTest, DoNotShowByDefault) {
  ExpectScreenToNotShow();
  TryToShowScreen();
}

// Verifies that the device disabled screen is not shown when the device is
// explicitly marked as not disabled.
TEST_F(DeviceDisabledScreenTest, DoNotShowWhenNotDisabled) {
  SetDeviceDisabled(false);
  ExpectScreenToNotShow();
  TryToShowScreen();
}

// Verifies that the device disabled screen is not shown when device disabling
// is turned off by flag, even if the device is marked as disabled.
TEST_F(DeviceDisabledScreenTest, DoNotShowWhenTurnedOffByFlag) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDeviceDisabling);
  SetDeviceDisabled(true);
  ExpectScreenToNotShow();
  TryToShowScreen();
}

// Verifies that the device disabled screen is not shown when the device is
// already enrolled, even if the device is marked as disabled.
TEST_F(DeviceDisabledScreenTest, DoNotShowWhenEnterpriseOwned) {
  SetDeviceMode(policy::DEVICE_MODE_ENTERPRISE);
  SetDeviceDisabled(true);
  ExpectScreenToNotShow();
  TryToShowScreen();
}

// Verifies that the device disabled screen is not shown when the device is
// already owned by a consumer, even if the device is marked as disabled.
TEST_F(DeviceDisabledScreenTest, DoNotShowWhenConsumerOwned) {
  SetDeviceMode(policy::DEVICE_MODE_CONSUMER);
  SetDeviceDisabled(true);
  ExpectScreenToNotShow();
  TryToShowScreen();
}

// Verifies that the device disabled screen is shown when the device is marked
// as disabled, device disabling is not turned off by flag and the device is not
// owned yet.
TEST_F(DeviceDisabledScreenTest, ShowWhenDisabledAndNotOwned) {
  SetDeviceDisabled(true);
  ExpectScreenToShow();
  TryToShowScreen();
}

}  // namespace chromeos
