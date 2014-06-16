// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/session_manager_operation.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_factory.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/mock_owner_key_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "content/public/test/test_browser_thread.h"
#include "crypto/rsa_private_key.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::Mock;
using testing::_;

namespace chromeos {

class SessionManagerOperationTest : public testing::Test {
 public:
  SessionManagerOperationTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        owner_key_util_(new MockOwnerKeyUtil()),
        validated_(false) {}

  virtual void SetUp() OVERRIDE {
    policy_.payload().mutable_pinned_apps()->add_app_id("fake-app");
    policy_.Build();

    profile_.reset(new TestingProfile());
    OwnerSettingsService::SetOwnerKeyUtilForTesting(owner_key_util_);
    service_ = OwnerSettingsServiceFactory::GetForProfile(profile_.get());
  }

  void TearDown() { OwnerSettingsService::SetOwnerKeyUtilForTesting(NULL); }

  MOCK_METHOD2(OnOperationCompleted,
               void(SessionManagerOperation*, DeviceSettingsService::Status));

  void CheckSuccessfulValidation(
      policy::DeviceCloudPolicyValidator* validator) {
    EXPECT_TRUE(validator->success());
    EXPECT_TRUE(validator->payload().get());
    EXPECT_EQ(validator->payload()->SerializeAsString(),
              policy_.payload().SerializeAsString());
    validated_ = true;
  }

  void CheckPublicKeyLoaded(SessionManagerOperation* op) {
    ASSERT_TRUE(op->public_key().get());
    ASSERT_TRUE(op->public_key()->is_loaded());
    std::vector<uint8> public_key;
    ASSERT_TRUE(policy_.GetSigningKey()->ExportPublicKey(&public_key));
    EXPECT_EQ(public_key, op->public_key()->data());
  }

 protected:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  policy::DevicePolicyBuilder policy_;
  DeviceSettingsTestHelper device_settings_test_helper_;
  scoped_refptr<MockOwnerKeyUtil> owner_key_util_;

  scoped_ptr<TestingProfile> profile_;
  OwnerSettingsService* service_;

  bool validated_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManagerOperationTest);
};

TEST_F(SessionManagerOperationTest, LoadNoPolicyNoKey) {
  LoadSettingsOperation op(
      base::Bind(&SessionManagerOperationTest::OnOperationCompleted,
                 base::Unretained(this)));

  EXPECT_CALL(*this,
              OnOperationCompleted(
                  &op, DeviceSettingsService::STORE_KEY_UNAVAILABLE));
  op.Start(&device_settings_test_helper_, owner_key_util_, NULL);
  device_settings_test_helper_.Flush();
  Mock::VerifyAndClearExpectations(this);

  EXPECT_FALSE(op.policy_data().get());
  EXPECT_FALSE(op.device_settings().get());
  ASSERT_TRUE(op.public_key().get());
  EXPECT_FALSE(op.public_key()->is_loaded());
}

TEST_F(SessionManagerOperationTest, LoadOwnerKey) {
  owner_key_util_->SetPublicKeyFromPrivateKey(*policy_.GetSigningKey());
  LoadSettingsOperation op(
      base::Bind(&SessionManagerOperationTest::OnOperationCompleted,
                 base::Unretained(this)));

  EXPECT_CALL(*this,
              OnOperationCompleted(
                  &op, DeviceSettingsService::STORE_NO_POLICY));
  op.Start(&device_settings_test_helper_, owner_key_util_, NULL);
  device_settings_test_helper_.Flush();
  Mock::VerifyAndClearExpectations(this);

  CheckPublicKeyLoaded(&op);
}

TEST_F(SessionManagerOperationTest, LoadPolicy) {
  owner_key_util_->SetPublicKeyFromPrivateKey(*policy_.GetSigningKey());
  device_settings_test_helper_.set_policy_blob(policy_.GetBlob());
  LoadSettingsOperation op(
      base::Bind(&SessionManagerOperationTest::OnOperationCompleted,
                 base::Unretained(this)));

  EXPECT_CALL(*this,
              OnOperationCompleted(
                  &op, DeviceSettingsService::STORE_SUCCESS));
  op.Start(&device_settings_test_helper_, owner_key_util_, NULL);
  device_settings_test_helper_.Flush();
  Mock::VerifyAndClearExpectations(this);

  ASSERT_TRUE(op.policy_data().get());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            op.policy_data()->SerializeAsString());
  ASSERT_TRUE(op.device_settings().get());
  EXPECT_EQ(policy_.payload().SerializeAsString(),
            op.device_settings()->SerializeAsString());
}

TEST_F(SessionManagerOperationTest, RestartLoad) {
  owner_key_util_->SetPrivateKey(policy_.GetSigningKey());
  device_settings_test_helper_.set_policy_blob(policy_.GetBlob());
  LoadSettingsOperation op(
      base::Bind(&SessionManagerOperationTest::OnOperationCompleted,
                 base::Unretained(this)));

  EXPECT_CALL(*this, OnOperationCompleted(&op, _)).Times(0);
  op.Start(&device_settings_test_helper_, owner_key_util_, NULL);
  device_settings_test_helper_.FlushLoops();
  device_settings_test_helper_.FlushRetrieve();
  EXPECT_TRUE(op.public_key().get());
  EXPECT_TRUE(op.public_key()->is_loaded());
  Mock::VerifyAndClearExpectations(this);

  // Now install a different key and policy and restart the operation.
  policy_.SetSigningKey(*policy::PolicyBuilder::CreateTestOtherSigningKey());
  policy_.payload().mutable_metrics_enabled()->set_metrics_enabled(true);
  policy_.Build();
  device_settings_test_helper_.set_policy_blob(policy_.GetBlob());
  owner_key_util_->SetPrivateKey(policy_.GetSigningKey());

  EXPECT_CALL(*this,
              OnOperationCompleted(
                  &op, DeviceSettingsService::STORE_SUCCESS));
  op.RestartLoad(true);
  device_settings_test_helper_.Flush();
  Mock::VerifyAndClearExpectations(this);

  // Check that the new keys have been loaded.
  CheckPublicKeyLoaded(&op);

  // Verify the new policy.
  ASSERT_TRUE(op.policy_data().get());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            op.policy_data()->SerializeAsString());
  ASSERT_TRUE(op.device_settings().get());
  EXPECT_EQ(policy_.payload().SerializeAsString(),
            op.device_settings()->SerializeAsString());
}

TEST_F(SessionManagerOperationTest, StoreSettings) {
  owner_key_util_->SetPublicKeyFromPrivateKey(*policy_.GetSigningKey());
  StoreSettingsOperation op(
      base::Bind(&SessionManagerOperationTest::OnOperationCompleted,
                 base::Unretained(this)),
      policy_.GetCopy());

  EXPECT_CALL(*this,
              OnOperationCompleted(
                  &op, DeviceSettingsService::STORE_SUCCESS));
  op.Start(&device_settings_test_helper_, owner_key_util_, NULL);
  device_settings_test_helper_.Flush();
  Mock::VerifyAndClearExpectations(this);

  EXPECT_EQ(device_settings_test_helper_.policy_blob(),
            policy_.GetBlob());
  ASSERT_TRUE(op.policy_data().get());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            op.policy_data()->SerializeAsString());
  ASSERT_TRUE(op.device_settings().get());
  EXPECT_EQ(policy_.payload().SerializeAsString(),
            op.device_settings()->SerializeAsString());
}

TEST_F(SessionManagerOperationTest, SignAndStoreSettings) {
  owner_key_util_->SetPrivateKey(policy_.GetSigningKey());
  service_->OnTPMTokenReady();

  scoped_ptr<em::PolicyData> policy(new em::PolicyData(policy_.policy_data()));
  SignAndStoreSettingsOperation op(
      base::Bind(&SessionManagerOperationTest::OnOperationCompleted,
                 base::Unretained(this)),
      policy.Pass());
  op.set_delegate(service_->as_weak_ptr());

  EXPECT_CALL(*this,
              OnOperationCompleted(
                  &op, DeviceSettingsService::STORE_SUCCESS));
  op.Start(&device_settings_test_helper_, owner_key_util_, NULL);
  device_settings_test_helper_.Flush();
  Mock::VerifyAndClearExpectations(this);

  // The blob should validate.
  scoped_ptr<em::PolicyFetchResponse> policy_response(
      new em::PolicyFetchResponse());
  ASSERT_TRUE(
      policy_response->ParseFromString(
          device_settings_test_helper_.policy_blob()));
  policy::DeviceCloudPolicyValidator* validator =
      policy::DeviceCloudPolicyValidator::Create(
          policy_response.Pass(), message_loop_.message_loop_proxy());
  validator->ValidateUsername(policy_.policy_data().username(), true);
  const base::Time expected_time = base::Time::UnixEpoch() +
      base::TimeDelta::FromMilliseconds(policy::PolicyBuilder::kFakeTimestamp);
  validator->ValidateTimestamp(
      expected_time,
      expected_time,
      policy::CloudPolicyValidatorBase::TIMESTAMP_REQUIRED);
  validator->ValidatePolicyType(policy::dm_protocol::kChromeDevicePolicyType);
  validator->ValidatePayload();
  std::vector<uint8> public_key;
  policy_.GetSigningKey()->ExportPublicKey(&public_key);
  // Convert from bytes to string format (which is what ValidateSignature()
  // takes).
  std::string public_key_as_string = std::string(
      reinterpret_cast<const char*>(vector_as_array(&public_key)),
      public_key.size());
  validator->ValidateSignature(
      public_key_as_string,
      policy::GetPolicyVerificationKey(),
      policy::PolicyBuilder::kFakeDomain,
      false);
  validator->StartValidation(
      base::Bind(&SessionManagerOperationTest::CheckSuccessfulValidation,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();
  EXPECT_TRUE(validated_);

  // Loaded device settings should match what the operation received.
  ASSERT_TRUE(op.device_settings().get());
  EXPECT_EQ(policy_.payload().SerializeAsString(),
            op.device_settings()->SerializeAsString());
}

}  // namespace chromeos
