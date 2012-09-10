// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/session_manager_operation.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/mock_owner_key_util.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud_policy_validator.h"
#include "chrome/browser/policy/policy_builder.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "content/public/test/test_browser_thread.h"
#include "crypto/rsa_private_key.h"
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
  }

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
    ASSERT_TRUE(op->owner_key().get());
    ASSERT_TRUE(op->owner_key()->public_key());
    std::vector<uint8> public_key;
    ASSERT_TRUE(policy_.signing_key()->ExportPublicKey(&public_key));
    EXPECT_EQ(public_key, *op->owner_key()->public_key());
  }

  void CheckPrivateKeyLoaded(SessionManagerOperation* op) {
    ASSERT_TRUE(op->owner_key().get());
    ASSERT_TRUE(op->owner_key()->private_key());
    std::vector<uint8> expected_key;
    ASSERT_TRUE(policy_.signing_key()->ExportPrivateKey(&expected_key));
    std::vector<uint8> actual_key;
    ASSERT_TRUE(op->owner_key()->private_key()->ExportPrivateKey(&actual_key));
    EXPECT_EQ(expected_key, actual_key);
  }

 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  policy::DevicePolicyBuilder policy_;
  DeviceSettingsTestHelper device_settings_test_helper_;
  scoped_refptr<MockOwnerKeyUtil> owner_key_util_;

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
  ASSERT_TRUE(op.owner_key().get());
  EXPECT_FALSE(op.owner_key()->public_key());
  EXPECT_FALSE(op.owner_key()->private_key());
}

TEST_F(SessionManagerOperationTest, LoadOwnerKey) {
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
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
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
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

TEST_F(SessionManagerOperationTest, LoadPrivateOwnerKey) {
  owner_key_util_->SetPrivateKey(policy_.signing_key());
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
  CheckPrivateKeyLoaded(&op);
}

TEST_F(SessionManagerOperationTest, RestartLoad) {
  owner_key_util_->SetPrivateKey(policy_.signing_key());
  device_settings_test_helper_.set_policy_blob(policy_.GetBlob());
  LoadSettingsOperation op(
      base::Bind(&SessionManagerOperationTest::OnOperationCompleted,
                 base::Unretained(this)));

  EXPECT_CALL(*this, OnOperationCompleted(&op, _)).Times(0);
  op.Start(&device_settings_test_helper_, owner_key_util_, NULL);
  device_settings_test_helper_.FlushLoops();
  device_settings_test_helper_.FlushRetrieve();
  EXPECT_TRUE(op.owner_key());
  EXPECT_TRUE(op.owner_key()->public_key());
  Mock::VerifyAndClearExpectations(this);

  // Now install a different key and policy and restart the operation.
  policy_.set_signing_key(policy::PolicyBuilder::CreateTestNewSigningKey());
  policy_.payload().mutable_metrics_enabled()->set_metrics_enabled(true);
  policy_.Build();
  device_settings_test_helper_.set_policy_blob(policy_.GetBlob());
  owner_key_util_->SetPrivateKey(policy_.signing_key());

  EXPECT_CALL(*this,
              OnOperationCompleted(
                  &op, DeviceSettingsService::STORE_SUCCESS));
  op.RestartLoad(true);
  device_settings_test_helper_.Flush();
  Mock::VerifyAndClearExpectations(this);

  // Check that the new keys have been loaded.
  CheckPublicKeyLoaded(&op);
  CheckPrivateKeyLoaded(&op);

  // Verify the new policy.
  ASSERT_TRUE(op.policy_data().get());
  EXPECT_EQ(policy_.policy_data().SerializeAsString(),
            op.policy_data()->SerializeAsString());
  ASSERT_TRUE(op.device_settings().get());
  EXPECT_EQ(policy_.payload().SerializeAsString(),
            op.device_settings()->SerializeAsString());
}

TEST_F(SessionManagerOperationTest, StoreSettings) {
  owner_key_util_->SetPublicKeyFromPrivateKey(policy_.signing_key());
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
  base::Time before(base::Time::NowFromSystemTime());
  owner_key_util_->SetPrivateKey(policy_.signing_key());
  SignAndStoreSettingsOperation op(
      base::Bind(&SessionManagerOperationTest::OnOperationCompleted,
                 base::Unretained(this)),
      scoped_ptr<em::ChromeDeviceSettingsProto>(
          new em::ChromeDeviceSettingsProto(policy_.payload())),
      policy_.policy_data().username());

  EXPECT_CALL(*this,
              OnOperationCompleted(
                  &op, DeviceSettingsService::STORE_SUCCESS));
  op.Start(&device_settings_test_helper_, owner_key_util_, NULL);
  device_settings_test_helper_.Flush();
  Mock::VerifyAndClearExpectations(this);
  base::Time after(base::Time::NowFromSystemTime());

  // The blob should validate.
  scoped_ptr<em::PolicyFetchResponse> policy_response(
      new em::PolicyFetchResponse());
  ASSERT_TRUE(
      policy_response->ParseFromString(
          device_settings_test_helper_.policy_blob()));
  policy::DeviceCloudPolicyValidator* validator =
      policy::DeviceCloudPolicyValidator::Create(
          policy_response.Pass(),
          base::Bind(&SessionManagerOperationTest::CheckSuccessfulValidation,
                     base::Unretained(this)));
  validator->ValidateUsername(policy_.policy_data().username());
  validator->ValidateTimestamp(before, after, false);
  validator->ValidatePolicyType(policy::dm_protocol::kChromeDevicePolicyType);
  validator->ValidatePayload();
  std::vector<uint8> public_key;
  policy_.signing_key()->ExportPublicKey(&public_key);
  validator->ValidateSignature(public_key, false);
  validator->StartValidation();
  message_loop_.RunAllPending();
  EXPECT_TRUE(validated_);

  // Check that the loaded policy_data contains the expected values.
  EXPECT_EQ(policy::dm_protocol::kChromeDevicePolicyType,
            op.policy_data()->policy_type());
  EXPECT_LE((before - base::Time::UnixEpoch()).InMilliseconds(),
            op.policy_data()->timestamp());
  EXPECT_GE((after - base::Time::UnixEpoch()).InMilliseconds(),
            op.policy_data()->timestamp());
  EXPECT_FALSE(op.policy_data()->has_request_token());
  EXPECT_EQ(policy_.policy_data().username(), op.policy_data()->username());

  // Loaded device settings should match what the operation received.
  ASSERT_TRUE(op.device_settings().get());
  EXPECT_EQ(policy_.payload().SerializeAsString(),
            op.device_settings()->SerializeAsString());
}

}  // namespace chromeos
