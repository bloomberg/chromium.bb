// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_unenrollment_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/ownership/fake_owner_settings_service.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/policy/consumer_management_stage.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/policy/fake_consumer_management_service.h"
#include "chrome/browser/chromeos/policy/fake_device_cloud_policy_manager.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class ConsumerUnenrollmentHandlerTest
    : public chromeos::DeviceSettingsTestBase {
 public:
  ConsumerUnenrollmentHandlerTest()
      : fake_service_(new FakeConsumerManagementService()),
        fake_cryptohome_client_(new chromeos::FakeCryptohomeClient()),
        install_attributes_(
            new EnterpriseInstallAttributes(fake_cryptohome_client_.get())) {
    // Set up FakeConsumerManagementService.
    fake_service_->SetStatusAndStage(
        ConsumerManagementService::STATUS_ENROLLED,
        ConsumerManagementStage::None());
  }

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    // Set up the ownership, so that we can modify device settings.
    owner_key_util_->SetPrivateKey(device_policy_.GetSigningKey());
    InitOwner(AccountId::FromUserEmail(device_policy_.policy_data().username()),
              true);
    FlushDeviceSettings();


    // Set up FakeDeviceCloudPolicyManager.
    scoped_ptr<DeviceCloudPolicyStoreChromeOS> store_(
        new DeviceCloudPolicyStoreChromeOS(
            &device_settings_service_,
            install_attributes_.get(),
            base::ThreadTaskRunnerHandle::Get()));
    fake_manager_.reset(new FakeDeviceCloudPolicyManager(
        store_.Pass(),
        base::ThreadTaskRunnerHandle::Get()));

    // Set up FakeOwnerSettingsService.
    fake_owner_settings_service_.reset(new chromeos::FakeOwnerSettingsService(
        profile_.get(), owner_key_util_, nullptr));
    chromeos::OwnerSettingsServiceChromeOS::ManagementSettings settings;
    settings.management_mode = policy::MANAGEMENT_MODE_CONSUMER_MANAGED;
    settings.request_token = "fake_request_token";
    settings.device_id = "fake_device_id";
    fake_owner_settings_service_->SetManagementSettings(
        settings,
        base::Bind(&ConsumerUnenrollmentHandlerTest::OnManagementSettingsSet,
                   base::Unretained(this)));
  }

  void OnManagementSettingsSet(bool success) {
    EXPECT_TRUE(success);
  }

  void RunUnenrollment() {
    handler_.reset(new ConsumerUnenrollmentHandler(
        &device_settings_service_,
        fake_service_.get(),
        fake_manager_.get(),
        fake_owner_settings_service_.get()));
    handler_->Start();
    FlushDeviceSettings();
  }

  scoped_ptr<FakeConsumerManagementService> fake_service_;
  scoped_ptr<chromeos::FakeCryptohomeClient> fake_cryptohome_client_;
  scoped_ptr<EnterpriseInstallAttributes> install_attributes_;
  scoped_ptr<FakeDeviceCloudPolicyManager> fake_manager_;
  scoped_ptr<chromeos::FakeOwnerSettingsService> fake_owner_settings_service_;

  scoped_ptr<ConsumerUnenrollmentHandler> handler_;
};

TEST_F(ConsumerUnenrollmentHandlerTest, UnenrollmentSucceeds) {
  EXPECT_EQ(ConsumerManagementStage::None(), fake_service_->GetStage());

  RunUnenrollment();

  EXPECT_EQ(ConsumerManagementStage::UnenrollmentSuccess(),
            fake_service_->GetStage());
  const chromeos::OwnerSettingsServiceChromeOS::ManagementSettings& settings =
      fake_owner_settings_service_->last_settings();
  EXPECT_EQ(policy::MANAGEMENT_MODE_LOCAL_OWNER, settings.management_mode);
  EXPECT_EQ("", settings.request_token);
  EXPECT_EQ("", settings.device_id);
}

TEST_F(ConsumerUnenrollmentHandlerTest,
       UnenrollmentFailsOnServerError) {
  EXPECT_EQ(ConsumerManagementStage::None(), fake_service_->GetStage());
  fake_manager_->set_unregister_result(false);

  RunUnenrollment();

  EXPECT_EQ(ConsumerManagementStage::UnenrollmentDMServerFailed(),
            fake_service_->GetStage());
  const chromeos::OwnerSettingsServiceChromeOS::ManagementSettings& settings =
      fake_owner_settings_service_->last_settings();
  EXPECT_EQ(policy::MANAGEMENT_MODE_CONSUMER_MANAGED, settings.management_mode);
  EXPECT_EQ("fake_request_token", settings.request_token);
  EXPECT_EQ("fake_device_id", settings.device_id);
}

}  // namespace policy
