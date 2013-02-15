// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_VALIDATOR_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_VALIDATOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"

namespace base {
class MessageLoopProxy;
}

namespace google {
namespace protobuf {
class MessageLite;
}
}

namespace enterprise_management {
class ChromeDeviceSettingsProto;
class CloudPolicySettings;
class PolicyData;
class PolicyFetchResponse;
}

namespace policy {

// Helper class that implements the gory details of validating a policy blob.
// Since signature checks are expensive, validation can happen on the FILE
// thread. The pattern is to create a validator, configure its behavior through
// the ValidateXYZ() functions, and then call StartValidation(). Alternatively,
// RunValidation() can be used to perform validation on the current thread.
class CloudPolicyValidatorBase {
 public:
  // Validation result codes.
  enum Status {
    // Indicates successful validation.
    VALIDATION_OK,
    // Bad signature.
    VALIDATION_BAD_SIGNATURE,
    // Policy blob contains error code.
    VALIDATION_ERROR_CODE_PRESENT,
    // Policy payload failed to decode.
    VALIDATION_PAYLOAD_PARSE_ERROR,
    // Unexpected policy type.
    VALIDATION_WRONG_POLICY_TYPE,
    // Time stamp from the future.
    VALIDATION_BAD_TIMESTAMP,
    // Token doesn't match.
    VALIDATION_WRONG_TOKEN,
    // Username doesn't match.
    VALIDATION_BAD_USERNAME,
    // Policy payload protobuf parse error.
    VALIDATION_POLICY_PARSE_ERROR,
  };

  enum ValidateDMTokenOption {
    // The policy must have a non-empty DMToken.
    DM_TOKEN_REQUIRED,

    // The policy may have an empty or missing DMToken, if the expected token
    // is also empty.
    DM_TOKEN_NOT_REQUIRED,
  };

  enum ValidateTimestampOption {
    // The policy must have a timestamp field.
    TIMESTAMP_REQUIRED,

    // No timestamp field is required.
    TIMESTAMP_NOT_REQUIRED,
  };

  virtual ~CloudPolicyValidatorBase();

  // Validation status which can be read after completion has been signaled.
  Status status() const { return status_; }
  bool success() const { return status_ == VALIDATION_OK; }

  // The policy objects owned by the validator. These are scoped_ptr
  // references, so ownership can be passed on once validation is complete.
  scoped_ptr<enterprise_management::PolicyFetchResponse>& policy() {
    return policy_;
  }
  scoped_ptr<enterprise_management::PolicyData>& policy_data() {
    return policy_data_;
  }

  // Instructs the validator to check that the policy timestamp is not before
  // |not_before| and not after |now| + grace interval. If
  // |timestamp_option| is set to TIMESTAMP_REQUIRED, then the policy will fail
  // validation if it does not have a timestamp field.
  void ValidateTimestamp(base::Time not_before,
                         base::Time now,
                         ValidateTimestampOption timestamp_option);

  // Validates the username in the policy blob matches |expected_user|.
  void ValidateUsername(const std::string& expected_user);

  // Validates the policy blob is addressed to |expected_domain|. This uses the
  // domain part of the username field in the policy for the check.
  void ValidateDomain(const std::string& expected_domain);

  // Makes sure the DM token on the policy matches |expected_token|.
  // If |dm_token_option| is DM_TOKEN_REQUIRED, then the policy will fail
  // validation if it does not have a non-empty request_token field.
  void ValidateDMToken(const std::string& dm_token,
                       ValidateDMTokenOption dm_token_option);

  // Validates the policy type.
  void ValidatePolicyType(const std::string& policy_type);

  // Validates that the payload can be decoded successfully.
  void ValidatePayload();

  // Verifies that the signature on the policy blob verifies against |key|. If |
  // |allow_key_rotation| is true and there is a key rotation present in the
  // policy blob, this checks the signature on the new key against |key| and the
  // policy blob against the new key.
  void ValidateSignature(const std::vector<uint8>& key,
                         bool allow_key_rotation);

  // Similar to StartSignatureVerification(), this checks the signature on the
  // policy blob. However, this variant expects a new policy key set in the
  // policy blob and makes sure the policy is signed using that key. This should
  // be called at setup time when there is no existing policy key present to
  // check against.
  void ValidateInitialKey();

  // Convenience helper that configures timestamp and token validation based on
  // the current policy blob. |policy_data| may be NULL, in which case the
  // timestamp validation will drop the lower bound. |dm_token_option|
  // and |timestamp_option| have the same effect as the corresponding
  // parameters for ValidateTimestamp() and ValidateDMToken().
  void ValidateAgainstCurrentPolicy(
      const enterprise_management::PolicyData* policy_data,
      ValidateTimestampOption timestamp_option,
      ValidateDMTokenOption dm_token_option);

  // Immediately performs validation on the current thread.
  void RunValidation();

 protected:
  // Create a new validator that checks |policy_response|. |payload| is the
  // message that the policy payload will be parsed to, and it needs to stay
  // valid for the lifetime of the validator.
  CloudPolicyValidatorBase(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy_response,
      google::protobuf::MessageLite* payload);

  // Performs validation, called on a background thread.
  static void PerformValidation(
      scoped_ptr<CloudPolicyValidatorBase> self,
      scoped_refptr<base::MessageLoopProxy> message_loop,
      const base::Closure& completion_callback);

 private:
  // Internal flags indicating what to check.
  enum ValidationFlags {
    VALIDATE_TIMESTAMP   = 1 << 0,
    VALIDATE_USERNAME    = 1 << 1,
    VALIDATE_DOMAIN      = 1 << 2,
    VALIDATE_TOKEN       = 1 << 3,
    VALIDATE_POLICY_TYPE = 1 << 4,
    VALIDATE_PAYLOAD     = 1 << 5,
    VALIDATE_SIGNATURE   = 1 << 6,
    VALIDATE_INITIAL_KEY = 1 << 7,
  };

  // Reports completion to the |completion_callback_|.
  static void ReportCompletion(scoped_ptr<CloudPolicyValidatorBase> self,
                               const base::Closure& completion_callback);

  // Invokes all the checks and reports the result.
  void RunChecks();

  // Helper functions implementing individual checks.
  Status CheckTimestamp();
  Status CheckUsername();
  Status CheckDomain();
  Status CheckToken();
  Status CheckPolicyType();
  Status CheckPayload();
  Status CheckSignature();
  Status CheckInitialKey();

  // Verifies the SHA1/RSA |signature| on |data| against |key|.
  static bool VerifySignature(const std::string& data,
                              const std::string& key,
                              const std::string& signature);

  Status status_;
  scoped_ptr<enterprise_management::PolicyFetchResponse> policy_;
  scoped_ptr<enterprise_management::PolicyData> policy_data_;
  google::protobuf::MessageLite* payload_;

  int validation_flags_;
  int64 timestamp_not_before_;
  int64 timestamp_not_after_;
  ValidateTimestampOption timestamp_option_;
  ValidateDMTokenOption dm_token_option_;
  std::string user_;
  std::string domain_;
  std::string token_;
  std::string policy_type_;
  std::string key_;
  bool allow_key_rotation_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyValidatorBase);
};

// A simple type-parameterized extension of CloudPolicyValidator that
// facilitates working with the actual protobuf payload type.
template<typename PayloadProto>
class CloudPolicyValidator : public CloudPolicyValidatorBase {
 public:
  typedef base::Callback<void(CloudPolicyValidator<PayloadProto>*)>
      CompletionCallback;

  virtual ~CloudPolicyValidator();

  // Creates a new validator.
  static CloudPolicyValidator<PayloadProto>* Create(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy_response);

  scoped_ptr<PayloadProto>& payload() {
    return payload_;
  }

  // Kicks off asynchronous validation. |completion_callback| is invoked when
  // done. From this point on, the validator manages its own lifetime - this
  // allows callers to provide a WeakPtr in the callback without leaking the
  // validator.
  void StartValidation(const CompletionCallback& completion_callback);

 private:
  CloudPolicyValidator(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy_response,
      scoped_ptr<PayloadProto> payload);

  scoped_ptr<PayloadProto> payload_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyValidator);
};

typedef CloudPolicyValidator<enterprise_management::ChromeDeviceSettingsProto>
    DeviceCloudPolicyValidator;
typedef CloudPolicyValidator<enterprise_management::CloudPolicySettings>
    UserCloudPolicyValidator;

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_VALIDATOR_H_
