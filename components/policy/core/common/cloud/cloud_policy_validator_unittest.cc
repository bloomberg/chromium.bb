// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_validator.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/policy_switches.h"
#include "crypto/rsa_private_key.h"
#include "policy/proto/device_management_backend.pb.h"
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
      : timestamp_(base::Time::UnixEpoch() +
                   base::TimeDelta::FromMilliseconds(
                       PolicyBuilder::kFakeTimestamp)),
        timestamp_option_(CloudPolicyValidatorBase::TIMESTAMP_REQUIRED),
        ignore_missing_dm_token_(CloudPolicyValidatorBase::DM_TOKEN_REQUIRED),
        allow_key_rotation_(true),
        existing_dm_token_(PolicyBuilder::kFakeToken),
        owning_domain_(PolicyBuilder::kFakeDomain),
        cached_key_signature_(PolicyBuilder::GetTestSigningKeySignature()) {
    policy_.SetDefaultNewSigningKey();
  }

  void Validate(testing::Action<void(UserCloudPolicyValidator*)> check_action) {
    policy_.Build();
    ValidatePolicy(check_action, policy_.GetCopy());
  }

  void ValidatePolicy(
      testing::Action<void(UserCloudPolicyValidator*)> check_action,
      std::unique_ptr<em::PolicyFetchResponse> policy_response) {
    // Create a validator.
    std::unique_ptr<UserCloudPolicyValidator> validator =
        CreateValidator(std::move(policy_response));

    // Run validation and check the result.
    EXPECT_CALL(*this, ValidationCompletion(validator.get())).WillOnce(
        check_action);
    validator.release()->StartValidation(
        base::Bind(&CloudPolicyValidatorTest::ValidationCompletion,
                   base::Unretained(this)));
    loop_.RunUntilIdle();
    Mock::VerifyAndClearExpectations(this);
  }

  std::unique_ptr<UserCloudPolicyValidator> CreateValidator(
      std::unique_ptr<em::PolicyFetchResponse> policy_response) {
    std::vector<uint8_t> public_key_bytes;
    EXPECT_TRUE(
        PolicyBuilder::CreateTestSigningKey()->ExportPublicKey(
            &public_key_bytes));

    // Convert from bytes to string format (which is what ValidateSignature()
    // takes).
    std::string public_key =
        std::string(reinterpret_cast<const char*>(public_key_bytes.data()),
                    public_key_bytes.size());

    UserCloudPolicyValidator* validator = UserCloudPolicyValidator::Create(
        std::move(policy_response), base::ThreadTaskRunnerHandle::Get());
    validator->ValidateTimestamp(timestamp_, timestamp_,
                                 timestamp_option_);
    validator->ValidateUsername(PolicyBuilder::kFakeUsername, true);
    if (!owning_domain_.empty())
      validator->ValidateDomain(owning_domain_);
    validator->ValidateDMToken(existing_dm_token_, ignore_missing_dm_token_);
    validator->ValidatePolicyType(dm_protocol::kChromeUserPolicyType);
    validator->ValidatePayload();
    validator->ValidateCachedKey(public_key,
                                 cached_key_signature_,
                                 GetPolicyVerificationKey(),
                                 owning_domain_);
    validator->ValidateSignature(public_key,
                                 GetPolicyVerificationKey(),
                                 owning_domain_,
                                 allow_key_rotation_);
    if (allow_key_rotation_)
      validator->ValidateInitialKey(GetPolicyVerificationKey(), owning_domain_);
    return base::WrapUnique(validator);
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

  base::MessageLoopForUI loop_;
  base::Time timestamp_;
  CloudPolicyValidatorBase::ValidateTimestampOption timestamp_option_;
  CloudPolicyValidatorBase::ValidateDMTokenOption ignore_missing_dm_token_;
  std::string signing_key_;
  bool allow_key_rotation_;
  std::string existing_dm_token_;
  std::string owning_domain_;
  std::string cached_key_signature_;

  UserPolicyBuilder policy_;

 private:
  MOCK_METHOD1(ValidationCompletion, void(UserCloudPolicyValidator* validator));

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyValidatorTest);
};

TEST_F(CloudPolicyValidatorTest, SuccessfulValidation) {
  Validate(Invoke(this, &CloudPolicyValidatorTest::CheckSuccessfulValidation));
}

TEST_F(CloudPolicyValidatorTest, SuccessfulRunValidation) {
  policy_.Build();
  std::unique_ptr<UserCloudPolicyValidator> validator =
      CreateValidator(policy_.GetCopy());
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
      base::ToUpperASCII(PolicyBuilder::kFakeUsername));
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
  timestamp_option_ = CloudPolicyValidatorBase::TIMESTAMP_NOT_REQUIRED;
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
  base::Time timestamp(timestamp_ + base::TimeDelta::FromHours(3));
  policy_.policy_data().set_timestamp(
      (timestamp - base::Time::UnixEpoch()).InMilliseconds());
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_TIMESTAMP));
}

TEST_F(CloudPolicyValidatorTest, IgnoreErrorTimestampFromTheFuture) {
  base::Time timestamp(timestamp_ + base::TimeDelta::FromMinutes(5));
  timestamp_option_ =
      CloudPolicyValidatorBase::TIMESTAMP_NOT_BEFORE;
  policy_.policy_data().set_timestamp(
      (timestamp - base::Time::UnixEpoch()).InMilliseconds());
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK));
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
  policy_.policy_data().set_username("invalid@example.com");
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
  policy_.UnsetSigningKey();
  policy_.UnsetNewSigningKey();
  policy_.policy().clear_policy_data_signature();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidSignature) {
  policy_.UnsetSigningKey();
  policy_.UnsetNewSigningKey();
  policy_.policy().set_policy_data_signature("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoPublicKey) {
  policy_.UnsetSigningKey();
  policy_.UnsetNewSigningKey();
  policy_.policy().clear_new_public_key();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidPublicKey) {
  policy_.UnsetSigningKey();
  policy_.UnsetNewSigningKey();
  policy_.policy().set_new_public_key("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoPublicKeySignature) {
  policy_.UnsetSigningKey();
  policy_.UnsetNewSigningKey();
  policy_.policy().clear_new_public_key_signature();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidPublicKeySignature) {
  policy_.UnsetSigningKey();
  policy_.UnsetNewSigningKey();
  policy_.policy().set_new_public_key_signature("invalid");
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

#if !defined(OS_CHROMEOS)
// Validation key is not currently checked on Chrome OS
// (http://crbug.com/328038).
TEST_F(CloudPolicyValidatorTest, ErrorInvalidPublicKeyVerificationSignature) {
  policy_.Build();
  policy_.policy().set_new_public_key_verification_signature("invalid");
  ValidatePolicy(CheckStatus(
      CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE),
                 policy_.GetCopy());
}

TEST_F(CloudPolicyValidatorTest, ErrorDomainMismatchForKeyVerification) {
  policy_.Build();
  // Generate a non-matching owning_domain, which should cause a validation
  // failure.
  owning_domain_ = "invalid.com";
  ValidatePolicy(CheckStatus(
      CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE),
                 policy_.GetCopy());
}

TEST_F(CloudPolicyValidatorTest, ErrorDomainExtractedFromUsernameMismatch) {
  // Generate a non-matching username domain, which should cause a validation
  // failure when we try to verify the signing key with it.
  policy_.policy_data().set_username("wonky@invalid.com");
  policy_.Build();
  // Pass an empty domain to tell validator to extract the domain from the
  // policy's |username| field.
  owning_domain_ = "";
  ValidatePolicy(CheckStatus(
      CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE),
                 policy_.GetCopy());
}

TEST_F(CloudPolicyValidatorTest, ErrorNoCachedKeySignature) {
  // Generate an empty cached_key_signature_ and this should cause a validation
  // error when we try to verify the signing key with it.
  cached_key_signature_ = "";
  Validate(CheckStatus(
      CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, ErrorInvalidCachedKeySignature) {
  // Generate a key signature for a different key (one that does not match
  // the signing key) and this should cause a validation error when we try to
  // verify the signing key with it.
  cached_key_signature_ = PolicyBuilder::GetTestOtherSigningKeySignature();
  Validate(CheckStatus(
      CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE));
}
#endif

TEST_F(CloudPolicyValidatorTest, SuccessfulNoDomainValidation) {
  // Don't pass in a domain - this tells the validation code to instead
  // extract the domain from the username.
  owning_domain_ = "";
  Validate(Invoke(this, &CloudPolicyValidatorTest::CheckSuccessfulValidation));
}

TEST_F(CloudPolicyValidatorTest, ErrorNoRotationAllowed) {
  allow_key_rotation_ = false;
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE));
}

TEST_F(CloudPolicyValidatorTest, NoRotation) {
  allow_key_rotation_ = false;
  policy_.UnsetNewSigningKey();
  Validate(CheckStatus(CloudPolicyValidatorBase::VALIDATION_OK));
}

}  // namespace

}  // namespace policy
