// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_local_account_policy_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/policy/cloud_policy_client.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/device_local_account_policy_provider.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/mock_device_management_service.h"
#include "chrome/browser/policy/policy_builder.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyNumber;
using testing::AtLeast;
using testing::Mock;
using testing::SaveArg;
using testing::_;

namespace em = enterprise_management;

namespace policy {

class MockDeviceLocalAccountPolicyServiceObserver
    : public DeviceLocalAccountPolicyService::Observer {
 public:
  MOCK_METHOD1(OnPolicyUpdated, void(const std::string&));
  MOCK_METHOD0(OnDeviceLocalAccountsChanged, void(void));
};

class DeviceLocalAccountPolicyServiceTest
    : public chromeos::DeviceSettingsTestBase {
 public:
  DeviceLocalAccountPolicyServiceTest()
      : service_(&device_settings_test_helper_, &device_settings_service_) {}

  virtual void SetUp() OVERRIDE {
    DeviceSettingsTestBase::SetUp();

    expected_policy_map_.Set(key::kDisableSpdy, POLICY_LEVEL_MANDATORY,
                             POLICY_SCOPE_USER,
                             Value::CreateBooleanValue(true));

    device_local_account_policy_.payload().mutable_disablespdy()->set_value(
        true);
    device_local_account_policy_.policy_data().set_policy_type(
        dm_protocol::kChromePublicAccountPolicyType);
    device_local_account_policy_.Build();

    device_policy_.payload().mutable_device_local_accounts()->add_account()->
        set_id(PolicyBuilder::kFakeUsername);
    device_policy_.Build();

    service_.AddObserver(&service_observer_);
  }

  virtual void TearDown() OVERRIDE {
    service_.RemoveObserver(&service_observer_);

    DeviceSettingsTestBase::TearDown();
  }

  void InstallDevicePolicy() {
    EXPECT_CALL(service_observer_, OnDeviceLocalAccountsChanged());
    device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
    ReloadDeviceSettings();
    Mock::VerifyAndClearExpectations(&service_observer_);
  }

  MOCK_METHOD0(OnRefreshDone, void(void));

  PolicyMap expected_policy_map_;
  UserPolicyBuilder device_local_account_policy_;
  MockDeviceLocalAccountPolicyServiceObserver service_observer_;
  MockDeviceManagementService mock_device_management_service_;
  DeviceLocalAccountPolicyService service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyServiceTest);
};

TEST_F(DeviceLocalAccountPolicyServiceTest, NoAccounts) {
  EXPECT_FALSE(service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, GetBroker) {
  InstallDevicePolicy();

  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);
  EXPECT_EQ(PolicyBuilder::kFakeUsername, broker->account_id());
  ASSERT_TRUE(broker->store());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, broker->store()->status());
  EXPECT_FALSE(broker->client());
  EXPECT_TRUE(broker->store()->policy_map().empty());
}

TEST_F(DeviceLocalAccountPolicyServiceTest, LoadNoPolicy) {
  InstallDevicePolicy();

  EXPECT_CALL(service_observer_, OnPolicyUpdated(PolicyBuilder::kFakeUsername));
  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&service_observer_);

  ASSERT_TRUE(broker->store());
  EXPECT_EQ(CloudPolicyStore::STATUS_LOAD_ERROR, broker->store()->status());
  EXPECT_TRUE(broker->store()->policy_map().empty());
}

TEST_F(DeviceLocalAccountPolicyServiceTest, LoadValidationFailure) {
  device_local_account_policy_.policy_data().set_policy_type(
      dm_protocol::kChromeUserPolicyType);
  device_local_account_policy_.Build();
  device_settings_test_helper_.set_device_local_account_policy_blob(
      PolicyBuilder::kFakeUsername, device_local_account_policy_.GetBlob());
  InstallDevicePolicy();

  EXPECT_CALL(service_observer_, OnPolicyUpdated(PolicyBuilder::kFakeUsername));
  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&service_observer_);

  ASSERT_TRUE(broker->store());
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR,
            broker->store()->status());
  EXPECT_TRUE(broker->store()->policy_map().empty());
}

TEST_F(DeviceLocalAccountPolicyServiceTest, LoadPolicy) {
  device_settings_test_helper_.set_device_local_account_policy_blob(
      PolicyBuilder::kFakeUsername, device_local_account_policy_.GetBlob());
  InstallDevicePolicy();

  EXPECT_CALL(service_observer_, OnPolicyUpdated(PolicyBuilder::kFakeUsername));
  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&service_observer_);

  ASSERT_TRUE(broker->store());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, broker->store()->status());
  ASSERT_TRUE(broker->store()->policy());
  EXPECT_EQ(device_local_account_policy_.policy_data().SerializeAsString(),
            broker->store()->policy()->SerializeAsString());
  EXPECT_TRUE(expected_policy_map_.Equals(broker->store()->policy_map()));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, StoreValidationFailure) {
  device_local_account_policy_.policy_data().set_policy_type(
      dm_protocol::kChromeUserPolicyType);
  device_local_account_policy_.Build();
  InstallDevicePolicy();

  EXPECT_CALL(service_observer_, OnPolicyUpdated(PolicyBuilder::kFakeUsername));
  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);
  ASSERT_TRUE(broker->store());
  broker->store()->Store(device_local_account_policy_.policy());
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&service_observer_);

  ASSERT_TRUE(broker->store());
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR,
            broker->store()->status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_WRONG_POLICY_TYPE,
            broker->store()->validation_status());
}

TEST_F(DeviceLocalAccountPolicyServiceTest, StorePolicy) {
  InstallDevicePolicy();

  EXPECT_CALL(service_observer_, OnPolicyUpdated(PolicyBuilder::kFakeUsername));
  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);
  ASSERT_TRUE(broker->store());
  broker->store()->Store(device_local_account_policy_.policy());
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&service_observer_);

  EXPECT_EQ(device_local_account_policy_.GetBlob(),
            device_settings_test_helper_.device_local_account_policy_blob(
                PolicyBuilder::kFakeUsername));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, DevicePolicyChange) {
  device_settings_test_helper_.set_device_local_account_policy_blob(
      PolicyBuilder::kFakeUsername, device_local_account_policy_.GetBlob());
  InstallDevicePolicy();

  EXPECT_CALL(service_observer_, OnDeviceLocalAccountsChanged());
  device_policy_.payload().mutable_device_local_accounts()->clear_account();
  device_policy_.Build();
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  device_settings_service_.PropertyChangeComplete(true);
  FlushDeviceSettings();
  EXPECT_FALSE(service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername));
  Mock::VerifyAndClearExpectations(&service_observer_);
}

TEST_F(DeviceLocalAccountPolicyServiceTest, FetchPolicy) {
  device_settings_test_helper_.set_device_local_account_policy_blob(
      PolicyBuilder::kFakeUsername, device_local_account_policy_.GetBlob());
  InstallDevicePolicy();

  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);

  service_.Connect(&mock_device_management_service_);
  EXPECT_TRUE(broker->client());

  em::DeviceManagementRequest request;
  em::DeviceManagementResponse response;
  response.mutable_policy_response()->add_response()->CopyFrom(
      device_local_account_policy_.policy());
  EXPECT_CALL(mock_device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH))
      .WillOnce(mock_device_management_service_.SucceedJob(response));
  EXPECT_CALL(mock_device_management_service_,
              StartJob(dm_protocol::kValueRequestPolicy,
                       std::string(), std::string(),
                       device_policy_.policy_data().request_token(),
                       dm_protocol::kValueUserAffiliationManaged,
                       device_policy_.policy_data().device_id(),
                       _))
      .WillOnce(SaveArg<6>(&request));
  EXPECT_CALL(service_observer_, OnPolicyUpdated(PolicyBuilder::kFakeUsername));
  broker->client()->FetchPolicy();
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&service_observer_);
  Mock::VerifyAndClearExpectations(&mock_device_management_service_);
  EXPECT_TRUE(request.has_policy_request());
  EXPECT_EQ(1, request.policy_request().request_size());
  EXPECT_EQ(dm_protocol::kChromePublicAccountPolicyType,
            request.policy_request().request(0).policy_type());
  EXPECT_FALSE(request.policy_request().request(0).has_machine_id());
  EXPECT_EQ(PolicyBuilder::kFakeUsername,
            request.policy_request().request(0).settings_entity_id());

  ASSERT_TRUE(broker->store());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, broker->store()->status());
  ASSERT_TRUE(broker->store()->policy());
  EXPECT_EQ(device_local_account_policy_.policy_data().SerializeAsString(),
            broker->store()->policy()->SerializeAsString());
  EXPECT_TRUE(expected_policy_map_.Equals(broker->store()->policy_map()));

  EXPECT_CALL(service_observer_,
              OnPolicyUpdated(PolicyBuilder::kFakeUsername)).Times(0);
  service_.Disconnect();
  EXPECT_FALSE(broker->client());
  Mock::VerifyAndClearExpectations(&service_observer_);
}

TEST_F(DeviceLocalAccountPolicyServiceTest, RefreshPolicy) {
  device_settings_test_helper_.set_device_local_account_policy_blob(
      PolicyBuilder::kFakeUsername, device_local_account_policy_.GetBlob());
  InstallDevicePolicy();

  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);

  service_.Connect(&mock_device_management_service_);

  em::DeviceManagementResponse response;
  response.mutable_policy_response()->add_response()->CopyFrom(
      device_local_account_policy_.policy());
  EXPECT_CALL(mock_device_management_service_, CreateJob(_))
      .WillOnce(mock_device_management_service_.SucceedJob(response));
  EXPECT_CALL(mock_device_management_service_, StartJob(_, _, _, _, _, _, _));
  EXPECT_CALL(*this, OnRefreshDone()).Times(1);
  EXPECT_CALL(service_observer_, OnPolicyUpdated(PolicyBuilder::kFakeUsername));
  broker->RefreshPolicy(
      base::Bind(&DeviceLocalAccountPolicyServiceTest::OnRefreshDone,
                 base::Unretained(this)));
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&service_observer_);
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_device_management_service_);

  ASSERT_TRUE(broker->store());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, broker->store()->status());
  EXPECT_TRUE(expected_policy_map_.Equals(broker->store()->policy_map()));
}

class DeviceLocalAccountPolicyProviderTest
    : public DeviceLocalAccountPolicyServiceTest {
 protected:
  DeviceLocalAccountPolicyProviderTest()
      : provider_(PolicyBuilder::kFakeUsername, &service_) {}

  virtual void SetUp() OVERRIDE {
    DeviceLocalAccountPolicyServiceTest::SetUp();
    provider_.Init();
    provider_.AddObserver(&provider_observer_);

    EXPECT_CALL(service_observer_, OnPolicyUpdated(_)).Times(AnyNumber());
    EXPECT_CALL(service_observer_, OnDeviceLocalAccountsChanged())
        .Times(AnyNumber());
  }

  virtual void TearDown() OVERRIDE {
    provider_.RemoveObserver(&provider_observer_);
    provider_.Shutdown();
    DeviceLocalAccountPolicyServiceTest::TearDown();
  }

  DeviceLocalAccountPolicyProvider provider_;
  MockConfigurationPolicyObserver provider_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyProviderTest);
};

TEST_F(DeviceLocalAccountPolicyProviderTest, Initialization) {
  EXPECT_FALSE(provider_.IsInitializationComplete());

  // Policy change should complete initialization.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(&provider_)).Times(AtLeast(1));
  device_settings_test_helper_.set_device_local_account_policy_blob(
      PolicyBuilder::kFakeUsername, device_local_account_policy_.GetBlob());
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(&provider_observer_);

  EXPECT_TRUE(provider_.IsInitializationComplete());

  // The account disappearing should *not* flip the initialization flag back.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(&provider_))
      .Times(AnyNumber());
  device_policy_.payload().mutable_device_local_accounts()->clear_account();
  device_policy_.Build();
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(&provider_observer_);

  EXPECT_TRUE(provider_.IsInitializationComplete());
}

TEST_F(DeviceLocalAccountPolicyProviderTest, Policy) {
  // Policy should load successfully.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(&provider_)).Times(AtLeast(1));
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  device_settings_test_helper_.set_device_local_account_policy_blob(
      PolicyBuilder::kFakeUsername, device_local_account_policy_.GetBlob());
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(&provider_observer_);

  PolicyBundle expected_policy_bundle;
  expected_policy_bundle.Get(POLICY_DOMAIN_CHROME, "").CopyFrom(
      expected_policy_map_);
  EXPECT_TRUE(expected_policy_bundle.Equals(provider_.policies()));

  // Policy change should be reported.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(&provider_)).Times(AtLeast(1));
  device_local_account_policy_.payload().mutable_disablespdy()->set_value(
      false);
  device_local_account_policy_.Build();
  device_settings_test_helper_.set_device_local_account_policy_blob(
      PolicyBuilder::kFakeUsername, device_local_account_policy_.GetBlob());
  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);
  broker->store()->Load();
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&provider_observer_);

  expected_policy_bundle.Get(POLICY_DOMAIN_CHROME, "").Set(
      key::kDisableSpdy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      Value::CreateBooleanValue(false));
  EXPECT_TRUE(expected_policy_bundle.Equals(provider_.policies()));

  // Account disappears, policy should stay in effect.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(&provider_))
      .Times(AnyNumber());
  device_policy_.payload().mutable_device_local_accounts()->clear_account();
  device_policy_.Build();
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(&provider_observer_);

  EXPECT_TRUE(expected_policy_bundle.Equals(provider_.policies()));
}

TEST_F(DeviceLocalAccountPolicyProviderTest, RefreshPolicies) {
  // If there's no device policy, the refresh completes immediately.
  EXPECT_FALSE(service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername));
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(&provider_)).Times(AtLeast(1));
  provider_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&provider_observer_);

  // Make device settings appear.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(&provider_))
      .Times(AnyNumber());
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(&provider_observer_);
  EXPECT_TRUE(service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername));

  // If there's no cloud connection, refreshes are still immediate.
  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);
  EXPECT_FALSE(broker->client());
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(&provider_)).Times(AtLeast(1));
  provider_.RefreshPolicies();
  Mock::VerifyAndClearExpectations(&provider_observer_);

  // Bring up the cloud connection. The refresh scheduler may fire refreshes at
  // this point which are not relevant for the test.
  EXPECT_CALL(mock_device_management_service_, CreateJob(_))
      .WillRepeatedly(
          mock_device_management_service_.FailJob(DM_STATUS_REQUEST_FAILED));
  service_.Connect(&mock_device_management_service_);
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&mock_device_management_service_);

  // No callbacks until the refresh completes.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(_)).Times(0);
  MockDeviceManagementJob* request_job;
  EXPECT_CALL(mock_device_management_service_, CreateJob(_))
      .WillOnce(mock_device_management_service_.CreateAsyncJob(&request_job));
  EXPECT_CALL(mock_device_management_service_, StartJob(_, _, _, _, _, _, _));
  provider_.RefreshPolicies();
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(&provider_observer_);
  Mock::VerifyAndClearExpectations(&mock_device_management_service_);
  EXPECT_TRUE(provider_.IsInitializationComplete());

  // When the response comes in, it should propagate and fire the notification.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(&provider_)).Times(AtLeast(1));
  ASSERT_TRUE(request_job);
  em::DeviceManagementResponse response;
  response.mutable_policy_response()->add_response()->CopyFrom(
      device_local_account_policy_.policy());
  request_job->SendResponse(DM_STATUS_SUCCESS, response);
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&provider_observer_);
}

}  // namespace policy
