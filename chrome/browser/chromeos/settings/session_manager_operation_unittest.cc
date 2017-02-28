// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/session_manager_operation.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
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
        owner_key_util_(new ownership::MockOwnerKeyUtil()),
        user_manager_(new chromeos::FakeChromeUserManager()),
        user_manager_enabler_(user_manager_),
        validated_(false) {
    OwnerSettingsServiceChromeOSFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(owner_key_util_);
  }

  void SetUp() override {
    policy_.payload().mutable_user_whitelist()->add_user_whitelist(
        "fake-whitelist");
    policy_.Build();

    profile_.reset(new TestingProfile());
    service_ = OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
        profile_.get());
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
    ASSERT_TRUE(op->public_key().get());
    ASSERT_TRUE(op->public_key()->is_loaded());
    std::vector<uint8_t> public_key;
    ASSERT_TRUE(policy_.GetSigningKey()->ExportPublicKey(&public_key));
    EXPECT_EQ(public_key, op->public_key()->data());
  }

 protected:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  policy::DevicePolicyBuilder policy_;
  DeviceSettingsTestHelper device_settings_test_helper_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;

  chromeos::FakeChromeUserManager* user_manager_;
  ScopedUserManagerEnabler user_manager_enabler_;

  std::unique_ptr<TestingProfile> profile_;
  OwnerSettingsServiceChromeOS* service_;

  bool validated_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManagerOperationTest);
};

TEST_F(SessionManagerOperationTest, LoadNoPolicyNoKey) {
  LoadSettingsOperation op(
      false /* force_key_load */, true /* cloud_validations */,
      false /* force_immediate_load */,
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
      false /* force_key_load */, true /* cloud_validations */,
      false /* force_immediate_load */,
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
      false /* force_key_load */, true /* cloud_validations */,
      false /* force_immediate_load */,
      base::Bind(&SessionManagerOperationTest::OnOperationCompleted,
                 base::Unretained(this)));

  EXPECT_CALL(*this,
              OnOperationCompleted(&op, DeviceSettingsService::STORE_SUCCESS));
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

TEST_F(SessionManagerOperationTest, LoadImmediately) {
  owner_key_util_->SetPublicKeyFromPrivateKey(*policy_.GetSigningKey());
  device_settings_test_helper_.set_policy_blob(policy_.GetBlob());
  LoadSettingsOperation op(
      false /* force_key_load */, true /* cloud_validations */,
      true /* force_immediate_load */,
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
      false /* force_key_load */, true /* cloud_validations */,
      false /* force_immediate_load */,
      base::Bind(&SessionManagerOperationTest::OnOperationCompleted,
                 base::Unretained(this)));

  EXPECT_CALL(*this, OnOperationCompleted(&op, _)).Times(0);
  op.Start(&device_settings_test_helper_, owner_key_util_, NULL);
  content::RunAllBlockingPoolTasksUntilIdle();
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

}  // namespace chromeos
