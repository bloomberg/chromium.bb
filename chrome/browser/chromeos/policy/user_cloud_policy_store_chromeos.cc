// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_store_chromeos.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/policy/user_policy_disk_cache.h"
#include "chrome/browser/chromeos/policy/user_policy_token_loader.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_local.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Path within |user_policy_key_dir_| that contains the policy key.
// "%s" must be substituted with the sanitized username.
const base::FilePath::CharType kPolicyKeyFile[] =
    FILE_PATH_LITERAL("%s/policy.pub");

// Maximum key size that will be loaded, in bytes.
const size_t kKeySizeLimit = 16 * 1024;

enum ValidationFailure {
  VALIDATION_FAILURE_DBUS,
  VALIDATION_FAILURE_LOAD_KEY,
  VALIDATION_FAILURE_SIZE,
};

void SampleValidationFailure(ValidationFailure sample) {
  UMA_HISTOGRAM_ENUMERATION("Enterprise.UserPolicyValidationFailure",
                            sample,
                            VALIDATION_FAILURE_SIZE);
}

// Extracts the domain name from the passed username.
std::string ExtractDomain(const std::string& username) {
  return gaia::ExtractDomainName(gaia::CanonicalizeEmail(username));
}

}  // namespace

UserCloudPolicyStoreChromeOS::UserCloudPolicyStoreChromeOS(
    chromeos::CryptohomeClient* cryptohome_client,
    chromeos::SessionManagerClient* session_manager_client,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const AccountId& account_id,
    const base::FilePath& user_policy_key_dir)
    : UserCloudPolicyStoreBase(background_task_runner),
      cryptohome_client_(cryptohome_client),
      session_manager_client_(session_manager_client),
      account_id_(account_id),
      user_policy_key_dir_(user_policy_key_dir),
      weak_factory_(this) {}

UserCloudPolicyStoreChromeOS::~UserCloudPolicyStoreChromeOS() {}

void UserCloudPolicyStoreChromeOS::Store(
    const em::PolicyFetchResponse& policy) {
  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();
  std::unique_ptr<em::PolicyFetchResponse> response(
      new em::PolicyFetchResponse(policy));
  EnsurePolicyKeyLoaded(
      base::Bind(&UserCloudPolicyStoreChromeOS::ValidatePolicyForStore,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&response)));
}

void UserCloudPolicyStoreChromeOS::Load() {
  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();
  session_manager_client_->RetrievePolicyForUser(
      cryptohome::Identification(account_id_),
      base::Bind(&UserCloudPolicyStoreChromeOS::OnPolicyRetrieved,
                 weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStoreChromeOS::LoadImmediately() {
  // This blocking DBus call is in the startup path and will block the UI
  // thread. This only happens when the Profile is created synchronously, which
  // on ChromeOS happens whenever the browser is restarted into the same
  // session. That happens when the browser crashes, or right after signin if
  // the user has flags configured in about:flags.
  // However, on those paths we must load policy synchronously so that the
  // Profile initialization never sees unmanaged prefs, which would lead to
  // data loss. http://crbug.com/263061
  std::string policy_blob =
      session_manager_client_->BlockingRetrievePolicyForUser(
          cryptohome::Identification(account_id_));
  if (policy_blob.empty()) {
    // The session manager doesn't have policy, or the call failed.
    NotifyStoreLoaded();
    return;
  }

  std::unique_ptr<em::PolicyFetchResponse> policy(
      new em::PolicyFetchResponse());
  if (!policy->ParseFromString(policy_blob)) {
    status_ = STATUS_PARSE_ERROR;
    NotifyStoreError();
    return;
  }

  std::string sanitized_username =
      cryptohome_client_->BlockingGetSanitizedUsername(
          cryptohome::Identification(account_id_));
  if (sanitized_username.empty()) {
    status_ = STATUS_LOAD_ERROR;
    NotifyStoreError();
    return;
  }

  cached_policy_key_path_ = user_policy_key_dir_.Append(
      base::StringPrintf(kPolicyKeyFile, sanitized_username.c_str()));
  LoadPolicyKey(cached_policy_key_path_, &cached_policy_key_);
  cached_policy_key_loaded_ = true;

  std::unique_ptr<UserCloudPolicyValidator> validator =
      CreateValidatorForLoad(std::move(policy));
  validator->RunValidation();
  OnRetrievedPolicyValidated(validator.get());
}

void UserCloudPolicyStoreChromeOS::ValidatePolicyForStore(
    std::unique_ptr<em::PolicyFetchResponse> policy) {
  // Create and configure a validator.
  std::unique_ptr<UserCloudPolicyValidator> validator = CreateValidator(
      std::move(policy), CloudPolicyValidatorBase::TIMESTAMP_FULLY_VALIDATED);
  validator->ValidateUsername(account_id_.GetUserEmail(), true);
  if (cached_policy_key_.empty()) {
    validator->ValidateInitialKey(ExtractDomain(account_id_.GetUserEmail()));
  } else {
    validator->ValidateSignatureAllowingRotation(
        cached_policy_key_, ExtractDomain(account_id_.GetUserEmail()));
  }

  // Start validation. The Validator will delete itself once validation is
  // complete.
  validator.release()->StartValidation(
      base::Bind(&UserCloudPolicyStoreChromeOS::OnPolicyToStoreValidated,
                 weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStoreChromeOS::OnPolicyToStoreValidated(
    UserCloudPolicyValidator* validator) {
  validation_status_ = validator->status();

  UMA_HISTOGRAM_ENUMERATION(
      "Enterprise.UserPolicyValidationStoreStatus",
      validation_status_,
      UserCloudPolicyValidator::VALIDATION_STATUS_SIZE);

  if (!validator->success()) {
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  std::string policy_blob;
  if (!validator->policy()->SerializeToString(&policy_blob)) {
    status_ = STATUS_SERIALIZE_ERROR;
    NotifyStoreError();
    return;
  }

  session_manager_client_->StorePolicyForUser(
      cryptohome::Identification(account_id_), policy_blob,
      base::Bind(&UserCloudPolicyStoreChromeOS::OnPolicyStored,
                 weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStoreChromeOS::OnPolicyStored(bool success) {
  if (!success) {
    status_ = STATUS_STORE_ERROR;
    NotifyStoreError();
  } else {
    // Load the policy right after storing it, to make sure it was accepted by
    // the session manager. An additional validation is performed after the
    // load; reload the key for that validation too, in case it was rotated.
    ReloadPolicyKey(base::Bind(&UserCloudPolicyStoreChromeOS::Load,
                               weak_factory_.GetWeakPtr()));
  }
}

void UserCloudPolicyStoreChromeOS::OnPolicyRetrieved(
    const std::string& policy_blob) {
  if (policy_blob.empty()) {
    // session_manager doesn't have policy. Adjust internal state and notify
    // the world about the policy update.
    policy_map_.Clear();
    policy_.reset();
    policy_signature_public_key_.clear();
    NotifyStoreLoaded();
    return;
  }

  std::unique_ptr<em::PolicyFetchResponse> policy(
      new em::PolicyFetchResponse());
  if (!policy->ParseFromString(policy_blob)) {
    status_ = STATUS_PARSE_ERROR;
    NotifyStoreError();
    return;
  }

  // Load |cached_policy_key_| to verify the loaded policy.
  EnsurePolicyKeyLoaded(
      base::Bind(&UserCloudPolicyStoreChromeOS::ValidateRetrievedPolicy,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&policy)));
}

void UserCloudPolicyStoreChromeOS::ValidateRetrievedPolicy(
    std::unique_ptr<em::PolicyFetchResponse> policy) {
  // Create and configure a validator for the loaded policy.
  std::unique_ptr<UserCloudPolicyValidator> validator =
      CreateValidatorForLoad(std::move(policy));
  // Start validation. The Validator will delete itself once validation is
  // complete.
  validator.release()->StartValidation(
      base::Bind(&UserCloudPolicyStoreChromeOS::OnRetrievedPolicyValidated,
                 weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStoreChromeOS::OnRetrievedPolicyValidated(
    UserCloudPolicyValidator* validator) {
  validation_status_ = validator->status();

  UMA_HISTOGRAM_ENUMERATION(
      "Enterprise.UserPolicyValidationLoadStatus",
      validation_status_,
      UserCloudPolicyValidator::VALIDATION_STATUS_SIZE);

  if (!validator->success()) {
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  InstallPolicy(std::move(validator->policy_data()),
                std::move(validator->payload()), cached_policy_key_);
  status_ = STATUS_OK;

  NotifyStoreLoaded();
}

void UserCloudPolicyStoreChromeOS::ReloadPolicyKey(
    const base::Closure& callback) {
  std::string* key = new std::string();
  background_task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(&UserCloudPolicyStoreChromeOS::LoadPolicyKey,
                            cached_policy_key_path_, key),
      base::Bind(&UserCloudPolicyStoreChromeOS::OnPolicyKeyReloaded,
                 weak_factory_.GetWeakPtr(), base::Owned(key), callback));
}

// static
void UserCloudPolicyStoreChromeOS::LoadPolicyKey(const base::FilePath& path,
                                                 std::string* key) {
  if (!base::PathExists(path)) {
    // There is no policy key the first time that a user fetches policy. If
    // |path| does not exist then that is the most likely scenario, so there's
    // no need to sample a failure.
    VLOG(1) << "No key at " << path.value();
    return;
  }

  const bool read_success =
      base::ReadFileToStringWithMaxSize(path, key, kKeySizeLimit);
  // If the read was successful and the file size is 0 or if the read fails
  // due to file size exceeding |kKeySizeLimit|, log error.
  if ((read_success && key->length() == 0) ||
      (!read_success && key->length() == kKeySizeLimit)) {
    LOG(ERROR) << "Key at " << path.value()
               << (read_success ? " is empty." : " exceeds size limit");
    key->clear();
  } else if (!read_success) {
    LOG(ERROR) << "Failed to read key at " << path.value();
  }

  if (key->empty())
    SampleValidationFailure(VALIDATION_FAILURE_LOAD_KEY);
}

void UserCloudPolicyStoreChromeOS::OnPolicyKeyReloaded(
    std::string* key,
    const base::Closure& callback) {
  cached_policy_key_ = *key;
  cached_policy_key_loaded_ = true;
  callback.Run();
}

void UserCloudPolicyStoreChromeOS::EnsurePolicyKeyLoaded(
    const base::Closure& callback) {
  if (cached_policy_key_loaded_) {
    callback.Run();
  } else {
    // Get the hashed username that's part of the key's path, to determine
    // |cached_policy_key_path_|.
    cryptohome_client_->GetSanitizedUsername(
        cryptohome::Identification(account_id_),
        base::Bind(&UserCloudPolicyStoreChromeOS::OnGetSanitizedUsername,
                   weak_factory_.GetWeakPtr(), callback));
  }
}

void UserCloudPolicyStoreChromeOS::OnGetSanitizedUsername(
    const base::Closure& callback,
    chromeos::DBusMethodCallStatus call_status,
    const std::string& sanitized_username) {
  // The default empty path will always yield an empty key.
  if (call_status == chromeos::DBUS_METHOD_CALL_SUCCESS &&
      !sanitized_username.empty()) {
    cached_policy_key_path_ = user_policy_key_dir_.Append(
        base::StringPrintf(kPolicyKeyFile, sanitized_username.c_str()));
  } else {
    SampleValidationFailure(VALIDATION_FAILURE_DBUS);
  }
  ReloadPolicyKey(callback);
}

std::unique_ptr<UserCloudPolicyValidator>
UserCloudPolicyStoreChromeOS::CreateValidatorForLoad(
    std::unique_ptr<em::PolicyFetchResponse> policy) {
  std::unique_ptr<UserCloudPolicyValidator> validator = CreateValidator(
      std::move(policy), CloudPolicyValidatorBase::TIMESTAMP_NOT_BEFORE);
  validator->ValidateUsername(account_id_.GetUserEmail(), true);
  // The policy loaded from session manager need not be validated using the
  // verification key since it is secure, and since there may be legacy policy
  // data that was stored without a verification key.
  validator->ValidateSignature(cached_policy_key_);
  return validator;
}

}  // namespace policy
