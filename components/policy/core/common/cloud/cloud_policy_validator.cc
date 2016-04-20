// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_validator.h"

#include <stddef.h>
#include <utility>

#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "crypto/signature_verifier.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Grace interval for policy-from-the-future timestamp checks.
const int kTimestampGraceIntervalHours = 2;

const char kMetricPolicyKeyVerification[] = "Enterprise.PolicyKeyVerification";

enum MetricPolicyKeyVerification {
  // UMA metric recorded when the client has no verification key.
  METRIC_POLICY_KEY_VERIFICATION_KEY_MISSING,
  // Recorded when the policy being verified has no key signature (e.g. policy
  // fetched before the server supported the verification key).
  METRIC_POLICY_KEY_VERIFICATION_SIGNATURE_MISSING,
  // Recorded when the key signature did not match the expected value (in
  // theory, this should only happen after key rotation or if the policy cached
  // on disk has been modified).
  METRIC_POLICY_KEY_VERIFICATION_FAILED,
  // Recorded when key verification succeeded.
  METRIC_POLICY_KEY_VERIFICATION_SUCCEEDED,
  METRIC_POLICY_KEY_VERIFICATION_SIZE  // Must be the last.
};

}  // namespace

CloudPolicyValidatorBase::~CloudPolicyValidatorBase() {}

void CloudPolicyValidatorBase::ValidateTimestamp(
    base::Time not_before,
    base::Time now,
    ValidateTimestampOption timestamp_option) {
  validation_flags_ |= VALIDATE_TIMESTAMP;
  timestamp_not_before_ =
      (not_before - base::Time::UnixEpoch()).InMilliseconds();
  timestamp_not_after_ =
      ((now + base::TimeDelta::FromHours(kTimestampGraceIntervalHours)) -
          base::Time::UnixEpoch()).InMillisecondsRoundedUp();
  timestamp_option_ = timestamp_option;
}

void CloudPolicyValidatorBase::ValidateUsername(
    const std::string& expected_user,
    bool canonicalize) {
  validation_flags_ |= VALIDATE_USERNAME;
  user_ = expected_user;
  canonicalize_user_ = canonicalize;
}

void CloudPolicyValidatorBase::ValidateDomain(
    const std::string& expected_domain) {
  validation_flags_ |= VALIDATE_DOMAIN;
  domain_ = gaia::CanonicalizeDomain(expected_domain);
}

void CloudPolicyValidatorBase::ValidateDMToken(
    const std::string& token,
    ValidateDMTokenOption dm_token_option) {
  validation_flags_ |= VALIDATE_TOKEN;
  token_ = token;
  dm_token_option_ = dm_token_option;
}

void CloudPolicyValidatorBase::ValidatePolicyType(
    const std::string& policy_type) {
  validation_flags_ |= VALIDATE_POLICY_TYPE;
  policy_type_ = policy_type;
}

void CloudPolicyValidatorBase::ValidateSettingsEntityId(
    const std::string& settings_entity_id) {
  validation_flags_ |= VALIDATE_ENTITY_ID;
  settings_entity_id_ = settings_entity_id;
}

void CloudPolicyValidatorBase::ValidatePayload() {
  validation_flags_ |= VALIDATE_PAYLOAD;
}


void CloudPolicyValidatorBase::ValidateCachedKey(
    const std::string& cached_key,
    const std::string& cached_key_signature,
    const std::string& verification_key,
    const std::string& owning_domain) {
  validation_flags_ |= VALIDATE_CACHED_KEY;
  set_verification_key_and_domain(verification_key, owning_domain);
  cached_key_ = cached_key;
  cached_key_signature_ = cached_key_signature;
}

void CloudPolicyValidatorBase::ValidateSignature(
    const std::string& key,
    const std::string& verification_key,
    const std::string& owning_domain,
    bool allow_key_rotation) {
  validation_flags_ |= VALIDATE_SIGNATURE;
  set_verification_key_and_domain(verification_key, owning_domain);
  key_ = key;
  allow_key_rotation_ = allow_key_rotation;
}

void CloudPolicyValidatorBase::ValidateInitialKey(
    const std::string& verification_key,
    const std::string& owning_domain) {
  validation_flags_ |= VALIDATE_INITIAL_KEY;
  set_verification_key_and_domain(verification_key, owning_domain);
}

void CloudPolicyValidatorBase::ValidateAgainstCurrentPolicy(
    const em::PolicyData* policy_data,
    ValidateTimestampOption timestamp_option,
    ValidateDMTokenOption dm_token_option) {
  base::Time last_policy_timestamp;
  std::string expected_dm_token;
  if (policy_data) {
    last_policy_timestamp =
        base::Time::UnixEpoch() +
        base::TimeDelta::FromMilliseconds(policy_data->timestamp());
    expected_dm_token = policy_data->request_token();
  }
  ValidateTimestamp(last_policy_timestamp, base::Time::NowFromSystemTime(),
                    timestamp_option);
  ValidateDMToken(expected_dm_token, dm_token_option);
}

CloudPolicyValidatorBase::CloudPolicyValidatorBase(
    std::unique_ptr<em::PolicyFetchResponse> policy_response,
    google::protobuf::MessageLite* payload,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : status_(VALIDATION_OK),
      policy_(std::move(policy_response)),
      payload_(payload),
      validation_flags_(0),
      timestamp_not_before_(0),
      timestamp_not_after_(0),
      timestamp_option_(TIMESTAMP_REQUIRED),
      dm_token_option_(DM_TOKEN_REQUIRED),
      canonicalize_user_(false),
      allow_key_rotation_(false),
      background_task_runner_(background_task_runner) {}

void CloudPolicyValidatorBase::PostValidationTask(
    const base::Closure& completion_callback) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CloudPolicyValidatorBase::PerformValidation,
                 base::Passed(std::unique_ptr<CloudPolicyValidatorBase>(this)),
                 base::ThreadTaskRunnerHandle::Get(), completion_callback));
}

// static
void CloudPolicyValidatorBase::PerformValidation(
    std::unique_ptr<CloudPolicyValidatorBase> self,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Closure& completion_callback) {
  // Run the validation activities on this thread.
  self->RunValidation();

  // Report completion on |task_runner|.
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&CloudPolicyValidatorBase::ReportCompletion,
                 base::Passed(&self),
                 completion_callback));
}

// static
void CloudPolicyValidatorBase::ReportCompletion(
    std::unique_ptr<CloudPolicyValidatorBase> self,
    const base::Closure& completion_callback) {
  completion_callback.Run();
}

void CloudPolicyValidatorBase::RunValidation() {
  policy_data_.reset(new em::PolicyData());
  RunChecks();
}

void CloudPolicyValidatorBase::RunChecks() {
  status_ = VALIDATION_OK;
  if ((policy_->has_error_code() && policy_->error_code() != 200) ||
      (policy_->has_error_message() && !policy_->error_message().empty())) {
    LOG(ERROR) << "Error in policy blob."
               << " code: " << policy_->error_code()
               << " message: " << policy_->error_message();
    status_ = VALIDATION_ERROR_CODE_PRESENT;
    return;
  }

  // Parse policy data.
  if (!policy_data_->ParseFromString(policy_->policy_data()) ||
      !policy_data_->IsInitialized()) {
    LOG(ERROR) << "Failed to parse policy response";
    status_ = VALIDATION_PAYLOAD_PARSE_ERROR;
    return;
  }

  // Table of checks we run. These are sorted by descending severity of the
  // error, s.t. the most severe check will determine the validation status.
  static const struct {
    int flag;
    Status (CloudPolicyValidatorBase::* checkFunction)();
  } kCheckFunctions[] = {
    { VALIDATE_SIGNATURE,   &CloudPolicyValidatorBase::CheckSignature },
    { VALIDATE_INITIAL_KEY, &CloudPolicyValidatorBase::CheckInitialKey },
    { VALIDATE_CACHED_KEY,  &CloudPolicyValidatorBase::CheckCachedKey },
    { VALIDATE_POLICY_TYPE, &CloudPolicyValidatorBase::CheckPolicyType },
    { VALIDATE_ENTITY_ID,   &CloudPolicyValidatorBase::CheckEntityId },
    { VALIDATE_TOKEN,       &CloudPolicyValidatorBase::CheckToken },
    { VALIDATE_USERNAME,    &CloudPolicyValidatorBase::CheckUsername },
    { VALIDATE_DOMAIN,      &CloudPolicyValidatorBase::CheckDomain },
    { VALIDATE_TIMESTAMP,   &CloudPolicyValidatorBase::CheckTimestamp },
    { VALIDATE_PAYLOAD,     &CloudPolicyValidatorBase::CheckPayload },
  };

  for (size_t i = 0; i < arraysize(kCheckFunctions); ++i) {
    if (validation_flags_ & kCheckFunctions[i].flag) {
      status_ = (this->*(kCheckFunctions[i].checkFunction))();
      if (status_ != VALIDATION_OK)
        break;
    }
  }
}

// Verifies the |new_public_key_verification_signature| for the |new_public_key|
// in the policy blob.
bool CloudPolicyValidatorBase::CheckNewPublicKeyVerificationSignature() {
  // If there's no local verification key, then just return true (no
  // validation possible).
  if (verification_key_.empty()) {
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicyKeyVerification,
                              METRIC_POLICY_KEY_VERIFICATION_KEY_MISSING,
                              METRIC_POLICY_KEY_VERIFICATION_SIZE);
    return true;
  }

  if (!policy_->has_new_public_key_verification_signature()) {
    // Policy does not contain a verification signature, so log an error.
    LOG(ERROR) << "Policy is missing public_key_verification_signature";
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicyKeyVerification,
                              METRIC_POLICY_KEY_VERIFICATION_SIGNATURE_MISSING,
                              METRIC_POLICY_KEY_VERIFICATION_SIZE);
    return false;
  }

  if (!CheckVerificationKeySignature(
          policy_->new_public_key(),
          verification_key_,
          policy_->new_public_key_verification_signature())) {
    LOG(ERROR) << "Signature verification failed";
    UMA_HISTOGRAM_ENUMERATION(kMetricPolicyKeyVerification,
                              METRIC_POLICY_KEY_VERIFICATION_FAILED,
                              METRIC_POLICY_KEY_VERIFICATION_SIZE);
    return false;
  }
  // Signature verification succeeded - return success to the caller.
  DVLOG(1) << "Signature verification succeeded";
  UMA_HISTOGRAM_ENUMERATION(kMetricPolicyKeyVerification,
                            METRIC_POLICY_KEY_VERIFICATION_SUCCEEDED,
                            METRIC_POLICY_KEY_VERIFICATION_SIZE);
  return true;
}

bool CloudPolicyValidatorBase::CheckVerificationKeySignature(
    const std::string& key,
    const std::string& verification_key,
    const std::string& signature) {
  DCHECK(!verification_key.empty());
  em::PolicyPublicKeyAndDomain signed_data;
  signed_data.set_new_public_key(key);

  // If no owning_domain_ supplied, try extracting the domain from the policy
  // itself (this happens on certain platforms during startup, when we validate
  // cached policy before prefs are loaded).
  std::string domain = owning_domain_.empty() ?
      ExtractDomainFromPolicy() : owning_domain_;
  if (domain.empty()) {
    LOG(ERROR) << "Policy does not contain a domain";
    return false;
  }
  signed_data.set_domain(domain);
  std::string signed_data_as_string;
  if (!signed_data.SerializeToString(&signed_data_as_string)) {
    DLOG(ERROR) << "Could not serialize verification key to string";
    return false;
  }
  return VerifySignature(signed_data_as_string, verification_key, signature,
                         SHA256);
}

std::string CloudPolicyValidatorBase::ExtractDomainFromPolicy() {
  std::string domain;
  if (policy_data_->has_username()) {
    domain = gaia::ExtractDomainName(
        gaia::CanonicalizeEmail(
            gaia::SanitizeEmail(policy_data_->username())));
  }
  return domain;
}

void CloudPolicyValidatorBase::set_verification_key_and_domain(
    const std::string& verification_key, const std::string& owning_domain) {
  // Make sure we aren't overwriting the verification key with a different key.
  DCHECK(verification_key_.empty() || verification_key_ == verification_key);
  DCHECK(owning_domain_.empty() || owning_domain_ == owning_domain);
  verification_key_ = verification_key;
  owning_domain_ = owning_domain;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckSignature() {
  const std::string* signature_key = &key_;
  if (policy_->has_new_public_key() && allow_key_rotation_) {
    signature_key = &policy_->new_public_key();
    if (!policy_->has_new_public_key_signature() ||
        !VerifySignature(policy_->new_public_key(), key_,
                         policy_->new_public_key_signature(), SHA1)) {
      LOG(ERROR) << "New public key rotation signature verification failed";
      return VALIDATION_BAD_SIGNATURE;
    }

    if (!CheckNewPublicKeyVerificationSignature()) {
      LOG(ERROR) << "New public key root verification failed";
      return VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE;
    }
  }

  if (!policy_->has_policy_data_signature() ||
      !VerifySignature(policy_->policy_data(), *signature_key,
                       policy_->policy_data_signature(), SHA1)) {
    LOG(ERROR) << "Policy signature validation failed";
    return VALIDATION_BAD_SIGNATURE;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckInitialKey() {
  if (!policy_->has_new_public_key() ||
      !policy_->has_policy_data_signature() ||
      !VerifySignature(policy_->policy_data(), policy_->new_public_key(),
                       policy_->policy_data_signature(), SHA1)) {
    LOG(ERROR) << "Initial policy signature validation failed";
    return VALIDATION_BAD_INITIAL_SIGNATURE;
  }

  if (!CheckNewPublicKeyVerificationSignature()) {
    LOG(ERROR) << "Initial policy root signature validation failed";
    return VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE;
  }
  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckCachedKey() {
  if (!verification_key_.empty() &&
      !CheckVerificationKeySignature(cached_key_, verification_key_,
                                     cached_key_signature_)) {
    LOG(ERROR) << "Cached key signature verification failed";
    return VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE;
  } else {
    DVLOG(1) << "Cached key signature verification succeeded";
  }
  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckPolicyType() {
  if (!policy_data_->has_policy_type() ||
       policy_data_->policy_type() != policy_type_) {
    LOG(ERROR) << "Wrong policy type " << policy_data_->policy_type();
    return VALIDATION_WRONG_POLICY_TYPE;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckEntityId() {
  if (!policy_data_->has_settings_entity_id() ||
      policy_data_->settings_entity_id() != settings_entity_id_) {
    LOG(ERROR) << "Wrong settings_entity_id "
               << policy_data_->settings_entity_id() << ", expected "
               << settings_entity_id_;
    return VALIDATION_WRONG_SETTINGS_ENTITY_ID;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckTimestamp() {
  if (timestamp_option_ == TIMESTAMP_NOT_REQUIRED)
    return VALIDATION_OK;

  if (!policy_data_->has_timestamp()) {
    LOG(ERROR) << "Policy timestamp missing";
    return VALIDATION_BAD_TIMESTAMP;
  }

  if (policy_data_->timestamp() < timestamp_not_before_) {
    LOG(ERROR) << "Policy too old: " << policy_data_->timestamp();
    return VALIDATION_BAD_TIMESTAMP;
  }

  // Limit the damage in case of an unlikely server bug: If the server
  // accidentally sends a time from the distant future, this time is stored
  // locally and after the server time is corrected, due to rollback prevention
  // the client could not receive policy updates until that future date.
  if (timestamp_option_ == TIMESTAMP_REQUIRED &&
      policy_data_->timestamp() > timestamp_not_after_) {
    LOG(ERROR) << "Policy from the future: " << policy_data_->timestamp();
    return VALIDATION_BAD_TIMESTAMP;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckToken() {
  // Make sure the token matches the expected token (if any) and also
  // make sure the token itself is valid (non-empty if DM_TOKEN_REQUIRED).
  if (dm_token_option_ == DM_TOKEN_REQUIRED &&
      (!policy_data_->has_request_token() ||
       policy_data_->request_token().empty())) {
    LOG(ERROR) << "Empty DM token encountered - expected: " << token_;
    return VALIDATION_WRONG_TOKEN;
  }
  if (!token_.empty() && policy_data_->request_token() != token_) {
    LOG(ERROR) << "Invalid DM token: " << policy_data_->request_token()
               << " - expected: " << token_;
    return VALIDATION_WRONG_TOKEN;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckUsername() {
  if (!policy_data_->has_username()) {
    LOG(ERROR) << "Policy is missing user name";
    return VALIDATION_BAD_USERNAME;
  }

  std::string expected = user_;
  std::string actual = policy_data_->username();
  if (canonicalize_user_) {
    expected = gaia::CanonicalizeEmail(gaia::SanitizeEmail(expected));
    actual = gaia::CanonicalizeEmail(gaia::SanitizeEmail(actual));
  }

  if (expected != actual) {
    LOG(ERROR) << "Invalid user name " << policy_data_->username();
    return VALIDATION_BAD_USERNAME;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckDomain() {
  std::string policy_domain = ExtractDomainFromPolicy();
  if (policy_domain.empty()) {
    LOG(ERROR) << "Policy is missing user name";
    return VALIDATION_BAD_USERNAME;
  }

  if (domain_ != policy_domain) {
    LOG(ERROR) << "Invalid user name " << policy_data_->username();
    return VALIDATION_BAD_USERNAME;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckPayload() {
  if (!policy_data_->has_policy_value() ||
      !payload_->ParseFromString(policy_data_->policy_value()) ||
      !payload_->IsInitialized()) {
    LOG(ERROR) << "Failed to decode policy payload protobuf";
    return VALIDATION_POLICY_PARSE_ERROR;
  }

  return VALIDATION_OK;
}

// static
bool CloudPolicyValidatorBase::VerifySignature(const std::string& data,
                                               const std::string& key,
                                               const std::string& signature,
                                               SignatureType signature_type) {
  crypto::SignatureVerifier verifier;
  crypto::SignatureVerifier::SignatureAlgorithm algorithm;
  switch (signature_type) {
    case SHA1:
      algorithm = crypto::SignatureVerifier::RSA_PKCS1_SHA1;
      break;
    case SHA256:
      algorithm = crypto::SignatureVerifier::RSA_PKCS1_SHA256;
      break;
    default:
      NOTREACHED() << "Invalid signature type: " << signature_type;
      return false;
  }

  if (!verifier.VerifyInit(
          algorithm, reinterpret_cast<const uint8_t*>(signature.c_str()),
          signature.size(), reinterpret_cast<const uint8_t*>(key.c_str()),
          key.size())) {
    DLOG(ERROR) << "Invalid verification signature/key format";
    return false;
  }
  verifier.VerifyUpdate(reinterpret_cast<const uint8_t*>(data.c_str()),
                        data.size());
  return verifier.VerifyFinal();
}

template class CloudPolicyValidator<em::CloudPolicySettings>;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
template class CloudPolicyValidator<em::ExternalPolicyData>;
#endif

}  // namespace policy
