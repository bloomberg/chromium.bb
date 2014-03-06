// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_cloud_policy_store.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/task_runner_util.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "policy/proto/cloud_policy.pb.h"
#include "policy/proto/device_management_backend.pb.h"
#include "policy/proto/policy_signing_key.pb.h"

namespace em = enterprise_management;

namespace policy {

enum PolicyLoadStatus {
  // Policy blob was successfully loaded and parsed.
  LOAD_RESULT_SUCCESS,

  // No previously stored policy was found.
  LOAD_RESULT_NO_POLICY_FILE,

  // Could not load the previously stored policy due to either a parse or
  // file read error.
  LOAD_RESULT_LOAD_ERROR,
};

// Struct containing the result of a policy load - if |status| ==
// LOAD_RESULT_SUCCESS, |policy| is initialized from the policy file on disk.
struct PolicyLoadResult {
  PolicyLoadStatus status;
  em::PolicyFetchResponse policy;
  em::PolicySigningKey key;
};

namespace {

// Subdirectory in the user's profile for storing user policies.
const base::FilePath::CharType kPolicyDir[] = FILE_PATH_LITERAL("Policy");
// File in the above directory for storing user policy data.
const base::FilePath::CharType kPolicyCacheFile[] =
    FILE_PATH_LITERAL("User Policy");

// File in the above directory for storing policy signing key data.
const base::FilePath::CharType kKeyCacheFile[] =
    FILE_PATH_LITERAL("Signing Key");

const char kMetricPolicyHasVerifiedCachedKey[] =
    "Enterprise.PolicyHasVerifiedCachedKey";

// Maximum policy and key size that will be loaded, in bytes.
const size_t kPolicySizeLimit = 1024 * 1024;
const size_t kKeySizeLimit = 16 * 1024;

// Loads policy from the backing file. Returns a PolicyLoadResult with the
// results of the fetch.
policy::PolicyLoadResult LoadPolicyFromDisk(
    const base::FilePath& policy_path,
    const base::FilePath& key_path) {
  policy::PolicyLoadResult result;
  // If the backing file does not exist, just return. We don't verify the key
  // path here, because the key is optional (the validation code will fail if
  // the key does not exist but the loaded policy is unsigned).
  if (!base::PathExists(policy_path)) {
    result.status = policy::LOAD_RESULT_NO_POLICY_FILE;
    return result;
  }
  std::string data;

  if (!base::ReadFileToString(policy_path, &data, kPolicySizeLimit) ||
      !result.policy.ParseFromString(data)) {
    LOG(WARNING) << "Failed to read or parse policy data from "
                 << policy_path.value();
    result.status = policy::LOAD_RESULT_LOAD_ERROR;
    return result;
  }

  if (!base::ReadFileToString(key_path, &data, kKeySizeLimit) ||
      !result.key.ParseFromString(data)) {
    // Log an error on missing key data, but do not trigger a load failure
    // for now since there are still old unsigned cached policy blobs in the
    // wild with no associated key (see kMetricPolicyHasVerifiedCachedKey UMA
    // stat below).
    LOG(ERROR) << "Failed to read or parse key data from " << key_path.value();
    result.key.clear_signing_key();
  }

  // Track the occurrence of valid cached keys - when this ratio gets high
  // enough, we can update the code to reject unsigned policy or unverified
  // keys.
  UMA_HISTOGRAM_BOOLEAN(kMetricPolicyHasVerifiedCachedKey,
                        result.key.has_signing_key());

  result.status = policy::LOAD_RESULT_SUCCESS;
  return result;
}

bool WriteStringToFile(const base::FilePath path, const std::string& data) {
 if (!base::CreateDirectory(path.DirName())) {
    DLOG(WARNING) << "Failed to create directory " << path.DirName().value();
    return false;
  }

  int size = data.size();
  if (base::WriteFile(path, data.c_str(), size) != size) {
    DLOG(WARNING) << "Failed to write " << path.value();
    return false;
  }

  return true;
}

// Stores policy to the backing file (must be called via a task on
// the background thread).
void StorePolicyToDiskOnBackgroundThread(
    const base::FilePath& policy_path,
    const base::FilePath& key_path,
    const std::string& verification_key,
    const em::PolicyFetchResponse& policy) {
  DVLOG(1) << "Storing policy to " << policy_path.value();
  std::string data;
  if (!policy.SerializeToString(&data)) {
    DLOG(WARNING) << "Failed to serialize policy data";
    return;
  }

  if (!WriteStringToFile(policy_path, data))
    return;

  if (policy.has_new_public_key()) {
    // Write the new public key and its verification signature/key to a file.
    em::PolicySigningKey key_info;
    key_info.set_signing_key(policy.new_public_key());
    key_info.set_signing_key_signature(
        policy.new_public_key_verification_signature());
    key_info.set_verification_key(verification_key);
    std::string key_data;
    if (!key_info.SerializeToString(&key_data)) {
      DLOG(WARNING) << "Failed to serialize policy signing key";
      return;
    }

    WriteStringToFile(key_path, key_data);
  }
}

}  // namespace

UserCloudPolicyStore::UserCloudPolicyStore(
    const base::FilePath& policy_path,
    const base::FilePath& key_path,
    const std::string& verification_key,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : UserCloudPolicyStoreBase(background_task_runner),
      weak_factory_(this),
      policy_path_(policy_path),
      key_path_(key_path),
      verification_key_(verification_key) {}

UserCloudPolicyStore::~UserCloudPolicyStore() {}

// static
scoped_ptr<UserCloudPolicyStore> UserCloudPolicyStore::Create(
    const base::FilePath& profile_path,
    const std::string& verification_key,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  base::FilePath policy_path =
      profile_path.Append(kPolicyDir).Append(kPolicyCacheFile);
  base::FilePath key_path =
      profile_path.Append(kPolicyDir).Append(kKeyCacheFile);
  return make_scoped_ptr(new UserCloudPolicyStore(
      policy_path, key_path, verification_key, background_task_runner));
}

void UserCloudPolicyStore::SetSigninUsername(const std::string& username) {
  signin_username_ = username;
}

void UserCloudPolicyStore::LoadImmediately() {
  DVLOG(1) << "Initiating immediate policy load from disk";
  // Cancel any pending Load/Store/Validate operations.
  weak_factory_.InvalidateWeakPtrs();
  // Load the policy from disk...
  PolicyLoadResult result = LoadPolicyFromDisk(policy_path_, key_path_);
  // ...and install it, reporting success/failure to any observers.
  PolicyLoaded(false, result);
}

void UserCloudPolicyStore::Clear() {
  background_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&base::DeleteFile), policy_path_, false));
  background_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&base::DeleteFile), key_path_, false));
  policy_.reset();
  policy_map_.Clear();
  policy_key_.clear();
  NotifyStoreLoaded();
}

void UserCloudPolicyStore::Load() {
  DVLOG(1) << "Initiating policy load from disk";
  // Cancel any pending Load/Store/Validate operations.
  weak_factory_.InvalidateWeakPtrs();

  // Start a new Load operation and have us get called back when it is
  // complete.
  base::PostTaskAndReplyWithResult(
      background_task_runner(),
      FROM_HERE,
      base::Bind(&LoadPolicyFromDisk, policy_path_, key_path_),
      base::Bind(&UserCloudPolicyStore::PolicyLoaded,
                 weak_factory_.GetWeakPtr(), true));
}

void UserCloudPolicyStore::PolicyLoaded(bool validate_in_background,
                                        PolicyLoadResult result) {
  switch (result.status) {
    case LOAD_RESULT_LOAD_ERROR:
      status_ = STATUS_LOAD_ERROR;
      NotifyStoreError();
      break;

    case LOAD_RESULT_NO_POLICY_FILE:
      DVLOG(1) << "No policy found on disk";
      NotifyStoreLoaded();
      break;

    case LOAD_RESULT_SUCCESS: {
      // Found policy on disk - need to validate it before it can be used.
      scoped_ptr<em::PolicyFetchResponse> cloud_policy(
          new em::PolicyFetchResponse(result.policy));
      scoped_ptr<em::PolicySigningKey> key(
          new em::PolicySigningKey(result.key));

      bool doing_key_rotation = false;
      const std::string& verification_key = verification_key_;
      if (!key->has_verification_key() ||
          key->verification_key() != verification_key_) {
        // The cached key didn't match our current key, so we're doing a key
        // rotation - make sure we request a new key from the server on our
        // next fetch.
        doing_key_rotation = true;
        DLOG(WARNING) << "Verification key rotation detected";
        // TODO(atwilson): Add code to update |verification_key| to point to
        // the correct key to validate the existing blob (can't do this until
        // we've done our first key rotation).
      }

      Validate(cloud_policy.Pass(),
               key.Pass(),
               verification_key,
               validate_in_background,
               base::Bind(
                   &UserCloudPolicyStore::InstallLoadedPolicyAfterValidation,
                   weak_factory_.GetWeakPtr(),
                   doing_key_rotation,
                   result.key.has_signing_key() ?
                       result.key.signing_key() : std::string()));
      break;
    }
    default:
      NOTREACHED();
  }
}

void UserCloudPolicyStore::InstallLoadedPolicyAfterValidation(
    bool doing_key_rotation,
    const std::string& signing_key,
    UserCloudPolicyValidator* validator) {
  validation_status_ = validator->status();
  if (!validator->success()) {
    DVLOG(1) << "Validation failed: status=" << validation_status_;
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  DVLOG(1) << "Validation succeeded - installing policy with dm_token: " <<
      validator->policy_data()->request_token();
  DVLOG(1) << "Device ID: " << validator->policy_data()->device_id();

  // If we're doing a key rotation, clear the public key version so a future
  // policy fetch will force regeneration of the keys.
  if (doing_key_rotation) {
    validator->policy_data()->clear_public_key_version();
    policy_key_.clear();
  } else {
    // Policy validation succeeded, so we know the signing key is good.
    policy_key_ = signing_key;
  }

  InstallPolicy(validator->policy_data().Pass(), validator->payload().Pass());
  status_ = STATUS_OK;
  NotifyStoreLoaded();
}

void UserCloudPolicyStore::Store(const em::PolicyFetchResponse& policy) {
  // Stop any pending requests to store policy, then validate the new policy
  // before storing it.
  weak_factory_.InvalidateWeakPtrs();
  scoped_ptr<em::PolicyFetchResponse> policy_copy(
      new em::PolicyFetchResponse(policy));
  Validate(policy_copy.Pass(),
           scoped_ptr<em::PolicySigningKey>(),
           verification_key_,
           true,
           base::Bind(&UserCloudPolicyStore::StorePolicyAfterValidation,
                      weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStore::Validate(
    scoped_ptr<em::PolicyFetchResponse> policy,
    scoped_ptr<em::PolicySigningKey> cached_key,
    const std::string& verification_key,
    bool validate_in_background,
    const UserCloudPolicyValidator::CompletionCallback& callback) {

  const bool signed_policy = policy->has_policy_data_signature();

  // Configure the validator.
  scoped_ptr<UserCloudPolicyValidator> validator = CreateValidator(
      policy.Pass(),
      CloudPolicyValidatorBase::TIMESTAMP_NOT_BEFORE);

  // Extract the owning domain from the signed-in user (if any is set yet).
  // If there's no owning domain, then the code just ensures that the policy
  // is self-consistent (that the keys are signed with the same domain that the
  // username field in the policy contains). UserPolicySigninServerBase will
  // verify that the username matches the signed in user once profile
  // initialization is complete (http://crbug.com/342327).
  std::string owning_domain;

  // Validate the username if the user is signed in. The signin_username_ can
  // be empty during initial policy load because this happens before the
  // Prefs subsystem is initialized.
  if (!signin_username_.empty()) {
    DVLOG(1) << "Validating username: " << signin_username_;
    validator->ValidateUsername(signin_username_, true);
    owning_domain = gaia::ExtractDomainName(
        gaia::CanonicalizeEmail(gaia::SanitizeEmail(signin_username_)));
  }

  // There are 4 cases:
  //
  // 1) Validation after loading from cache with no cached key.
  // Action: Don't validate signature (migration from previously cached
  // unsigned blob).
  //
  // 2) Validation after loading from cache with a cached key
  // Action: Validate signature on policy blob but don't allow key rotation.
  //
  // 3) Validation after loading new policy from the server with no cached key
  // Action: Validate as initial key provisioning (case where we are migrating
  // from unsigned policy)
  //
  // 4) Validation after loading new policy from the server with a cached key
  // Action: Validate as normal, and allow key rotation.
  if (cached_key) {
    // Loading from cache should not change the cached keys.
    DCHECK(policy_key_.empty() || policy_key_ == cached_key->signing_key());
    if (!signed_policy || !cached_key->has_signing_key()) {
      // Case #1 - loading from cache with no signing key.
      // TODO(atwilson): Reject policy with no cached key once
      // kMetricPolicyHasVerifiedCachedKey rises to a high enough level.
      DLOG(WARNING) << "Allowing unsigned cached blob for migration";
    } else {
      // Case #2 - loading from cache with a cached key - validate the cached
      // key, then do normal policy data signature validation using the cached
      // key. We're loading from cache so don't allow key rotation.
      validator->ValidateCachedKey(cached_key->signing_key(),
                                   cached_key->signing_key_signature(),
                                   verification_key,
                                   owning_domain);
      const bool no_rotation = false;
      validator->ValidateSignature(cached_key->signing_key(),
                                   verification_key,
                                   owning_domain,
                                   no_rotation);
    }
  } else {
    // No passed cached_key - this is not validating the initial policy load
    // from cache, but rather an update from the server.
    if (policy_key_.empty()) {
      // Case #3 - no valid existing policy key (either this is the initial
      // policy fetch, or we're doing a key rotation), so this new policy fetch
      // should include an initial key provision.
      validator->ValidateInitialKey(verification_key, owning_domain);
    } else {
      // Case #4 - verify new policy with existing key. We always allow key
      // rotation - the verification key will prevent invalid policy from being
      // injected. |policy_key_| is already known to be valid, so no need to
      // verify via ValidateCachedKey().
      const bool allow_rotation = true;
      validator->ValidateSignature(
          policy_key_, verification_key, owning_domain, allow_rotation);
    }
  }

  if (validate_in_background) {
    // Start validation in the background. The Validator will free itself once
    // validation is complete.
    validator.release()->StartValidation(callback);
  } else {
    // Run validation immediately and invoke the callback with the results.
    validator->RunValidation();
    callback.Run(validator.get());
  }
}

void UserCloudPolicyStore::StorePolicyAfterValidation(
    UserCloudPolicyValidator* validator) {
  validation_status_ = validator->status();
  DVLOG(1) << "Policy validation complete: status = " << validation_status_;
  if (!validator->success()) {
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  // Persist the validated policy (just fire a task - don't bother getting a
  // reply because we can't do anything if it fails).
  background_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&StorePolicyToDiskOnBackgroundThread,
                 policy_path_, key_path_, verification_key_,
                 *validator->policy()));
  InstallPolicy(validator->policy_data().Pass(), validator->payload().Pass());

  // If the key was rotated, update our local cache of the key.
  if (validator->policy()->has_new_public_key())
    policy_key_ = validator->policy()->new_public_key();
  status_ = STATUS_OK;
  NotifyStoreLoaded();
}

}  // namespace policy
