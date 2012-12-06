// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_cloud_policy_manager_chromeos.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/policy/cloud_policy_client.h"
#include "chrome/browser/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/mock_device_management_service.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_pref_service.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AtMost;
using testing::Mock;
using testing::_;

namespace em = enterprise_management;

namespace policy {
namespace {

class DeviceCloudPolicyManagerChromeOSTest
    : public chromeos::DeviceSettingsTestBase {
 protected:
  DeviceCloudPolicyManagerChromeOSTest()
      : cryptohome_library_(chromeos::CryptohomeLibrary::GetImpl(true)),
        install_attributes_(cryptohome_library_.get()),
        store_(new DeviceCloudPolicyStoreChromeOS(&device_settings_service_,
                                                  &install_attributes_)),
        manager_(make_scoped_ptr(store_), &install_attributes_) {}

  virtual void SetUp() OVERRIDE {
    DeviceSettingsTestBase::SetUp();
    chrome::RegisterLocalState(&local_state_);
    manager_.Init();
  }

  virtual void TearDown() OVERRIDE {
    manager_.Shutdown();
    DeviceSettingsTestBase::TearDown();
  }

  scoped_ptr<chromeos::CryptohomeLibrary> cryptohome_library_;
  EnterpriseInstallAttributes install_attributes_;

  TestingPrefService local_state_;
  MockDeviceManagementService device_management_service_;

  DeviceCloudPolicyStoreChromeOS* store_;
  DeviceCloudPolicyManagerChromeOS manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyManagerChromeOSTest);
};

TEST_F(DeviceCloudPolicyManagerChromeOSTest, FreshDevice) {
  owner_key_util_->Clear();
  FlushDeviceSettings();
  EXPECT_TRUE(manager_.IsInitializationComplete());

  manager_.Connect(&local_state_, &device_management_service_,
                   scoped_ptr<CloudPolicyClient::StatusProvider>(NULL));

  PolicyBundle bundle;
  EXPECT_TRUE(manager_.policies().Equals(bundle));
}

TEST_F(DeviceCloudPolicyManagerChromeOSTest, EnrolledDevice) {
  ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(PolicyBuilder::kFakeUsername,
                                           DEVICE_MODE_ENTERPRISE,
                                           PolicyBuilder::kFakeDeviceId));
  FlushDeviceSettings();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_TRUE(manager_.IsInitializationComplete());

  PolicyBundle bundle;
  bundle.Get(POLICY_DOMAIN_CHROME, std::string()).Set(
      key::kDeviceMetricsReportingEnabled, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_MACHINE, Value::CreateBooleanValue(false));
  EXPECT_TRUE(manager_.policies().Equals(bundle));

  manager_.Connect(&local_state_, &device_management_service_,
                   scoped_ptr<CloudPolicyClient::StatusProvider>(NULL));
  EXPECT_TRUE(manager_.policies().Equals(bundle));

  manager_.Shutdown();
  EXPECT_TRUE(manager_.policies().Equals(bundle));
}

TEST_F(DeviceCloudPolicyManagerChromeOSTest, ConsumerDevice) {
  FlushDeviceSettings();
  EXPECT_EQ(CloudPolicyStore::STATUS_BAD_STATE, store_->status());
  EXPECT_TRUE(manager_.IsInitializationComplete());

  PolicyBundle bundle;
  EXPECT_TRUE(manager_.policies().Equals(bundle));

  manager_.Connect(&local_state_, &device_management_service_,
                   scoped_ptr<CloudPolicyClient::StatusProvider>(NULL));
  EXPECT_TRUE(manager_.policies().Equals(bundle));

  manager_.Shutdown();
  EXPECT_TRUE(manager_.policies().Equals(bundle));
}

class DeviceCloudPolicyManagerChromeOSEnrollmentTest
    : public DeviceCloudPolicyManagerChromeOSTest {
 public:
  void Done(EnrollmentStatus status) {
    EXPECT_EQ(expected_enrollment_status_, status.status());
    EXPECT_EQ(expected_dm_status_, status.client_status());
    EXPECT_EQ(expected_validation_status_, status.validation_status());
    EXPECT_EQ(expected_store_status_, status.store_status());
    done_ = true;
  }

 protected:
  DeviceCloudPolicyManagerChromeOSEnrollmentTest()
      : register_status_(DM_STATUS_SUCCESS),
        fetch_status_(DM_STATUS_SUCCESS),
        store_result_(true),
        expected_enrollment_status_(EnrollmentStatus::STATUS_SUCCESS),
        expected_dm_status_(DM_STATUS_SUCCESS),
        expected_validation_status_(CloudPolicyValidatorBase::VALIDATION_OK),
        expected_store_status_(CloudPolicyStore::STATUS_OK),
        done_(false) {}

  virtual void SetUp() OVERRIDE {
    DeviceCloudPolicyManagerChromeOSTest::SetUp();

    // Set up test data.
    device_policy_.set_new_signing_key(
        PolicyBuilder::CreateTestNewSigningKey());
    device_policy_.policy_data().set_timestamp(
        (base::Time::NowFromSystemTime() -
         base::Time::UnixEpoch()).InMilliseconds());
    device_policy_.Build();

    register_response_.mutable_register_response()->set_device_management_token(
        PolicyBuilder::kFakeToken);
    fetch_response_.mutable_policy_response()->add_response()->CopyFrom(
        device_policy_.policy());
    loaded_blob_ = device_policy_.GetBlob();

    // Initialize the manager.
    FlushDeviceSettings();
    EXPECT_EQ(CloudPolicyStore::STATUS_BAD_STATE, store_->status());
    EXPECT_TRUE(manager_.IsInitializationComplete());

    PolicyBundle bundle;
    EXPECT_TRUE(manager_.policies().Equals(bundle));

    manager_.Connect(&local_state_, &device_management_service_,
                     scoped_ptr<CloudPolicyClient::StatusProvider>(NULL));
  }

  virtual void TearDown() OVERRIDE {
    RunTest();

    // Enrollment should have completed.
    EXPECT_TRUE(done_);
    PolicyBundle bundle;
    if (expected_enrollment_status_ != EnrollmentStatus::STATUS_SUCCESS) {
      EXPECT_FALSE(store_->has_policy());
      EXPECT_FALSE(store_->is_managed());
      EXPECT_TRUE(manager_.policies().Equals(bundle));
    } else {
      EXPECT_EQ(DEVICE_MODE_ENTERPRISE, install_attributes_.GetMode());
      EXPECT_TRUE(store_->has_policy());
      EXPECT_TRUE(store_->is_managed());
      ASSERT_TRUE(manager_.core()->client());
      EXPECT_TRUE(manager_.core()->client()->is_registered());

      bundle.Get(POLICY_DOMAIN_CHROME, std::string()).Set(
          key::kDeviceMetricsReportingEnabled, POLICY_LEVEL_MANDATORY,
          POLICY_SCOPE_MACHINE, Value::CreateBooleanValue(false));
      EXPECT_TRUE(manager_.policies().Equals(bundle));
    }

    DeviceCloudPolicyManagerChromeOSTest::TearDown();
  }

  void RunTest() {
    // Trigger enrollment.
    MockDeviceManagementJob* register_job = NULL;
    EXPECT_CALL(device_management_service_,
                CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION))
        .Times(AtMost(1))
        .WillOnce(device_management_service_.CreateAsyncJob(&register_job));
    EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
        .Times(AtMost(1));
    DeviceCloudPolicyManagerChromeOS::AllowedDeviceModes modes;
    modes[DEVICE_MODE_ENTERPRISE] = true;
    manager_.StartEnrollment(
        "auth token", modes,
        base::Bind(&DeviceCloudPolicyManagerChromeOSEnrollmentTest::Done,
                   base::Unretained(this)));
    Mock::VerifyAndClearExpectations(&device_management_service_);

    if (done_)
      return;

    // Process registration.
    ASSERT_TRUE(register_job);
    MockDeviceManagementJob* fetch_job = NULL;
    EXPECT_CALL(device_management_service_,
                CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH))
        .Times(AtMost(1))
        .WillOnce(device_management_service_.CreateAsyncJob(&fetch_job));
    EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
        .Times(AtMost(1));
    register_job->SendResponse(register_status_, register_response_);
    Mock::VerifyAndClearExpectations(&device_management_service_);

    if (done_)
      return;

    // Process policy fetch.
    ASSERT_TRUE(fetch_job);
    fetch_job->SendResponse(fetch_status_, fetch_response_);

    if (done_)
      return;

    // Process verification.
    base::RunLoop().RunUntilIdle();

    if (done_)
      return;

    // Process policy store.
    device_settings_test_helper_.set_store_result(store_result_);
    device_settings_test_helper_.FlushStore();
    EXPECT_EQ(device_policy_.GetBlob(),
              device_settings_test_helper_.policy_blob());

    if (done_)
      return;

    // Key installation and policy load.
    device_settings_test_helper_.set_policy_blob(loaded_blob_);
    owner_key_util_->SetPublicKeyFromPrivateKey(
        device_policy_.new_signing_key());
    ReloadDeviceSettings();
  }

  DeviceManagementStatus register_status_;
  em::DeviceManagementResponse register_response_;

  DeviceManagementStatus fetch_status_;
  em::DeviceManagementResponse fetch_response_;

  bool store_result_;
  std::string loaded_blob_;

  EnrollmentStatus::Status expected_enrollment_status_;
  DeviceManagementStatus expected_dm_status_;
  CloudPolicyValidatorBase::Status expected_validation_status_;
  CloudPolicyStore::Status expected_store_status_;
  bool done_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyManagerChromeOSEnrollmentTest);
};

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, Success) {
  // The defaults should result in successful enrollment.
}

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, Reenrollment) {
  ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(PolicyBuilder::kFakeUsername,
                                           DEVICE_MODE_ENTERPRISE,
                                           PolicyBuilder::kFakeDeviceId));
}

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, RegistrationFailed) {
  register_status_ = DM_STATUS_REQUEST_FAILED;
  expected_enrollment_status_ = EnrollmentStatus::STATUS_REGISTRATION_FAILED;
  expected_dm_status_ = DM_STATUS_REQUEST_FAILED;
}

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, PolicyFetchFailed) {
  fetch_status_ = DM_STATUS_REQUEST_FAILED;
  expected_enrollment_status_ = EnrollmentStatus::STATUS_POLICY_FETCH_FAILED;
  expected_dm_status_ = DM_STATUS_REQUEST_FAILED;
}

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, ValidationFailed) {
  device_policy_.policy().set_policy_data_signature("bad");
  fetch_response_.clear_policy_response();
  fetch_response_.mutable_policy_response()->add_response()->CopyFrom(
      device_policy_.policy());
  expected_enrollment_status_ = EnrollmentStatus::STATUS_VALIDATION_FAILED;
  expected_validation_status_ =
      CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE;
}

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, StoreError) {
  store_result_ = false;
  expected_enrollment_status_ = EnrollmentStatus::STATUS_STORE_ERROR;
  expected_store_status_ = CloudPolicyStore::STATUS_STORE_ERROR;
}

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, LoadError) {
  loaded_blob_.clear();
  expected_enrollment_status_ = EnrollmentStatus::STATUS_STORE_ERROR;
  expected_store_status_ = CloudPolicyStore::STATUS_LOAD_ERROR;
}

}  // namespace
}  // namespace policy
