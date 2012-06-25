// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud_policy_validator.h"
#include "chrome/browser/policy/policy_builder.h"
#include "content/public/test/test_browser_thread.h"
#include "crypto/rsa_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::Invoke;
using testing::Mock;

namespace policy {

namespace {

ACTION_P(CheckStatus, expected_status) {
  EXPECT_EQ(expected_status, arg0->status());
};

class CloudPolicyValidatorTest : public testing::Test {
 public:
  CloudPolicyValidatorTest()
      : loop_(MessageLoop::TYPE_UI),
        timestamp_(base::Time::UnixEpoch() +
                   base::TimeDelta::FromMilliseconds(
                       PolicyBuilder::kFakeTimestamp)),
        file_thread_(content::BrowserThread::FILE, &loop_) {}

  void Validate(testing::Action<void(UserCloudPolicyValidator*)> check_action) {
    std::vector<uint8> public_key;
    ASSERT_TRUE(
        PolicyBuilder::CreateTestSigningKey()->ExportPublicKey(&public_key));

    // Create a validator.
    policy_.Build();
    UserCloudPolicyValidator* validator =
        UserCloudPolicyValidator::Create(
            policy_.GetCopy(),
            base::Bind(&CloudPolicyValidatorTest::ValidationCompletion,
                       base::Unretained(this)));
    validator->ValidateTimestamp(timestamp_, timestamp_);
    validator->ValidateUsername(PolicyBuilder::kFakeUsername);
    validator->ValidateDomain(PolicyBuilder::kFakeDomain);
    validator->ValidateDMToken(PolicyBuilder::kFakeToken);
    validator->ValidatePolicyType(dm_protocol::kChromeUserPolicyType);
    validator->ValidatePayload();
    validator->ValidateSignature(std::string(public_key.begin(),
                                             public_key.end()));
    validator->ValidateInitialKey();

    // Run validation and check the result.
    EXPECT_CALL(*this, ValidationCompletion(validator)).WillOnce(check_action);
    validator->StartValidation();
    loop_.RunAllPending();
    Mock::VerifyAndClearExpectations(this);
  }

  void CheckSuccessfulValidation(UserCloudPolicyValidator* validator) {
    EXPECT_TRUE(validator->success());
    EXPECT_EQ(policy_.policy().SerializeAsString(),
              validator->policy()->SerializeAsString());
    EXPECT_EQ(policy_.policy_data().SerializeAsString(),
              validator->policy_data()->SerializeAsString());
    EXPECT_EQ(policy_.payload().SerializeAsString(),
              validator->payload()->SerializeAsString());
  }

  MessageLoop loop_;
  base::Time timestamp_;
  std::string signing_key_;

  UserPolicyBuilder policy_;

 private:
  MOCK_METHOD1(ValidationCompletion, void(UserCloudPolicyValidator* validator));

  content::TestBrowserThread file_thread_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyValidatorTest);
};

TEST_F(CloudPolicyValidatorTest, SuccessfulValidation) {
  Validate(Invoke(this, &CloudPolicyValidatorTest::CheckSuccessfulValidation));
}

TEST_F(CloudPolicyValidatorTest, UsernameCanonicalization) {
  policy_.policy_data().set_username(
      StringToUpperASCII(std::string(PolicyBuilder::kFakeUsername)));
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoPolicyType) {
  policy_.policy_data().clear_policy_type();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_WRONG_POLICY_TYPE));
}

TEST_F(CloudPolicyValidatorTest, ErrorWrongPolicyType) {
  policy_.policy_data().set_policy_type("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_WRONG_POLICY_TYPE));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoTimestamp) {
  policy_.policy_data().clear_timestamp();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_TIMESTAMP));
}

TEST_F(CloudPolicyValidatorTest, ErrorOldTimestamp) {
  base::Time timestamp(timestamp_ - base::TimeDelta::FromMinutes(5));
  policy_.policy_data().set_timestamp(
      (timestamp - base::Time::UnixEpoch()).InMilliseconds());
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_TIMESTAMP));
}

TEST_F(CloudPolicyValidatorTest, ErrorTimestampFromTheFuture) {
  base::Time timestamp(timestamp_ + base::TimeDelta::FromMinutes(5));
  policy_.policy_data().set_timestamp(
      (timestamp - base::Time::UnixEpoch()).InMilliseconds());
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_TIMESTAMP));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoRequestToken) {
  policy_.policy_data().clear_request_token();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_WRONG_TOKEN));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidRequestToken) {
  policy_.policy_data().set_request_token("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_WRONG_TOKEN));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoPolicyValue) {
  policy_.clear_payload();
  Validate(
      CheckStatus(CloudPolicyValidatorBase::VALIDATION_POLICY_PARSE_ERROR));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidPolicyValue) {
  policy_.clear_payload();
  policy_.policy_data().set_policy_value("invalid");
  Validate(
      CheckStatus(CloudPolicyValidatorBase::VALIDATION_POLICY_PARSE_ERROR));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoUsername) {
  policy_.policy_data().clear_username();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_USERNAME));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidUsername) {
  policy_.policy_data().set_username("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_USERNAME));
}

TEST_F(CloudPolicyValidatorTest, ErrorErrorMessage) {
  policy_.policy().set_error_message("error");
  Validate(
      CheckStatus(CloudPolicyValidatorBase::VALIDATION_ERROR_CODE_PRESENT));
}

TEST_F(CloudPolicyValidatorTest, ErrorErrorCode) {
  policy_.policy().set_error_code(42);
  Validate(
      CheckStatus(CloudPolicyValidatorBase::VALIDATION_ERROR_CODE_PRESENT));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoSignature) {
  policy_.set_signing_key(scoped_ptr<crypto::RSAPrivateKey>());
  policy_.set_new_signing_key(scoped_ptr<crypto::RSAPrivateKey>());
  policy_.policy().clear_policy_data_signature();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidSignature) {
  policy_.set_signing_key(scoped_ptr<crypto::RSAPrivateKey>());
  policy_.set_new_signing_key(scoped_ptr<crypto::RSAPrivateKey>());
  policy_.policy().set_policy_data_signature("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoPublicKey) {
  policy_.set_signing_key(scoped_ptr<crypto::RSAPrivateKey>());
  policy_.set_new_signing_key(scoped_ptr<crypto::RSAPrivateKey>());
  policy_.policy().clear_new_public_key();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidPublicKey) {
  policy_.set_signing_key(scoped_ptr<crypto::RSAPrivateKey>());
  policy_.set_new_signing_key(scoped_ptr<crypto::RSAPrivateKey>());
  policy_.policy().set_new_public_key("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoPublicKeySignature) {
  policy_.set_signing_key(scoped_ptr<crypto::RSAPrivateKey>());
  policy_.set_new_signing_key(scoped_ptr<crypto::RSAPrivateKey>());
  policy_.policy().clear_new_public_key_signature();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidPublicKeySignature) {
  policy_.set_signing_key(scoped_ptr<crypto::RSAPrivateKey>());
  policy_.set_new_signing_key(scoped_ptr<crypto::RSAPrivateKey>());
  policy_.policy().set_new_public_key_signature("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

}  // namespace

}  // namespace policy
