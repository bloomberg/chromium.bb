// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_VALIDATOR_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_VALIDATOR_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"

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
// Since signature checks are expensive, validation is happening on the FILE
// thread. The pattern is to create a validator, configure its behavior through
// the ValidateXYZ() functions, and then call StartValidation().
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
  // |not_before| and not after |now| + grace interval.
  void ValidateTimestamp(base::Time not_before, base::Time now);

  // Validates the username in the policy blob matches |expected_user|.
  void ValidateUsername(const std::string& expected_user);

  // Validates the policy blob is addressed to |expected_domain|. This uses the
  // domain part of the username field in the policy for the check.
  void ValidateDomain(const std::string& expected_domain);

  // Makes sure the DM token on the policy matches |expected_token|.
  void ValidateDMToken(const std::string& dm_token);

  // Validates the policy type.
  void ValidatePolicyType(const std::string& policy_type);

  // Validates that the payload can be decoded successfully.
  void ValidatePayload();

  // Verifies that the signature on the policy blob verifies against |key|. If
  // there is a key rotation present in the policy blob, this checks the
  // signature on the new key against |key| and the policy blob against the new
  // key.
  void ValidateSignature(const std::string& key);

  // Similar to StartSignatureVerification(), this checks the signature on the
  // policy blob. However, this variant expects a new policy key set in the
  // policy blob and makes sure the policy is signed using that key. This should
  // be called at setup time when there is no existing policy key present to
  // check against.
  void ValidateInitialKey();

  // Convenience helper that configures timestamp and token validation based on
  // the current policy blob. |policy_data| may be NULL, in which case the
  // timestamp validation will drop the lower bound and no token validation will
  // be configured.
  void ValidateAgainstCurrentPolicy(
      const enterprise_management::PolicyData* policy_data);

  // Kicks off validation. From this point on, the validator manages its own
  // lifetime. |completion_callback| is invoked when done.
  void StartValidation();

 protected:
  // Create a new validator that checks |policy_response|. |payload| is the
  // message that the policy payload will be parsed to, and it needs to stay
  // valid for the lifetime of the validator.
  CloudPolicyValidatorBase(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy_response,
      google::protobuf::MessageLite* payload,
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

  // Performs validation, called on a background thread.
  static void PerformValidation(
      scoped_ptr<CloudPolicyValidatorBase> self,
      scoped_refptr<base::MessageLoopProxy> message_loop);

  // Reports completion to |self|'s |completion_callback_|.
  static void ReportCompletion(scoped_ptr<CloudPolicyValidatorBase> self);

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
  base::Closure completion_callback_;

  int validation_flags_;
  base::Time timestamp_not_before_;
  base::Time timestamp_not_after_;
  std::string user_;
  std::string domain_;
  std::string token_;
  std::string policy_type_;
  std::string key_;

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
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy_response,
      const CompletionCallback& completion_callback);

  scoped_ptr<PayloadProto>& payload() {
    return payload_;
  }

 private:
  CloudPolicyValidator(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy_response,
      scoped_ptr<PayloadProto> payload,
      const CompletionCallback& completion_callback);

  scoped_ptr<PayloadProto> payload_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyValidator);
};

typedef CloudPolicyValidator<enterprise_management::ChromeDeviceSettingsProto>
    DeviceCloudPolicyValidator;
typedef CloudPolicyValidator<enterprise_management::CloudPolicySettings>
    UserCloudPolicyValidator;

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_VALIDATOR_H_
