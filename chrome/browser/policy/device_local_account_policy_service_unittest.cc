// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_local_account_policy_service.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/policy/mock_device_management_service.h"
#include "chrome/browser/policy/policy_builder.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;

namespace policy {

class MockDeviceLocalAccountPolicyServiceObserver
    : public DeviceLocalAccountPolicyService::Observer {
 public:
  MOCK_METHOD1(OnPolicyChanged, void(const std::string&));
  MOCK_METHOD0(OnDeviceLocalAccountsChanged, void(void));
};

class DeviceLocalAccountPolicyServiceTest
    : public chromeos::DeviceSettingsTestBase {
 protected:
  DeviceLocalAccountPolicyServiceTest()
      : service_(&device_settings_test_helper_, &device_settings_service_) {}

  virtual void SetUp() OVERRIDE {
    DeviceSettingsTestBase::SetUp();

    device_local_account_policy_.payload().mutable_disablespdy()->set_value(
        true);
    device_local_account_policy_.policy_data().set_policy_type(
        dm_protocol::kChromePublicAccountPolicyType);
    device_local_account_policy_.Build();

    device_policy_.payload().mutable_device_local_accounts()->add_account()->
        set_id(PolicyBuilder::kFakeUsername);
    device_policy_.Build();

    service_.AddObserver(&observer_);
  }

  virtual void TearDown() OVERRIDE {
    service_.RemoveObserver(&observer_);

    DeviceSettingsTestBase::TearDown();
  }

  void InstallDevicePolicy() {
    EXPECT_CALL(observer_, OnDeviceLocalAccountsChanged());
    device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
    ReloadDeviceSettings();
    Mock::VerifyAndClearExpectations(&observer_);
  }

  UserPolicyBuilder device_local_account_policy_;
  MockDeviceLocalAccountPolicyServiceObserver observer_;
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
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, broker->status());
  EXPECT_FALSE(broker->policy_data());
  EXPECT_FALSE(broker->policy_settings());
}

TEST_F(DeviceLocalAccountPolicyServiceTest, LoadNoPolicy) {
  InstallDevicePolicy();

  EXPECT_CALL(observer_, OnPolicyChanged(PolicyBuilder::kFakeUsername));
  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);
  broker->Load();
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_EQ(CloudPolicyStore::STATUS_LOAD_ERROR, broker->status());
  EXPECT_FALSE(broker->policy_data());
  EXPECT_FALSE(broker->policy_settings());
}

TEST_F(DeviceLocalAccountPolicyServiceTest, LoadValidationFailure) {
  device_local_account_policy_.policy_data().set_policy_type(
      dm_protocol::kChromeUserPolicyType);
  device_local_account_policy_.Build();
  device_settings_test_helper_.set_device_local_account_policy_blob(
      PolicyBuilder::kFakeUsername, device_local_account_policy_.GetBlob());
  InstallDevicePolicy();

  EXPECT_CALL(observer_, OnPolicyChanged(PolicyBuilder::kFakeUsername));
  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);
  broker->Load();
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, broker->status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_WRONG_POLICY_TYPE,
            broker->validation_status());
  EXPECT_FALSE(broker->policy_data());
  EXPECT_FALSE(broker->policy_settings());
}

TEST_F(DeviceLocalAccountPolicyServiceTest, LoadPolicy) {
  device_settings_test_helper_.set_device_local_account_policy_blob(
      PolicyBuilder::kFakeUsername, device_local_account_policy_.GetBlob());
  InstallDevicePolicy();

  EXPECT_CALL(observer_, OnPolicyChanged(PolicyBuilder::kFakeUsername));
  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);
  broker->Load();
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_EQ(CloudPolicyStore::STATUS_OK, broker->status());
  ASSERT_TRUE(broker->policy_data());
  EXPECT_EQ(device_local_account_policy_.policy_data().SerializeAsString(),
            broker->policy_data()->SerializeAsString());
  ASSERT_TRUE(broker->policy_settings());
  EXPECT_EQ(device_local_account_policy_.payload().SerializeAsString(),
            broker->policy_settings()->SerializeAsString());
}

TEST_F(DeviceLocalAccountPolicyServiceTest, StoreValidationFailure) {
  device_local_account_policy_.policy_data().set_policy_type(
      dm_protocol::kChromeUserPolicyType);
  device_local_account_policy_.Build();
  InstallDevicePolicy();

  EXPECT_CALL(observer_, OnPolicyChanged(PolicyBuilder::kFakeUsername));
  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);
  broker->Store(device_local_account_policy_.policy());
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, broker->status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_WRONG_POLICY_TYPE,
            broker->validation_status());
}

TEST_F(DeviceLocalAccountPolicyServiceTest, StorePolicy) {
  InstallDevicePolicy();

  EXPECT_CALL(observer_, OnPolicyChanged(PolicyBuilder::kFakeUsername));
  DeviceLocalAccountPolicyBroker* broker =
      service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername);
  ASSERT_TRUE(broker);
  broker->Store(device_local_account_policy_.policy());
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_EQ(device_local_account_policy_.GetBlob(),
            device_settings_test_helper_.device_local_account_policy_blob(
                PolicyBuilder::kFakeUsername));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, DevicePolicyChange) {
  device_settings_test_helper_.set_device_local_account_policy_blob(
      PolicyBuilder::kFakeUsername, device_local_account_policy_.GetBlob());
  InstallDevicePolicy();

  EXPECT_CALL(observer_, OnDeviceLocalAccountsChanged());
  device_policy_.payload().mutable_device_local_accounts()->clear_account();
  device_policy_.Build();
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  device_settings_service_.PropertyChangeComplete(true);
  FlushDeviceSettings();
  EXPECT_FALSE(service_.GetBrokerForAccount(PolicyBuilder::kFakeUsername));
  Mock::VerifyAndClearExpectations(&observer_);
}

}  // namespace policy
