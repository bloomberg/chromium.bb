// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_service.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/mock_owner_key_util.h"
#include "chrome/browser/policy/policy_builder.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "content/public/test/test_browser_thread.h"
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

class DeviceSettingsServiceTest : public testing::Test {
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
      : owner_key_util_(new chromeos::MockOwnerKeyUtil()),
        message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        operation_completed_(false),
        is_owner_(true),
        ownership_status_(DeviceSettingsService::OWNERSHIP_UNKNOWN) {}

  virtual void SetUp() OVERRIDE {
    policy_.payload().mutable_device_policy_refresh_rate()->
        set_device_policy_refresh_rate(120);
    policy_.Build();
    device_settings_test_helper_.set_policy_blob(policy_.GetBlob());

    device_settings_service_.Initialize(&device_settings_test_helper_,
                                        owner_key_util_);
  }

  virtual void TearDown() OVERRIDE {
    device_settings_test_helper_.Flush();
    device_settings_service_.Shutdown();
  }

  void CheckPolicy() {
    ASSERT_TRUE(device_settings_service_.policy_data());
    EXPECT_EQ(policy_.policy_data().SerializeAsString(),
              device_settings_service_.policy_data()->SerializeAsString());
    ASSERT_TRUE(device_settings_service_.device_settings());
    EXPECT_EQ(policy_.payload().SerializeAsString(),
              device_settings_service_.device_settings()->SerializeAsString());
  }

  policy::DevicePolicyBuilder policy_;
  scoped_refptr<MockOwnerKeyUtil> owner_key_util_;

  DeviceSettingsTestHelper device_settings_test_helper_;
  DeviceSettingsService device_settings_service_;

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  bool operation_completed_;
  bool is_owner_;
  DeviceSettingsService::OwnershipStatus ownership_status_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsServiceTest);
};

TEST_F(DeviceSettingsServiceTest, LoadNoKey) {
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();

  EXPECT_EQ(DeviceSettingsService::STORE_KEY_UNAVAILABLE,
            device_settings_service_.status());
  EXPECT_FALSE(device_settings_service_.policy_data());
  EXPECT_FALSE(device_settings_service_.device_settings());
}

TEST_F(DeviceSettingsServiceTest, LoadNoPolicy) {
  device_settings_test_helper_.set_policy_blob(std::string());
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();

  EXPECT_EQ(DeviceSettingsService::STORE_NO_POLICY,
            device_settings_service_.status());
  EXPECT_FALSE(device_settings_service_.policy_data());
  EXPECT_FALSE(device_settings_service_.device_settings());
}

TEST_F(DeviceSettingsServiceTest, LoadValidationError) {
  policy_.policy().set_policy_data_signature("bad");
  device_settings_test_helper_.set_policy_blob(policy_.GetBlob());
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();

  EXPECT_EQ(DeviceSettingsService::STORE_VALIDATION_ERROR,
            device_settings_service_.status());
  EXPECT_FALSE(device_settings_service_.policy_data());
  EXPECT_FALSE(device_settings_service_.device_settings());
}

TEST_F(DeviceSettingsServiceTest, LoadSuccess) {
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();

  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());
  CheckPolicy();
}

TEST_F(DeviceSettingsServiceTest, SignAndStoreNoKey) {
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());

  scoped_ptr<em::ChromeDeviceSettingsProto> new_device_settings(
      new em::ChromeDeviceSettingsProto(policy_.payload()));
  new_device_settings->mutable_device_policy_refresh_rate()->
      set_device_policy_refresh_rate(300);
  device_settings_service_.SignAndStore(
      new_device_settings.Pass(),
      base::Bind(&DeviceSettingsServiceTest::SetOperationCompleted,
                 base::Unretained(this)));
  device_settings_test_helper_.Flush();
  EXPECT_TRUE(operation_completed_);
  EXPECT_EQ(DeviceSettingsService::STORE_KEY_UNAVAILABLE,
            device_settings_service_.status());
  CheckPolicy();
}

TEST_F(DeviceSettingsServiceTest, SignAndStoreFailure) {
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());

  owner_key_util_->SetPrivateKey(policy_.signing_key());
  device_settings_service_.SetUsername(policy_.policy_data().username());
  device_settings_test_helper_.Flush();

  scoped_ptr<em::ChromeDeviceSettingsProto> new_device_settings(
      new em::ChromeDeviceSettingsProto(policy_.payload()));
  new_device_settings->mutable_device_policy_refresh_rate()->
      set_device_policy_refresh_rate(300);
  device_settings_test_helper_.set_store_result(false);
  device_settings_service_.SignAndStore(
      new_device_settings.Pass(),
      base::Bind(&DeviceSettingsServiceTest::SetOperationCompleted,
                 base::Unretained(this)));
  device_settings_test_helper_.Flush();
  EXPECT_TRUE(operation_completed_);
  EXPECT_EQ(DeviceSettingsService::STORE_OPERATION_FAILED,
            device_settings_service_.status());
  CheckPolicy();
}

TEST_F(DeviceSettingsServiceTest, SignAndStoreSuccess) {
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());

  owner_key_util_->SetPrivateKey(policy_.signing_key());
  device_settings_service_.SetUsername(policy_.policy_data().username());
  device_settings_test_helper_.Flush();

  policy_.payload().mutable_device_policy_refresh_rate()->
      set_device_policy_refresh_rate(300);
  policy_.Build();
  device_settings_service_.SignAndStore(
      scoped_ptr<em::ChromeDeviceSettingsProto>(
          new em::ChromeDeviceSettingsProto(policy_.payload())),
      base::Bind(&DeviceSettingsServiceTest::SetOperationCompleted,
                 base::Unretained(this)));
  device_settings_test_helper_.Flush();
  EXPECT_TRUE(operation_completed_);
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());
  ASSERT_TRUE(device_settings_service_.device_settings());
  EXPECT_EQ(policy_.payload().SerializeAsString(),
            device_settings_service_.device_settings()->SerializeAsString());
}

TEST_F(DeviceSettingsServiceTest, StoreFailure) {
  device_settings_test_helper_.set_policy_blob(std::string());
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();
  EXPECT_EQ(DeviceSettingsService::STORE_KEY_UNAVAILABLE,
            device_settings_service_.status());

  device_settings_test_helper_.set_store_result(false);
  device_settings_service_.Store(
      policy_.GetCopy(),
      base::Bind(&DeviceSettingsServiceTest::SetOperationCompleted,
                 base::Unretained(this)));
  device_settings_test_helper_.Flush();
  EXPECT_TRUE(operation_completed_);
  EXPECT_EQ(DeviceSettingsService::STORE_OPERATION_FAILED,
            device_settings_service_.status());
}

TEST_F(DeviceSettingsServiceTest, StoreSuccess) {
  device_settings_test_helper_.set_policy_blob(std::string());
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();
  EXPECT_EQ(DeviceSettingsService::STORE_KEY_UNAVAILABLE,
            device_settings_service_.status());

  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  device_settings_service_.Store(
      policy_.GetCopy(),
      base::Bind(&DeviceSettingsServiceTest::SetOperationCompleted,
                 base::Unretained(this)));
  device_settings_test_helper_.Flush();
  EXPECT_TRUE(operation_completed_);
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());
  CheckPolicy();
}

TEST_F(DeviceSettingsServiceTest, StoreRotation) {
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());

  policy_.payload().mutable_device_policy_refresh_rate()->
      set_device_policy_refresh_rate(300);
  policy_.set_new_signing_key(policy::PolicyBuilder::CreateTestNewSigningKey());
  policy_.Build();
  device_settings_service_.Store(policy_.GetCopy(), base::Closure());
  device_settings_test_helper_.FlushStore();
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.new_signing_key());
  device_settings_service_.OwnerKeySet(true);
  device_settings_test_helper_.Flush();
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_.status());
  CheckPolicy();

  // Check the new key has been loaded.
  std::vector<uint8> key;
  ASSERT_TRUE(policy_.new_signing_key()->ExportPublicKey(&key));
  EXPECT_EQ(*device_settings_service_.GetOwnerKey()->public_key(), key);
}

TEST_F(DeviceSettingsServiceTest, OwnershipStatus) {
  EXPECT_FALSE(device_settings_service_.HasPrivateOwnerKey());
  EXPECT_FALSE(device_settings_service_.GetOwnerKey());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_UNKNOWN,
            device_settings_service_.GetOwnershipStatus());

  device_settings_service_.GetOwnershipStatusAsync(
      base::Bind(&DeviceSettingsServiceTest::SetOwnershipStatus,
                 base::Unretained(this)));
  device_settings_test_helper_.Flush();
  EXPECT_FALSE(device_settings_service_.HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_.GetOwnerKey());
  EXPECT_FALSE(device_settings_service_.GetOwnerKey()->public_key());
  EXPECT_FALSE(device_settings_service_.GetOwnerKey()->private_key());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_NONE,
            device_settings_service_.GetOwnershipStatus());
  EXPECT_FALSE(is_owner_);
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_NONE, ownership_status_);

  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();
  device_settings_service_.GetOwnershipStatusAsync(
      base::Bind(&DeviceSettingsServiceTest::SetOwnershipStatus,
                 base::Unretained(this)));
  device_settings_test_helper_.Flush();
  EXPECT_FALSE(device_settings_service_.HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_.GetOwnerKey());
  ASSERT_TRUE(device_settings_service_.GetOwnerKey()->public_key());
  std::vector<uint8> key;
  ASSERT_TRUE(policy_.signing_key()->ExportPublicKey(&key));
  EXPECT_EQ(*device_settings_service_.GetOwnerKey()->public_key(), key);
  EXPECT_FALSE(device_settings_service_.GetOwnerKey()->private_key());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            device_settings_service_.GetOwnershipStatus());
  EXPECT_FALSE(is_owner_);
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN, ownership_status_);

  owner_key_util_->SetPrivateKey(policy_.signing_key());
  device_settings_service_.SetUsername(policy_.policy_data().username());
  device_settings_service_.GetOwnershipStatusAsync(
      base::Bind(&DeviceSettingsServiceTest::SetOwnershipStatus,
                 base::Unretained(this)));
  device_settings_test_helper_.Flush();
  EXPECT_TRUE(device_settings_service_.HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_.GetOwnerKey());
  ASSERT_TRUE(device_settings_service_.GetOwnerKey()->public_key());
  ASSERT_TRUE(policy_.signing_key()->ExportPublicKey(&key));
  EXPECT_EQ(*device_settings_service_.GetOwnerKey()->public_key(), key);
  EXPECT_TRUE(device_settings_service_.GetOwnerKey()->private_key());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            device_settings_service_.GetOwnershipStatus());
  EXPECT_TRUE(is_owner_);
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN, ownership_status_);
}

TEST_F(DeviceSettingsServiceTest, Observer) {
  MockDeviceSettingsObserver observer_;
  device_settings_service_.AddObserver(&observer_);

  EXPECT_CALL(observer_, OwnershipStatusChanged()).Times(1);
  EXPECT_CALL(observer_, DeviceSettingsUpdated()).Times(1);
  device_settings_service_.Load();
  device_settings_test_helper_.Flush();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OwnershipStatusChanged()).Times(1);
  EXPECT_CALL(observer_, DeviceSettingsUpdated()).Times(1);
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
  device_settings_service_.OwnerKeySet(true);
  device_settings_test_helper_.Flush();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OwnershipStatusChanged()).Times(0);
  EXPECT_CALL(observer_, DeviceSettingsUpdated()).Times(1);
  device_settings_service_.Store(policy_.GetCopy(), base::Closure());
  device_settings_test_helper_.Flush();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OwnershipStatusChanged()).Times(0);
  EXPECT_CALL(observer_, DeviceSettingsUpdated()).Times(1);
  device_settings_service_.PropertyChangeComplete(true);
  device_settings_test_helper_.Flush();
  Mock::VerifyAndClearExpectations(&observer_);

  device_settings_service_.RemoveObserver(&observer_);
}

}  // namespace chromeos
