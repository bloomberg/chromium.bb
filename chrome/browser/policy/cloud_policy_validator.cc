// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_validator.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/signature_verifier.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Grace interval for policy timestamp checks, in seconds.
const int kTimestampGraceIntervalSeconds = 60;

// DER-encoded ASN.1 object identifier for the SHA1-RSA signature algorithm.
const uint8 kSignatureAlgorithm[] = {
    0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
    0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00
};

}  // namespace

CloudPolicyValidatorBase::~CloudPolicyValidatorBase() {}

void CloudPolicyValidatorBase::ValidateTimestamp(
    base::Time not_before,
    base::Time now,
    ValidateTimestampOption timestamp_option) {
  // Timestamp should be from the past. We allow for a 1-minute grace interval
  // to cover clock drift.
  validation_flags_ |= VALIDATE_TIMESTAMP;
  timestamp_not_before_ =
      (not_before - base::Time::UnixEpoch()).InMilliseconds();
  timestamp_not_after_ =
      ((now + base::TimeDelta::FromSeconds(kTimestampGraceIntervalSeconds)) -
          base::Time::UnixEpoch()).InMillisecondsRoundedUp();
  timestamp_option_ = timestamp_option;
}

void CloudPolicyValidatorBase::ValidateUsername(
    const std::string& expected_user) {
  validation_flags_ |= VALIDATE_USERNAME;
  user_ = gaia::CanonicalizeEmail(expected_user);
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

void CloudPolicyValidatorBase::ValidatePayload() {
  validation_flags_ |= VALIDATE_PAYLOAD;
}

void CloudPolicyValidatorBase::ValidateSignature(const std::vector<uint8>& key,
                                                 bool allow_key_rotation) {
  validation_flags_ |= VALIDATE_SIGNATURE;
  key_ = std::string(reinterpret_cast<const char*>(vector_as_array(&key)),
                     key.size());
  allow_key_rotation_ = allow_key_rotation;
}

void CloudPolicyValidatorBase::ValidateInitialKey() {
  validation_flags_ |= VALIDATE_INITIAL_KEY;
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
    scoped_ptr<em::PolicyFetchResponse> policy_response,
    google::protobuf::MessageLite* payload)
    : status_(VALIDATION_OK),
      policy_(policy_response.Pass()),
      payload_(payload),
      validation_flags_(0),
      timestamp_not_before_(0),
      timestamp_not_after_(0),
      timestamp_option_(TIMESTAMP_REQUIRED),
      dm_token_option_(DM_TOKEN_REQUIRED),
      allow_key_rotation_(false) {}

// static
void CloudPolicyValidatorBase::PerformValidation(
    scoped_ptr<CloudPolicyValidatorBase> self,
    scoped_refptr<base::MessageLoopProxy> message_loop,
    const base::Closure& completion_callback) {
  // Run the validation activities on this thread.
  self->RunValidation();

  // Report completion on |message_loop|.
  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&CloudPolicyValidatorBase::ReportCompletion,
                 base::Passed(&self),
                 completion_callback));
}

// static
void CloudPolicyValidatorBase::ReportCompletion(
    scoped_ptr<CloudPolicyValidatorBase> self,
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
    { VALIDATE_POLICY_TYPE, &CloudPolicyValidatorBase::CheckPolicyType },
    { VALIDATE_TOKEN,       &CloudPolicyValidatorBase::CheckToken },
    { VALIDATE_USERNAME,    &CloudPolicyValidatorBase::CheckUsername },
    { VALIDATE_DOMAIN,      &CloudPolicyValidatorBase::CheckDomain },
    { VALIDATE_TIMESTAMP,   &CloudPolicyValidatorBase::CheckTimestamp },
    { VALIDATE_PAYLOAD,     &CloudPolicyValidatorBase::CheckPayload },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kCheckFunctions); ++i) {
    if (validation_flags_ & kCheckFunctions[i].flag) {
      status_ = (this->*(kCheckFunctions[i].checkFunction))();
      if (status_ != VALIDATION_OK)
        break;
    }
  }
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckSignature() {
  const std::string* signature_key = &key_;
  if (policy_->has_new_public_key() && allow_key_rotation_) {
    signature_key = &policy_->new_public_key();
    if (!policy_->has_new_public_key_signature() ||
        !VerifySignature(policy_->new_public_key(), key_,
                         policy_->new_public_key_signature())) {
      LOG(ERROR) << "New public key signature verification failed";
      return VALIDATION_BAD_SIGNATURE;
    }
  }

  if (!policy_->has_policy_data_signature() ||
      !VerifySignature(policy_->policy_data(), *signature_key,
                       policy_->policy_data_signature())) {
    LOG(ERROR) << "Policy signature validation failed";
    return VALIDATION_BAD_SIGNATURE;
  }

  return VALIDATION_OK;
}

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckInitialKey() {
  if (!policy_->has_new_public_key() ||
      !policy_->has_policy_data_signature() ||
      !VerifySignature(policy_->policy_data(), policy_->new_public_key(),
                       policy_->policy_data_signature())) {
    LOG(ERROR) << "Initial policy signature validation failed";
    return VALIDATION_BAD_SIGNATURE;
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

CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckTimestamp() {
  if (!policy_data_->has_timestamp()) {
    if (timestamp_option_ == TIMESTAMP_NOT_REQUIRED) {
      return VALIDATION_OK;
    } else {
      LOG(ERROR) << "Policy timestamp missing";
      return VALIDATION_BAD_TIMESTAMP;
    }
  }

  if (policy_data_->timestamp() < timestamp_not_before_) {
    LOG(ERROR) << "Policy too old: " << policy_data_->timestamp();
    return VALIDATION_BAD_TIMESTAMP;
  }
  if (policy_data_->timestamp() > timestamp_not_after_) {
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

  std::string policy_username =
      gaia::CanonicalizeEmail(gaia::SanitizeEmail(policy_data_->username()));

  if (user_ != policy_username) {
    LOG(ERROR) << "Invalid user name " << policy_data_->username();
    return VALIDATION_BAD_USERNAME;
  }

  return VALIDATION_OK;
}


CloudPolicyValidatorBase::Status CloudPolicyValidatorBase::CheckDomain() {
  if (!policy_data_->has_username()) {
    LOG(ERROR) << "Policy is missing user name";
    return VALIDATION_BAD_USERNAME;
  }

  std::string policy_domain =
      gaia::ExtractDomainName(
          gaia::CanonicalizeEmail(
              gaia::SanitizeEmail(policy_data_->username())));

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
                                               const std::string& signature) {
  crypto::SignatureVerifier verifier;

  if (!verifier.VerifyInit(kSignatureAlgorithm, sizeof(kSignatureAlgorithm),
                           reinterpret_cast<const uint8*>(signature.c_str()),
                           signature.size(),
                           reinterpret_cast<const uint8*>(key.c_str()),
                           key.size())) {
    return false;
  }
  verifier.VerifyUpdate(reinterpret_cast<const uint8*>(data.c_str()),
                        data.size());
  return verifier.VerifyFinal();
}

template<typename PayloadProto>
CloudPolicyValidator<PayloadProto>::~CloudPolicyValidator() {}

template<typename PayloadProto>
CloudPolicyValidator<PayloadProto>* CloudPolicyValidator<PayloadProto>::Create(
    scoped_ptr<em::PolicyFetchResponse> policy_response) {
  return new CloudPolicyValidator(
      policy_response.Pass(),
      scoped_ptr<PayloadProto>(new PayloadProto()));
}

template<typename PayloadProto>
CloudPolicyValidator<PayloadProto>::CloudPolicyValidator(
    scoped_ptr<em::PolicyFetchResponse> policy_response,
    scoped_ptr<PayloadProto> payload)
    : CloudPolicyValidatorBase(policy_response.Pass(), payload.get()),
      payload_(payload.Pass()) {}

template<typename PayloadProto>
void CloudPolicyValidator<PayloadProto>::StartValidation(
    const CompletionCallback& completion_callback) {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&CloudPolicyValidatorBase::PerformValidation,
                 base::Passed(scoped_ptr<CloudPolicyValidatorBase>(this)),
                 MessageLoop::current()->message_loop_proxy(),
                 base::Bind(completion_callback, this)));
}

template class CloudPolicyValidator<em::ChromeDeviceSettingsProto>;
template class CloudPolicyValidator<em::CloudPolicySettings>;

}  // namespace policy
