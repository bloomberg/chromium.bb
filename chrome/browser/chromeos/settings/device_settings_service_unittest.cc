// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_service.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using ::testing::Mock;

namespace chromeos {

namespace {

class MockDeviceSettingsObserver : public DeviceSettingsService::Observer {
 public:
  virtual ~MockDeviceSettingsObserver() {}

  MOCK_METHOD0(OwnershipStatusChanged, void());
  MOCK_METHOD0(DeviceSettingsUpdated, void());
};

}  // namespace

class DeviceSettingsServiceTest : public DeviceSettingsTestBase {
 public:
  void SetOperationCompleted() {
    operation_completed_ = true;
  }

  void SetOwnershipStatus(
      DeviceSettingsService::OwnershipStatus ownership_status,
      bool is_owner) {
    is_owner_ = is_owner;
    ownership_status_ = ownership_status;
  }

 protected:
  DeviceSettingsServiceTest()
      : operation_completed_(false),
        is_owner_(true),
        ownership_status_(DeviceSettingsService::OWNERSHIP_UNKNOWN) {}

  virtual void SetUp() OVERRIDE {
    device_policy_.payload().mutable_device_policy_refresh_rate()->
        set_device_policy_refresh_rate(120);
    DeviceSettingsTestBase::SetUp();
  }

  void CheckPolicy() {
    ASSERT_TRUE(device_settings_service_.policy_data());
    EXPECT_EQ(device_policy_.policy_data().SerializeAsString(),
              device_settings_service_.policy_data()->SerializeAsString());
    ASSERT_TRUE(device_settings_service_.device_settings());
    EXPECT_EQ(device_policy_.payload().SerializeAsString(),
              device_settings_service_.device_settings()->SerializeAsString());
  }

  bool operation_completed_;
  bool is_owner_;
  DeviceSettingsService::OwnershipStatus ownership_status_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsServiceTest);
};

TEST_F(DeviceSettingsServiceTest, LoadNoKey) {
  owner_key_util_->Clear();
  ReloadDeviceSettings();

  EXPECT_EQ(DeviceSettingsService::STORE_KEY_UNAVAILABLE,
            device_settings_service_.status());
  EXPECT_FALSE(device_settings_service_.policy_data());
  EXPECT_FALSE(device_settings_service_.device_settings());
}

TEST_F(DeviceSettingsServiceTest, LoadNoPolicy) {
  device_settings_test_helper_.set_policy_blob(std::string());
  ReloadDeviceSettings();

  EXPECT_EQ(DeviceSettingsService::STORE_NO_POLICY,
            device_settings_service_.status());
  EXPECT_FALSE(device_settings_service_.policy_data());
  EXPECT_FALSE(device_settings_service_.device_settings());
}

TEST_F(DeviceSettingsServiceTest, LoadValidationError) {
  device_policy_.policy().set_policy_data_signature("bad");
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  owner_key_util_->SetPublicKeyFromPrivateKey(device_policy_.signing_key());
  ReloadDeviceSettings();

  EXPECT_EQ(DeviceSettingsService::STORE_VALIDATION_ERROR,
            device_settings_service_.status());
  EXPECT_FALSE(device_settings_service_.policy_data());
  EXPECT_FALSE(device_settings_service_.device_settings());
}

TEST_F(DeviceSettingsServiceTest, LoadValidationErrorFutureTimestamp) {
  base::Time timestamp(base::Time::NowFromSystemTime() +
                       base::TimeDelta::FromDays(5000));
  device_policy_.policy_data().set_timestamp(
      (timestamp - base::Time::UnixEpoch()).InMilliseconds());
  device_policy_.Build();
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  owner_key_util_->SetPublicKeyFromPrivateKey(device_policy_.signing_key());
  ReloadDeviceSettings();

  EXPECT_EQ(DeviceSettingsService::STORE_TEMP_VALIDATION_ERROR,
            device_settings_service_.status());
  EXPECT_FALSE(device_settings_service_.policy_data());
  EXPECT_FALSE(device_settings_service_.device_settings());
}

TEST_F(DeviceSettingsServiceTest, LoadSuccess) {
  ReloadDeviceSettings();

  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());
  CheckPolicy();
}

TEST_F(DeviceSettingsServiceTest, SignAndStoreNoKey) {
  ReloadDeviceSettings();
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());

  scoped_ptr<em::ChromeDeviceSettingsProto> new_device_settings(
      new em::ChromeDeviceSettingsProto(device_policy_.payload()));
  new_device_settings->mutable_device_policy_refresh_rate()->
      set_device_policy_refresh_rate(300);
  device_settings_service_.SignAndStore(
      new_device_settings.Pass(),
      base::Bind(&DeviceSettingsServiceTest::SetOperationCompleted,
                 base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_TRUE(operation_completed_);
  EXPECT_EQ(DeviceSettingsService::STORE_KEY_UNAVAILABLE,
            device_settings_service_.status());
  CheckPolicy();
}

TEST_F(DeviceSettingsServiceTest, SignAndStoreFailure) {
  ReloadDeviceSettings();
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());

  owner_key_util_->SetPrivateKey(device_policy_.signing_key());
  device_settings_service_.SetUsername(device_policy_.policy_data().username());
  FlushDeviceSettings();

  scoped_ptr<em::ChromeDeviceSettingsProto> new_device_settings(
      new em::ChromeDeviceSettingsProto(device_policy_.payload()));
  new_device_settings->mutable_device_policy_refresh_rate()->
      set_device_policy_refresh_rate(300);
  device_settings_test_helper_.set_store_result(false);
  device_settings_service_.SignAndStore(
      new_device_settings.Pass(),
      base::Bind(&DeviceSettingsServiceTest::SetOperationCompleted,
                 base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_TRUE(operation_completed_);
  EXPECT_EQ(DeviceSettingsService::STORE_OPERATION_FAILED,
            device_settings_service_.status());
  CheckPolicy();
}

TEST_F(DeviceSettingsServiceTest, SignAndStoreSuccess) {
  ReloadDeviceSettings();
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());

  owner_key_util_->SetPrivateKey(device_policy_.signing_key());
  device_settings_service_.SetUsername(device_policy_.policy_data().username());
  FlushDeviceSettings();

  device_policy_.payload().mutable_device_policy_refresh_rate()->
      set_device_policy_refresh_rate(300);
  device_policy_.Build();
  device_settings_service_.SignAndStore(
      scoped_ptr<em::ChromeDeviceSettingsProto>(
          new em::ChromeDeviceSettingsProto(device_policy_.payload())),
      base::Bind(&DeviceSettingsServiceTest::SetOperationCompleted,
                 base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_TRUE(operation_completed_);
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());
  ASSERT_TRUE(device_settings_service_.device_settings());
  EXPECT_EQ(device_policy_.payload().SerializeAsString(),
            device_settings_service_.device_settings()->SerializeAsString());
}

TEST_F(DeviceSettingsServiceTest, StoreFailure) {
  owner_key_util_->Clear();
  device_settings_test_helper_.set_policy_blob(std::string());
  ReloadDeviceSettings();
  EXPECT_EQ(DeviceSettingsService::STORE_KEY_UNAVAILABLE,
            device_settings_service_.status());

  device_settings_test_helper_.set_store_result(false);
  device_settings_service_.Store(
      device_policy_.GetCopy(),
      base::Bind(&DeviceSettingsServiceTest::SetOperationCompleted,
                 base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_TRUE(operation_completed_);
  EXPECT_EQ(DeviceSettingsService::STORE_OPERATION_FAILED,
            device_settings_service_.status());
}

TEST_F(DeviceSettingsServiceTest, StoreSuccess) {
  owner_key_util_->Clear();
  device_settings_test_helper_.set_policy_blob(std::string());
  ReloadDeviceSettings();
  EXPECT_EQ(DeviceSettingsService::STORE_KEY_UNAVAILABLE,
            device_settings_service_.status());

  owner_key_util_->SetPublicKeyFromPrivateKey(device_policy_.signing_key());
  device_settings_service_.Store(
      device_policy_.GetCopy(),
      base::Bind(&DeviceSettingsServiceTest::SetOperationCompleted,
                 base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_TRUE(operation_completed_);
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());
  CheckPolicy();
}

TEST_F(DeviceSettingsServiceTest, StoreRotation) {
  ReloadDeviceSettings();
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());

  device_policy_.payload().mutable_device_policy_refresh_rate()->
      set_device_policy_refresh_rate(300);
  device_policy_.set_new_signing_key(
      policy::PolicyBuilder::CreateTestNewSigningKey());
  device_policy_.Build();
  device_settings_service_.Store(device_policy_.GetCopy(), base::Closure());
  FlushDeviceSettings();
  owner_key_util_->SetPublicKeyFromPrivateKey(device_policy_.new_signing_key());
  device_settings_service_.OwnerKeySet(true);
  FlushDeviceSettings();
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());
  CheckPolicy();

  // Check the new key has been loaded.
  std::vector<uint8> key;
  ASSERT_TRUE(device_policy_.new_signing_key()->ExportPublicKey(&key));
  EXPECT_EQ(*device_settings_service_.GetOwnerKey()->public_key(), key);
}

TEST_F(DeviceSettingsServiceTest, OwnershipStatus) {
  owner_key_util_->Clear();

  EXPECT_FALSE(device_settings_service_.HasPrivateOwnerKey());
  EXPECT_FALSE(device_settings_service_.GetOwnerKey());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_UNKNOWN,
            device_settings_service_.GetOwnershipStatus());

  device_settings_service_.GetOwnershipStatusAsync(
      base::Bind(&DeviceSettingsServiceTest::SetOwnershipStatus,
                 base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_FALSE(device_settings_service_.HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_.GetOwnerKey());
  EXPECT_FALSE(device_settings_service_.GetOwnerKey()->public_key());
  EXPECT_FALSE(device_settings_service_.GetOwnerKey()->private_key());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_NONE,
            device_settings_service_.GetOwnershipStatus());
  EXPECT_FALSE(is_owner_);
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_NONE, ownership_status_);

  owner_key_util_->SetPublicKeyFromPrivateKey(device_policy_.signing_key());
  ReloadDeviceSettings();
  device_settings_service_.GetOwnershipStatusAsync(
      base::Bind(&DeviceSettingsServiceTest::SetOwnershipStatus,
                 base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_FALSE(device_settings_service_.HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_.GetOwnerKey());
  ASSERT_TRUE(device_settings_service_.GetOwnerKey()->public_key());
  std::vector<uint8> key;
  ASSERT_TRUE(device_policy_.signing_key()->ExportPublicKey(&key));
  EXPECT_EQ(*device_settings_service_.GetOwnerKey()->public_key(), key);
  EXPECT_FALSE(device_settings_service_.GetOwnerKey()->private_key());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            device_settings_service_.GetOwnershipStatus());
  EXPECT_FALSE(is_owner_);
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN, ownership_status_);

  owner_key_util_->SetPrivateKey(device_policy_.signing_key());
  device_settings_service_.SetUsername(device_policy_.policy_data().username());
  device_settings_service_.GetOwnershipStatusAsync(
      base::Bind(&DeviceSettingsServiceTest::SetOwnershipStatus,
                 base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_TRUE(device_settings_service_.HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_.GetOwnerKey());
  ASSERT_TRUE(device_settings_service_.GetOwnerKey()->public_key());
  ASSERT_TRUE(device_policy_.signing_key()->ExportPublicKey(&key));
  EXPECT_EQ(*device_settings_service_.GetOwnerKey()->public_key(), key);
  EXPECT_TRUE(device_settings_service_.GetOwnerKey()->private_key());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            device_settings_service_.GetOwnershipStatus());
  EXPECT_TRUE(is_owner_);
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN, ownership_status_);
}

TEST_F(DeviceSettingsServiceTest, Observer) {
  owner_key_util_->Clear();
  MockDeviceSettingsObserver observer_;
  device_settings_service_.AddObserver(&observer_);

  EXPECT_CALL(observer_, OwnershipStatusChanged()).Times(1);
  EXPECT_CALL(observer_, DeviceSettingsUpdated()).Times(1);
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OwnershipStatusChanged()).Times(1);
  EXPECT_CALL(observer_, DeviceSettingsUpdated()).Times(1);
  owner_key_util_->SetPublicKeyFromPrivateKey(device_policy_.signing_key());
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OwnershipStatusChanged()).Times(0);
  EXPECT_CALL(observer_, DeviceSettingsUpdated()).Times(1);
  device_settings_service_.Store(device_policy_.GetCopy(), base::Closure());
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OwnershipStatusChanged()).Times(0);
  EXPECT_CALL(observer_, DeviceSettingsUpdated()).Times(1);
  device_settings_service_.PropertyChangeComplete(true);
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  device_settings_service_.RemoveObserver(&observer_);
}

}  // namespace chromeos
