// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "policy/proto/device_management_backend.pb.h"

namespace chromeos {
namespace system {

class DeviceDisablingTest : public InProcessBrowserTest {
 public:
  DeviceDisablingTest();

  // Device policy is updated in two steps:
  // - First, set up the policy builder that GetDevicePolicyBuilder() returns.
  // - Second, call SimulatePolicyFetch() to build and flush the resulting
  //   policy blob to the browser.
  policy::DevicePolicyBuilder* GetDevicePolicyBuilder();
  void SimulatePolicyFetch();

 private:
  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  FakeSessionManagerClient* fake_session_manager_client_;
  policy::DevicePolicyCrosTestHelper test_helper_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDisablingTest);
};


DeviceDisablingTest::DeviceDisablingTest()
    : fake_session_manager_client_(new FakeSessionManagerClient) {
}

policy::DevicePolicyBuilder* DeviceDisablingTest::GetDevicePolicyBuilder() {
  return test_helper_.device_policy();
}

void DeviceDisablingTest::SimulatePolicyFetch() {
  GetDevicePolicyBuilder()->Build();
  fake_session_manager_client_->set_device_policy(
      GetDevicePolicyBuilder()->GetBlob());
  fake_session_manager_client_->OnPropertyChangeComplete(true);
}

void DeviceDisablingTest::SetUpInProcessBrowserTestFixture() {
  DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
      scoped_ptr<SessionManagerClient>(fake_session_manager_client_));

  test_helper_.InstallOwnerKey();
  test_helper_.MarkAsEnterpriseOwned();
}

void DeviceDisablingTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kLoginManager);
  command_line->AppendSwitch(switches::kForceLoginManagerInTests);
}

IN_PROC_BROWSER_TEST_F(DeviceDisablingTest, DisableDuringNormalOperation) {
  // Mark the device as disabled and wait until cros settings update.
  base::RunLoop run_loop;
  scoped_ptr<CrosSettings::ObserverSubscription> observer =
      CrosSettings::Get()->AddSettingsObserver(
           kDeviceDisabled,
           run_loop.QuitClosure());
  GetDevicePolicyBuilder()->policy_data().mutable_device_state()->
      set_device_mode(enterprise_management::DeviceState::DEVICE_MODE_DISABLED);
  SimulatePolicyFetch();
  run_loop.Run();

  // Verify that the device disabled screen is being shown.
  WizardController* wizard_controller = WizardController::default_controller();
  ASSERT_TRUE(wizard_controller);
  EXPECT_EQ(wizard_controller->GetScreen(
                WizardController::kDeviceDisabledScreenName),
            wizard_controller->current_screen());
}

}  // namespace system
}  // namespace chromeos
