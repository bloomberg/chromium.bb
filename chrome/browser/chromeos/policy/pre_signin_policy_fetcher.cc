// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/pre_signin_policy_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/time/time.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// We will abort fresh policy fetch after this time and use cached policy.
const int kPolicyFetchTimeoutSecs = 10;

// Traits for the tasks posted in pre-signin policy fetch. As this blocks
// signin, the tasks have user-visible priority.
constexpr base::TaskTraits kTaskTraits = {base::MayBlock(),
                                          base::TaskPriority::USER_VISIBLE};
}  // namespace

PreSigninPolicyFetcher::PreSigninPolicyFetcher(
    chromeos::CryptohomeClient* cryptohome_client,
    chromeos::SessionManagerClient* session_manager_client,
    std::unique_ptr<CloudPolicyClient> cloud_policy_client,
    const AccountId& account_id,
    const cryptohome::KeyDefinition& auth_key)
    : cryptohome_client_(cryptohome_client),
      session_manager_client_(session_manager_client),
      cloud_policy_client_(std::move(cloud_policy_client)),
      account_id_(account_id),
      auth_key_(auth_key),
      task_runner_(base::CreateSequencedTaskRunnerWithTraits(kTaskTraits)),
      weak_ptr_factory_(this) {}

PreSigninPolicyFetcher ::~PreSigninPolicyFetcher() {}

void PreSigninPolicyFetcher::FetchPolicy(PolicyFetchResultCallback callback) {
  DCHECK(callback_.is_null());
  callback_ = std::move(callback);

  cryptohome::MountRequest mount;
  mount.set_hidden_mount(true);
  cryptohome::HomedirMethods::GetInstance()->MountEx(
      cryptohome::Identification(account_id_),
      cryptohome::Authorization(auth_key_), mount,
      base::Bind(&PreSigninPolicyFetcher::OnMountTemporaryUserHome,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool PreSigninPolicyFetcher::ForceTimeoutForTesting() {
  if (!policy_fetch_timeout_.IsRunning())
    return false;

  policy_fetch_timeout_.Stop();
  OnPolicyFetchTimeout();

  return true;
}

void PreSigninPolicyFetcher::OnMountTemporaryUserHome(
    bool success,
    cryptohome::MountError return_code,
    const std::string& mount_hash) {
  if (!success || return_code != cryptohome::MOUNT_ERROR_NONE) {
    LOG(ERROR) << "Temporary user home mount failed.";
    NotifyCallback(PolicyFetchResult::ERROR, nullptr);
    return;
  }

  session_manager_client_->RetrievePolicyForUserWithoutSession(
      cryptohome::Identification(account_id_),
      base::Bind(&PreSigninPolicyFetcher::OnCachedPolicyRetrieved,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PreSigninPolicyFetcher::OnCachedPolicyRetrieved(
    const std::string& policy_blob,
    RetrievePolicyResponseType retrieve_policy_response) {
  // We only need the cached policy key if there was policy.
  if (!policy_blob.empty()) {
    base::FilePath policy_key_dir;
    CHECK(PathService::Get(chromeos::DIR_USER_POLICY_KEYS, &policy_key_dir));
    cached_policy_key_loader_ = base::MakeUnique<CachedPolicyKeyLoaderChromeOS>(
        cryptohome_client_, task_runner_, account_id_, policy_key_dir);
    cached_policy_key_loader_->EnsurePolicyKeyLoaded(base::Bind(
        &PreSigninPolicyFetcher::OnPolicyKeyLoaded,
        weak_ptr_factory_.GetWeakPtr(), policy_blob, retrieve_policy_response));
  } else {
    // Skip and pretend we've loaded policy key. We won't need it anyway,
    // because there is no policy to validate.
    OnPolicyKeyLoaded(policy_blob, retrieve_policy_response);
  }
}

void PreSigninPolicyFetcher::OnPolicyKeyLoaded(
    const std::string& policy_blob,
    RetrievePolicyResponseType retrieve_policy_response) {
  cryptohome_client_->Unmount(base::Bind(
      &PreSigninPolicyFetcher::OnUnmountTemporaryUserHome,
      weak_ptr_factory_.GetWeakPtr(), policy_blob, retrieve_policy_response));
}

void PreSigninPolicyFetcher::OnUnmountTemporaryUserHome(
    const std::string& policy_blob,
    RetrievePolicyResponseType retrieve_policy_response,
    chromeos::DBusMethodCallStatus unmount_call_status,
    bool unmount_success) {
  if (unmount_call_status != chromeos::DBUS_METHOD_CALL_SUCCESS ||
      !unmount_success) {
    // The temporary userhome mount could not be unmounted. Log an error and
    // continue, and hope that the unmount will be successful on the next mount
    // (temporary user homes are automatically unmounted by cryptohomed on every
    // mount request).
    LOG(ERROR) << "Couldn't unmount temporary mount point";
  }

  if (retrieve_policy_response != RetrievePolicyResponseType::SUCCESS) {
    NotifyCallback(PolicyFetchResult::ERROR, nullptr);
    return;
  }

  if (policy_blob.empty()) {
    VLOG(1) << "No cached policy.";
    NotifyCallback(PolicyFetchResult::NO_POLICY, nullptr);
    return;
  }

  // Parse policy.
  auto policy = base::MakeUnique<em::PolicyFetchResponse>();
  if (!policy->ParseFromString(policy_blob)) {
    NotifyCallback(PolicyFetchResult::ERROR, nullptr);
    return;
  }

  // Before validating, check that we have a cached policy key.
  if (cached_policy_key_loader_->cached_policy_key().empty()) {
    LOG(ERROR) << "No cached policy key loaded.";
    NotifyCallback(PolicyFetchResult::ERROR, nullptr);
    return;
  }

  // Validate policy from session_manager.
  UserCloudPolicyValidator::StartValidation(
      CreateValidatorForCachedPolicy(std::move(policy)),
      base::Bind(&PreSigninPolicyFetcher::OnCachedPolicyValidated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PreSigninPolicyFetcher::OnCachedPolicyValidated(
    UserCloudPolicyValidator* validator) {
  if (!validator->success()) {
    NotifyCallback(PolicyFetchResult::ERROR, nullptr);
    return;
  }

  policy_data_ = std::move(validator->policy_data());
  policy_payload_ = std::move(validator->payload());

  if (account_id_.GetAccountType() == AccountType::ACTIVE_DIRECTORY) {
    // For AD, we don't support fresh policy fetch at the moment. Simply exit
    // with cached policy.
    NotifyCallback(PolicyFetchResult::SUCCESS, std::move(policy_payload_));
    return;
  }

  // Try to retrieve fresh policy.
  cloud_policy_client_->SetupRegistration(policy_data_->request_token(),
                                          policy_data_->device_id());
  cloud_policy_client_->AddPolicyTypeToFetch(
      dm_protocol::kChromeUserPolicyType,
      std::string() /* settings_entity_id */);
  if (policy_data_->has_public_key_version()) {
    cloud_policy_client_->set_public_key_version(
        policy_data_->public_key_version());
  }
  cloud_policy_client_->AddObserver(this);

  // Start a timer that will limit how long we wait for fresh policy.
  policy_fetch_timeout_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kPolicyFetchTimeoutSecs),
      base::Bind(&PreSigninPolicyFetcher::OnPolicyFetchTimeout,
                 weak_ptr_factory_.GetWeakPtr()));

  cloud_policy_client_->FetchPolicy();
}

void PreSigninPolicyFetcher::OnPolicyFetched(CloudPolicyClient* client) {
  policy_fetch_timeout_.Stop();

  const em::PolicyFetchResponse* fetched_policy =
      cloud_policy_client_->GetPolicyFor(
          dm_protocol::kChromeUserPolicyType,
          std::string() /* settings_entity_id */);
  if (!fetched_policy || !fetched_policy->has_policy_data()) {
    // policy_payload_ still holds cached policy loaded from session_manager.
    NotifyCallback(PolicyFetchResult::SUCCESS, std::move(policy_payload_));
    return;
  }

  // Make a copy because there's currently no way to transfer ownership out of
  // CloudPolicyClient.
  auto fetched_policy_copy =
      base::MakeUnique<em::PolicyFetchResponse>(*fetched_policy);

  // Validate fresh policy.
  UserCloudPolicyValidator::StartValidation(
      CreateValidatorForFetchedPolicy(std::move(fetched_policy_copy)),
      base::Bind(&PreSigninPolicyFetcher::OnFetchedPolicyValidated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PreSigninPolicyFetcher::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  // Ignored.
}

void PreSigninPolicyFetcher::OnClientError(CloudPolicyClient* client) {
  policy_fetch_timeout_.Stop();
  VLOG(1) << "Fresh policy fetch failed.";
  // policy_payload_ still holds cached policy loaded from session_manager.
  NotifyCallback(PolicyFetchResult::SUCCESS, std::move(policy_payload_));
}

void PreSigninPolicyFetcher::OnPolicyFetchTimeout() {
  VLOG(1) << "Fresh policy fetch timed out.";
  // Invalidate all weak pointrs so OnPolicyFetched is not called back anymore.
  weak_ptr_factory_.InvalidateWeakPtrs();
  NotifyCallback(PolicyFetchResult::SUCCESS, std::move(policy_payload_));
}

void PreSigninPolicyFetcher::OnFetchedPolicyValidated(
    UserCloudPolicyValidator* validator) {
  if (!validator->success()) {
    VLOG(1) << "Validation of fetched policy failed.";
    // Return the cached policy.
    NotifyCallback(PolicyFetchResult::SUCCESS, std::move(policy_payload_));
    return;
  }

  policy_data_ = std::move(validator->policy_data());
  policy_payload_ = std::move(validator->payload());
  NotifyCallback(PolicyFetchResult::SUCCESS, std::move(policy_payload_));
}

void PreSigninPolicyFetcher::NotifyCallback(
    PolicyFetchResult result,
    std::unique_ptr<em::CloudPolicySettings> policy_payload) {
  // Clean up instances created during the policy fetch procedure.
  cached_policy_key_loader_.reset();
  policy_data_.reset();
  if (cloud_policy_client_) {
    cloud_policy_client_->RemoveObserver(this);
  }

  DCHECK(callback_);
  std::move(callback_).Run(result, std::move(policy_payload));
}

std::unique_ptr<UserCloudPolicyValidator>
PreSigninPolicyFetcher::CreateValidatorForCachedPolicy(
    std::unique_ptr<em::PolicyFetchResponse> policy) {
  std::unique_ptr<UserCloudPolicyValidator> validator =
      UserCloudPolicyValidator::Create(std::move(policy), task_runner_);

  validator->ValidatePolicyType(dm_protocol::kChromeUserPolicyType);
  validator->ValidatePayload();

  if (account_id_.GetAccountType() != AccountType::ACTIVE_DIRECTORY) {
    // Also validate the user e-mail and the signature (except for authpolicy).
    validator->ValidateUsername(account_id_.GetUserEmail(), true);
    validator->ValidateSignature(
        cached_policy_key_loader_->cached_policy_key());
  }
  return validator;
}

std::unique_ptr<UserCloudPolicyValidator>
PreSigninPolicyFetcher::CreateValidatorForFetchedPolicy(
    std::unique_ptr<em::PolicyFetchResponse> policy) {
  // Configure the validator to validate based on cached policy.
  std::unique_ptr<UserCloudPolicyValidator> validator =
      UserCloudPolicyValidator::Create(std::move(policy), task_runner_);

  validator->ValidatePolicyType(dm_protocol::kChromeUserPolicyType);
  validator->ValidateAgainstCurrentPolicy(
      policy_data_.get(), CloudPolicyValidatorBase::TIMESTAMP_VALIDATED,
      CloudPolicyValidatorBase::DM_TOKEN_REQUIRED,
      CloudPolicyValidatorBase::DEVICE_ID_REQUIRED);
  validator->ValidatePayload();

  if (account_id_.GetAccountType() != AccountType::ACTIVE_DIRECTORY) {
    // Also validate the signature.
    const std::string domain = gaia::ExtractDomainName(
        gaia::CanonicalizeEmail(account_id_.GetUserEmail()));
    validator->ValidateSignatureAllowingRotation(
        cached_policy_key_loader_->cached_policy_key(), domain);
  }
  return validator;
}

}  // namespace policy
