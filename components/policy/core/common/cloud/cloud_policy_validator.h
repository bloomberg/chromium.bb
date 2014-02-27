// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_VALIDATOR_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_VALIDATOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/policy/policy_export.h"
#include "policy/proto/cloud_policy.pb.h"

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "policy/proto/chrome_extension_policy.pb.h"
#endif

namespace base {
class MessageLoopProxy;
}

namespace google {
namespace protobuf {
class MessageLite;
}
}

namespace enterprise_management {
class PolicyData;
class PolicyFetchResponse;
}

namespace policy {

// Helper class that implements the gory details of validating a policy blob.
// Since signature checks are expensive, validation can happen on a background
// thread. The pattern is to create a validator, configure its behavior through
// the ValidateXYZ() functions, and then call StartValidation(). Alternatively,
// RunValidation() can be used to perform validation on the current thread.
class POLICY_EXPORT CloudPolicyValidatorBase {
 public:
  // Validation result codes. These values are also used for UMA histograms by
  // UserCloudPolicyStoreChromeOS and must stay stable - new elements should
  // be added at the end before VALIDATION_STATUS_SIZE.
  enum Status {
    // Indicates successful validation.
    VALIDATION_OK,
    // Bad signature on the initial key.
    VALIDATION_BAD_INITIAL_SIGNATURE,
    // Bad signature.
    VALIDATION_BAD_SIGNATURE,
    // Policy blob contains error code.
    VALIDATION_ERROR_CODE_PRESENT,
    // Policy payload failed to decode.
    VALIDATION_PAYLOAD_PARSE_ERROR,
    // Unexpected policy type.
    VALIDATION_WRONG_POLICY_TYPE,
    // Unexpected settings entity id.
    VALIDATION_WRONG_SETTINGS_ENTITY_ID,
    // Time stamp from the future.
    VALIDATION_BAD_TIMESTAMP,
    // Token doesn't match.
    VALIDATION_WRONG_TOKEN,
    // Username doesn't match.
    VALIDATION_BAD_USERNAME,
    // Policy payload protobuf parse error.
    VALIDATION_POLICY_PARSE_ERROR,
    // Policy key signature could not be verified using the hard-coded
    // verification key.
    VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE,
    VALIDATION_STATUS_SIZE  // MUST BE LAST
  };

  enum ValidateDMTokenOption {
    // The policy must have a non-empty DMToken.
    DM_TOKEN_REQUIRED,

    // The policy may have an empty or missing DMToken, if the expected token
    // is also empty.
    DM_TOKEN_NOT_REQUIRED,
  };

  enum ValidateTimestampOption {
    // The policy must have a timestamp field and it should be checked against
    // both the start and end times.
    TIMESTAMP_REQUIRED,

    // The timestamp should only be compared vs the |not_before| value (this
    // is appropriate for platforms with unreliable system times, where we want
    // to ensure that fresh policy is newer than existing policy, but we can't
    // do any other validation).
    TIMESTAMP_NOT_BEFORE,

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
  // |not_before| and not after |not_after| + grace interval. If
  // |timestamp_option| is set to TIMESTAMP_REQUIRED, then the policy will fail
  // validation if it does not have a timestamp field.
  void ValidateTimestamp(base::Time not_before,
                         base::Time not_after,
                         ValidateTimestampOption timestamp_option);

  // Validates that the username in the policy blob matches |expected_user|. If
  // canonicalize is set to true, both values will be canonicalized before
  // comparison.
  void ValidateUsername(const std::string& expected_user, bool canonicalize);

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

  // Validates the settings_entity_id value.
  void ValidateSettingsEntityId(const std::string& settings_entity_id);

  // Validates that the payload can be decoded successfully.
  void ValidatePayload();

  // Verifies that |cached_key| is valid, by verifying the
  // |cached_key_signature| using the passed |owning_domain| and
  // |verification_key|.
  void ValidateCachedKey(const std::string& cached_key,
                         const std::string& cached_key_signature,
                         const std::string& verification_key,
                         const std::string& owning_domain);

  // Verifies that the signature on the policy blob verifies against |key|. If
  // |allow_key_rotation| is true and there is a key rotation present in the
  // policy blob, this checks the signature on the new key against |key| and the
  // policy blob against the new key. New key is also validated using the passed
  // |verification_key| and |owning_domain|, and the
  // |new_public_key_verification_signature| field.
  void ValidateSignature(const std::string& key,
                         const std::string& verification_key,
                         const std::string& owning_domain,
                         bool allow_key_rotation);

  // Similar to ValidateSignature(), this checks the signature on the
  // policy blob. However, this variant expects a new policy key set in the
  // policy blob and makes sure the policy is signed using that key. This should
  // be called at setup time when there is no existing policy key present to
  // check against. New key is validated using the passed |verification_key| and
  // the new_public_key_verification_signature field.
  void ValidateInitialKey(const std::string& verification_key,
                          const std::string& owning_domain);

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
      google::protobuf::MessageLite* payload,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  // Posts an asynchronous calls to PerformValidation, which will eventually
  // report its result via |completion_callback|.
  void PostValidationTask(const base::Closure& completion_callback);

 private:
  // Internal flags indicating what to check.
  enum ValidationFlags {
    VALIDATE_TIMESTAMP   = 1 << 0,
    VALIDATE_USERNAME    = 1 << 1,
    VALIDATE_DOMAIN      = 1 << 2,
    VALIDATE_TOKEN       = 1 << 3,
    VALIDATE_POLICY_TYPE = 1 << 4,
    VALIDATE_ENTITY_ID   = 1 << 5,
    VALIDATE_PAYLOAD     = 1 << 6,
    VALIDATE_SIGNATURE   = 1 << 7,
    VALIDATE_INITIAL_KEY = 1 << 8,
    VALIDATE_CACHED_KEY  = 1 << 9,
  };

  enum SignatureType {
    SHA1,
    SHA256
  };

  // Performs validation, called on a background thread.
  static void PerformValidation(
      scoped_ptr<CloudPolicyValidatorBase> self,
      scoped_refptr<base::MessageLoopProxy> message_loop,
      const base::Closure& completion_callback);

  // Reports completion to the |completion_callback_|.
  static void ReportCompletion(scoped_ptr<CloudPolicyValidatorBase> self,
                               const base::Closure& completion_callback);

  // Invokes all the checks and reports the result.
  void RunChecks();

  // Helper routine that verifies that the new public key in the policy blob
  // is properly signed by the |verification_key_|.
  bool CheckNewPublicKeyVerificationSignature();

  // Helper routine that performs a verification-key-based signature check,
  // which includes the domain name associated with this policy. Returns true
  // if the verification succeeds, or if |signature| is empty.
  bool CheckVerificationKeySignature(const std::string& key_to_verify,
                                     const std::string& server_key,
                                     const std::string& signature);

  // Returns the domain name from the policy being validated. Returns an
  // empty string if the policy does not contain a username field.
  std::string ExtractDomainFromPolicy();

  // Sets the key and domain used to verify new public keys, and ensures that
  // callers don't try to set conflicting values.
  void set_verification_key_and_domain(const std::string& verification_key,
                                       const std::string& owning_domain);

  // Helper functions implementing individual checks.
  Status CheckTimestamp();
  Status CheckUsername();
  Status CheckDomain();
  Status CheckToken();
  Status CheckPolicyType();
  Status CheckEntityId();
  Status CheckPayload();
  Status CheckSignature();
  Status CheckInitialKey();
  Status CheckCachedKey();

  // Verifies the SHA1/ or SHA256/RSA |signature| on |data| against |key|.
  // |signature_type| specifies the type of signature (SHA1 or SHA256).
  static bool VerifySignature(const std::string& data,
                              const std::string& key,
                              const std::string& signature,
                              SignatureType signature_type);

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
  bool canonicalize_user_;
  std::string domain_;
  std::string token_;
  std::string policy_type_;
  std::string settings_entity_id_;
  std::string key_;
  std::string cached_key_;
  std::string cached_key_signature_;
  std::string verification_key_;
  std::string owning_domain_;
  bool allow_key_rotation_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyValidatorBase);
};

// A simple type-parameterized extension of CloudPolicyValidator that
// facilitates working with the actual protobuf payload type.
template<typename PayloadProto>
class POLICY_EXPORT CloudPolicyValidator : public CloudPolicyValidatorBase {
 public:
  typedef base::Callback<void(CloudPolicyValidator<PayloadProto>*)>
      CompletionCallback;

  virtual ~CloudPolicyValidator() {}

  // Creates a new validator.
  // |background_task_runner| is optional; if RunValidation() is used directly
  // and StartValidation() is not used then it can be NULL.
  static CloudPolicyValidator<PayloadProto>* Create(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy_response,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
    return new CloudPolicyValidator(
        policy_response.Pass(),
        scoped_ptr<PayloadProto>(new PayloadProto()),
        background_task_runner);
  }

  scoped_ptr<PayloadProto>& payload() {
    return payload_;
  }

  // Kicks off asynchronous validation. |completion_callback| is invoked when
  // done. From this point on, the validator manages its own lifetime - this
  // allows callers to provide a WeakPtr in the callback without leaking the
  // validator.
  void StartValidation(const CompletionCallback& completion_callback) {
    PostValidationTask(base::Bind(completion_callback, this));
  }

 private:
  CloudPolicyValidator(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy_response,
      scoped_ptr<PayloadProto> payload,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner)
      : CloudPolicyValidatorBase(policy_response.Pass(),
                                 payload.get(),
                                 background_task_runner),
        payload_(payload.Pass()) {}

  scoped_ptr<PayloadProto> payload_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyValidator);
};

typedef CloudPolicyValidator<enterprise_management::CloudPolicySettings>
    UserCloudPolicyValidator;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
typedef CloudPolicyValidator<enterprise_management::ExternalPolicyData>
    ComponentCloudPolicyValidator;
#endif

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_VALIDATOR_H_
