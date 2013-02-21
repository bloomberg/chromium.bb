// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_cloud_policy_manager_chromeos.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
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
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyNumber;
using testing::AtMost;
using testing::DoAll;
using testing::Mock;
using testing::SaveArg;
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
    chrome::RegisterLocalState(local_state_.registry());
    manager_.Init();
  }

  virtual void TearDown() OVERRIDE {
    manager_.Shutdown();
    DeviceSettingsTestBase::TearDown();
  }

  scoped_ptr<chromeos::CryptohomeLibrary> cryptohome_library_;
  EnterpriseInstallAttributes install_attributes_;

  TestingPrefServiceSimple local_state_;
  MockDeviceManagementService device_management_service_;

  DeviceCloudPolicyStoreChromeOS* store_;
  DeviceCloudPolicyManagerChromeOS manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyManagerChromeOSTest);
};

TEST_F(DeviceCloudPolicyManagerChromeOSTest, FreshDevice) {
  owner_key_util_->Clear();
  FlushDeviceSettings();
  EXPECT_TRUE(manager_.IsInitializationComplete(POLICY_DOMAIN_CHROME));

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
  EXPECT_TRUE(manager_.IsInitializationComplete(POLICY_DOMAIN_CHROME));

  PolicyBundle bundle;
  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(key::kDeviceMetricsReportingEnabled,
           POLICY_LEVEL_MANDATORY,
           POLICY_SCOPE_MACHINE,
           Value::CreateBooleanValue(false));
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
  EXPECT_TRUE(manager_.IsInitializationComplete(POLICY_DOMAIN_CHROME));

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
    status_ = status;
    done_ = true;
  }

 protected:
  DeviceCloudPolicyManagerChromeOSEnrollmentTest()
      : is_auto_enrollment_(false),
        register_status_(DM_STATUS_SUCCESS),
        fetch_status_(DM_STATUS_SUCCESS),
        store_result_(true),
        status_(EnrollmentStatus::ForStatus(EnrollmentStatus::STATUS_SUCCESS)),
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
    EXPECT_TRUE(manager_.IsInitializationComplete(POLICY_DOMAIN_CHROME));

    PolicyBundle bundle;
    EXPECT_TRUE(manager_.policies().Equals(bundle));

    manager_.Connect(&local_state_, &device_management_service_,
                     scoped_ptr<CloudPolicyClient::StatusProvider>(NULL));
  }

  void ExpectFailedEnrollment(EnrollmentStatus::Status status) {
    EXPECT_EQ(status, status_.status());
    EXPECT_FALSE(store_->is_managed());
    PolicyBundle empty_bundle;
    EXPECT_TRUE(manager_.policies().Equals(empty_bundle));
  }

  void ExpectSuccessfulEnrollment() {
    EXPECT_EQ(EnrollmentStatus::STATUS_SUCCESS, status_.status());
    EXPECT_EQ(DEVICE_MODE_ENTERPRISE, install_attributes_.GetMode());
    EXPECT_TRUE(store_->has_policy());
    EXPECT_TRUE(store_->is_managed());
    ASSERT_TRUE(manager_.core()->client());
    EXPECT_TRUE(manager_.core()->client()->is_registered());

    PolicyBundle bundle;
    bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
        .Set(key::kDeviceMetricsReportingEnabled,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_MACHINE,
             Value::CreateBooleanValue(false));
    EXPECT_TRUE(manager_.policies().Equals(bundle));
  }

  void RunTest() {
    // Trigger enrollment.
    MockDeviceManagementJob* register_job = NULL;
    EXPECT_CALL(device_management_service_,
                CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION))
        .Times(AtMost(1))
        .WillOnce(device_management_service_.CreateAsyncJob(&register_job));
    EXPECT_CALL(device_management_service_, StartJob(_, _, _, _, _, _, _))
        .Times(AtMost(1))
        .WillOnce(DoAll(SaveArg<5>(&client_id_),
                        SaveArg<6>(&register_request_)));
    DeviceCloudPolicyManagerChromeOS::AllowedDeviceModes modes;
    modes[DEVICE_MODE_ENTERPRISE] = true;
    manager_.StartEnrollment(
        "auth token", is_auto_enrollment_, modes,
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

  bool is_auto_enrollment_;

  DeviceManagementStatus register_status_;
  em::DeviceManagementResponse register_response_;

  DeviceManagementStatus fetch_status_;
  em::DeviceManagementResponse fetch_response_;

  bool store_result_;
  std::string loaded_blob_;

  em::DeviceManagementRequest register_request_;
  std::string client_id_;
  EnrollmentStatus status_;

  bool done_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyManagerChromeOSEnrollmentTest);
};

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, Success) {
  RunTest();
  ExpectSuccessfulEnrollment();
}

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, AutoEnrollment) {
  is_auto_enrollment_ = true;
  RunTest();
  ExpectSuccessfulEnrollment();
  EXPECT_TRUE(register_request_.register_request().auto_enrolled());
}

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, Reenrollment) {
  ASSERT_EQ(EnterpriseInstallAttributes::LOCK_SUCCESS,
            install_attributes_.LockDevice(PolicyBuilder::kFakeUsername,
                                           DEVICE_MODE_ENTERPRISE,
                                           PolicyBuilder::kFakeDeviceId));

  RunTest();
  ExpectSuccessfulEnrollment();
  EXPECT_TRUE(register_request_.register_request().reregister());
  EXPECT_EQ(PolicyBuilder::kFakeDeviceId, client_id_);
}

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, RegistrationFailed) {
  register_status_ = DM_STATUS_REQUEST_FAILED;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::STATUS_REGISTRATION_FAILED);
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, status_.client_status());
}

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, PolicyFetchFailed) {
  fetch_status_ = DM_STATUS_REQUEST_FAILED;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::STATUS_POLICY_FETCH_FAILED);
  EXPECT_EQ(DM_STATUS_REQUEST_FAILED, status_.client_status());
}

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, ValidationFailed) {
  device_policy_.policy().set_policy_data_signature("bad");
  fetch_response_.clear_policy_response();
  fetch_response_.mutable_policy_response()->add_response()->CopyFrom(
      device_policy_.policy());
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::STATUS_VALIDATION_FAILED);
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE,
            status_.validation_status());
}

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, StoreError) {
  store_result_ = false;
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::STATUS_STORE_ERROR);
  EXPECT_EQ(CloudPolicyStore::STATUS_STORE_ERROR,
            status_.store_status());
}

TEST_F(DeviceCloudPolicyManagerChromeOSEnrollmentTest, LoadError) {
  loaded_blob_.clear();
  RunTest();
  ExpectFailedEnrollment(EnrollmentStatus::STATUS_STORE_ERROR);
  EXPECT_EQ(CloudPolicyStore::STATUS_LOAD_ERROR,
            status_.store_status());
}

}  // namespace
}  // namespace policy
