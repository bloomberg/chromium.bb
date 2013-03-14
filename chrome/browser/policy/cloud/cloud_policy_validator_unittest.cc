// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/cloud_policy_validator.h"
#include "chrome/browser/policy/cloud/policy_builder.h"
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
        ignore_missing_timestamp_(CloudPolicyValidatorBase::TIMESTAMP_REQUIRED),
        ignore_missing_dm_token_(CloudPolicyValidatorBase::DM_TOKEN_REQUIRED),
        allow_key_rotation_(true),
        existing_dm_token_(PolicyBuilder::kFakeToken),
        file_thread_(content::BrowserThread::FILE, &loop_) {
    policy_.set_new_signing_key(PolicyBuilder::CreateTestNewSigningKey());
  }

  void Validate(testing::Action<void(UserCloudPolicyValidator*)> check_action) {
    // Create a validator.
    scoped_ptr<UserCloudPolicyValidator> validator = CreateValidator();

    // Run validation and check the result.
    EXPECT_CALL(*this, ValidationCompletion(validator.get())).WillOnce(
        check_action);
    validator.release()->StartValidation(
        base::Bind(&CloudPolicyValidatorTest::ValidationCompletion,
                   base::Unretained(this)));
    loop_.RunUntilIdle();
    Mock::VerifyAndClearExpectations(this);
  }

  scoped_ptr<UserCloudPolicyValidator> CreateValidator() {
    std::vector<uint8> public_key;
    EXPECT_TRUE(
        PolicyBuilder::CreateTestSigningKey()->ExportPublicKey(&public_key));
    policy_.Build();

    UserCloudPolicyValidator* validator =
        UserCloudPolicyValidator::Create(policy_.GetCopy());
    validator->ValidateTimestamp(timestamp_, timestamp_,
                                 ignore_missing_timestamp_);
    validator->ValidateUsername(PolicyBuilder::kFakeUsername);
    validator->ValidateDomain(PolicyBuilder::kFakeDomain);
    validator->ValidateDMToken(existing_dm_token_, ignore_missing_dm_token_);
    validator->ValidatePolicyType(dm_protocol::kChromeUserPolicyType);
    validator->ValidatePayload();
    validator->ValidateSignature(public_key, allow_key_rotation_);
    if (allow_key_rotation_)
      validator->ValidateInitialKey();
    return make_scoped_ptr(validator);
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
  CloudPolicyValidatorBase::ValidateTimestampOption ignore_missing_timestamp_;
  CloudPolicyValidatorBase::ValidateDMTokenOption ignore_missing_dm_token_;
  std::string signing_key_;
  bool allow_key_rotation_;
  std::string existing_dm_token_;

  UserPolicyBuilder policy_;

 private:
  MOCK_METHOD1(ValidationCompletion, void(UserCloudPolicyValidator* validator));

  content::TestBrowserThread file_thread_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyValidatorTest);
};

TEST_F(CloudPolicyValidatorTest, SuccessfulValidation) {
  Validate(Invoke(this, &CloudPolicyValidatorTest::CheckSuccessfulValidation));
}

TEST_F(CloudPolicyValidatorTest, SuccessfulRunValidation) {
  scoped_ptr<UserCloudPolicyValidator> validator = CreateValidator();
  // Run validation immediately (no background tasks).
  validator->RunValidation();
  CheckSuccessfulValidation(validator.get());
}

TEST_F(CloudPolicyValidatorTest, SuccessfulRunValidationWithNoExistingDMToken) {
  existing_dm_token_.clear();
  Validate(Invoke(this, &CloudPolicyValidatorTest::CheckSuccessfulValidation));
}

TEST_F(CloudPolicyValidatorTest, SuccessfulRunValidationWithNoDMTokens) {
  existing_dm_token_.clear();
  policy_.policy_data().clear_request_token();
  ignore_missing_dm_token_ = CloudPolicyValidatorBase::DM_TOKEN_NOT_REQUIRED;
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

TEST_F(CloudPolicyValidatorTest, IgnoreMissingTimestamp) {
  ignore_missing_timestamp_ = CloudPolicyValidatorBase::TIMESTAMP_NOT_REQUIRED;
  policy_.policy_data().clear_timestamp();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK));
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

TEST_F(CloudPolicyValidatorTest, ErrorNoRequestTokenNotRequired) {
  // Even though DMTokens are not required, if the existing policy has a token,
  // we should still generate an error if the new policy has none.
  policy_.policy_data().clear_request_token();
  ignore_missing_dm_token_ = CloudPolicyValidatorBase::DM_TOKEN_NOT_REQUIRED;
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_WRONG_TOKEN));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoRequestTokenNoTokenPassed) {
  // Mimic the first fetch of policy (no existing DM token) - should still
  // complain about not having any DMToken.
  existing_dm_token_.clear();
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

TEST_F(CloudPolicyValidatorTest, ErrorNoRotationAllowed) {
  allow_key_rotation_ = false;
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, NoRotation) {
  allow_key_rotation_ = false;
  policy_.set_new_signing_key(scoped_ptr<crypto::RSAPrivateKey>());
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK));
}

}  // namespace

}  // namespace policy
