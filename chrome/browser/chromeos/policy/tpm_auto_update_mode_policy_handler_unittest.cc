// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/tpm_auto_update_mode_policy_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/tpm_firmware_update.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class TPMAutoUpdateModePolicyHandlerTest : public testing::Test {
 public:
  TPMAutoUpdateModePolicyHandlerTest() : weak_factory_(this) {}
  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    fake_session_manager_client_ = new chromeos::FakeSessionManagerClient();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::unique_ptr<chromeos::SessionManagerClient>(
            fake_session_manager_client_));
  }

  void TearDown() override { chromeos::DBusThreadManager::Shutdown(); }

  void SetAutoUpdateMode(AutoUpdateMode auto_update_mode) {
    base::DictionaryValue dict;
    dict.SetKey(chromeos::tpm_firmware_update::kSettingsKeyAutoUpdateMode,
                base::Value(static_cast<int>(auto_update_mode)));
    scoped_testing_cros_settings_.device_settings()->Set(
        chromeos::kTPMFirmwareUpdateSettings, dict);
    base::RunLoop().RunUntilIdle();
  }

  void CheckForUpdate(base::OnceCallback<void(bool)> callback) {
    std::move(callback).Run(update_available_);
  }

 protected:
  bool update_available_ = false;

  content::TestBrowserThreadBundle thread_bundle_;
  // Set up fake install attributes to pretend the machine is enrolled.
  chromeos::ScopedStubInstallAttributes test_install_attributes_{
      chromeos::StubInstallAttributes::CreateCloudManaged("example.com",
                                                          "fake-id")};
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;

  chromeos::FakeSessionManagerClient* fake_session_manager_client_ = nullptr;

  base::WeakPtrFactory<TPMAutoUpdateModePolicyHandlerTest> weak_factory_;
};

// Verify if the TPM updates are triggered (or not) according to the device
// policy option TPMFirmwareUpdateSettings.AutoUpdateMode.
TEST_F(TPMAutoUpdateModePolicyHandlerTest, PolicyUpdatesTriggered) {
  TPMAutoUpdateModePolicyHandler tpm_update_policy_handler(
      chromeos::CrosSettings::Get());
  tpm_update_policy_handler.SetUpdateCheckerCallbackForTesting(
      base::BindRepeating(&TPMAutoUpdateModePolicyHandlerTest::CheckForUpdate,
                          weak_factory_.GetWeakPtr()));

  update_available_ = true;

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      0, fake_session_manager_client_->start_tpm_firmware_update_call_count());

  SetAutoUpdateMode(AutoUpdateMode::kWithoutAcknowledgment);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      1, fake_session_manager_client_->start_tpm_firmware_update_call_count());

  SetAutoUpdateMode(AutoUpdateMode::kNever);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      1, fake_session_manager_client_->start_tpm_firmware_update_call_count());

  SetAutoUpdateMode(AutoUpdateMode::kWithoutAcknowledgment);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      2, fake_session_manager_client_->start_tpm_firmware_update_call_count());

  SetAutoUpdateMode(AutoUpdateMode::kEnrollment);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      2, fake_session_manager_client_->start_tpm_firmware_update_call_count());
}

// Verify that the DBus call to start TPM firmware update is not triggered if
// state preserving update is not available.
TEST_F(TPMAutoUpdateModePolicyHandlerTest, NoUpdatesAvailable) {
  TPMAutoUpdateModePolicyHandler tpm_update_policy_handler(
      chromeos::CrosSettings::Get());
  tpm_update_policy_handler.SetUpdateCheckerCallbackForTesting(
      base::BindRepeating(&TPMAutoUpdateModePolicyHandlerTest::CheckForUpdate,
                          weak_factory_.GetWeakPtr()));

  update_available_ = false;

  SetAutoUpdateMode(AutoUpdateMode::kWithoutAcknowledgment);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      0, fake_session_manager_client_->start_tpm_firmware_update_call_count());
}

}  // namespace policy
