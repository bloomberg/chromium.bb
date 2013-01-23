// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_cloud_policy_store_chromeos.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "chrome/browser/policy/user_policy_disk_cache.h"
#include "chrome/browser/policy/user_policy_token_loader.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace em = enterprise_management;

namespace policy {

namespace {
// Subdirectory in the user's profile for storing user policies.
const FilePath::CharType kPolicyDir[] = FILE_PATH_LITERAL("Device Management");
// File in the above directory for stroing user policy dmtokens.
const FilePath::CharType kTokenCacheFile[] = FILE_PATH_LITERAL("Token");
// File in the above directory for storing user policy data.
const FilePath::CharType kPolicyCacheFile[] = FILE_PATH_LITERAL("Policy");
}  // namespace


// Helper class for loading legacy policy caches.
class LegacyPolicyCacheLoader : public UserPolicyTokenLoader::Delegate,
                                public UserPolicyDiskCache::Delegate {
 public:
  typedef base::Callback<void(const std::string&,
                              const std::string&,
                              CloudPolicyStore::Status,
                              scoped_ptr<em::PolicyFetchResponse>)> Callback;

  LegacyPolicyCacheLoader(const FilePath& token_cache_file,
                          const FilePath& policy_cache_file);
  virtual ~LegacyPolicyCacheLoader();

  // Starts loading, and reports the result to |callback| when done.
  void Load(const Callback& callback);

  // UserPolicyTokenLoader::Delegate:
  virtual void OnTokenLoaded(const std::string& token,
                             const std::string& device_id) OVERRIDE;

  // UserPolicyDiskCache::Delegate:
  virtual void OnDiskCacheLoaded(
      UserPolicyDiskCache::LoadResult result,
      const em::CachedCloudPolicyResponse& policy) OVERRIDE;

 private:
  // Checks whether the load operations from the legacy caches completed. If so,
  // fires the appropriate notification.
  void CheckLoadFinished();

  // Maps a disk cache LoadResult to a CloudPolicyStore::Status.
  static CloudPolicyStore::Status TranslateLoadResult(
      UserPolicyDiskCache::LoadResult result);

  base::WeakPtrFactory<LegacyPolicyCacheLoader> weak_factory_;

  scoped_refptr<UserPolicyTokenLoader> token_loader_;
  scoped_refptr<UserPolicyDiskCache> policy_cache_;

  std::string dm_token_;
  std::string device_id_;
  bool has_policy_;
  scoped_ptr<em::PolicyFetchResponse> policy_;
  CloudPolicyStore::Status status_;

  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(LegacyPolicyCacheLoader);
};

LegacyPolicyCacheLoader::LegacyPolicyCacheLoader(
    const FilePath& token_cache_file,
    const FilePath& policy_cache_file)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      has_policy_(false),
      status_(CloudPolicyStore::STATUS_OK) {
  token_loader_ = new UserPolicyTokenLoader(weak_factory_.GetWeakPtr(),
                                            token_cache_file);
  policy_cache_ = new UserPolicyDiskCache(weak_factory_.GetWeakPtr(),
                                          policy_cache_file);
}

LegacyPolicyCacheLoader::~LegacyPolicyCacheLoader() {}

void LegacyPolicyCacheLoader::Load(const Callback& callback) {
  callback_ = callback;
  token_loader_->Load();
  policy_cache_->Load();
}

void LegacyPolicyCacheLoader::OnTokenLoaded(const std::string& token,
                                            const std::string& device_id) {
  dm_token_ = token;
  device_id_ = device_id;
  token_loader_ = NULL;
  CheckLoadFinished();
}

void LegacyPolicyCacheLoader::OnDiskCacheLoaded(
    UserPolicyDiskCache::LoadResult result,
    const em::CachedCloudPolicyResponse& policy) {
  status_ = TranslateLoadResult(result);
  if (result == UserPolicyDiskCache::LOAD_RESULT_SUCCESS) {
    if (policy.has_cloud_policy())
      policy_.reset(new em::PolicyFetchResponse(policy.cloud_policy()));
  } else {
    LOG(WARNING) << "Failed to load legacy policy cache: " << result;
  }
  policy_cache_ = NULL;
  CheckLoadFinished();
}

void LegacyPolicyCacheLoader::CheckLoadFinished() {
  if (!token_loader_.get() && !policy_cache_.get())
    callback_.Run(dm_token_, device_id_, status_, policy_.Pass());
}

// static
CloudPolicyStore::Status LegacyPolicyCacheLoader::TranslateLoadResult(
    UserPolicyDiskCache::LoadResult result) {
  switch (result) {
    case UserPolicyDiskCache::LOAD_RESULT_SUCCESS:
    case UserPolicyDiskCache::LOAD_RESULT_NOT_FOUND:
      return CloudPolicyStore::STATUS_OK;
    case UserPolicyDiskCache::LOAD_RESULT_PARSE_ERROR:
    case UserPolicyDiskCache::LOAD_RESULT_READ_ERROR:
      return CloudPolicyStore::STATUS_LOAD_ERROR;
  }
  NOTREACHED();
  return CloudPolicyStore::STATUS_OK;
}

UserCloudPolicyStoreChromeOS::UserCloudPolicyStoreChromeOS(
    chromeos::SessionManagerClient* session_manager_client,
    const std::string& username,
    const FilePath& legacy_token_cache_file,
    const FilePath& legacy_policy_cache_file)
    : session_manager_client_(session_manager_client),
      username_(username),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      legacy_cache_dir_(legacy_token_cache_file.DirName()),
      legacy_loader_(new LegacyPolicyCacheLoader(legacy_token_cache_file,
                                                 legacy_policy_cache_file)),
      legacy_caches_loaded_(false) {}

UserCloudPolicyStoreChromeOS::~UserCloudPolicyStoreChromeOS() {}

void UserCloudPolicyStoreChromeOS::Store(
    const em::PolicyFetchResponse& policy) {
  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();
  Validate(
      scoped_ptr<em::PolicyFetchResponse>(new em::PolicyFetchResponse(policy)),
      base::Bind(&UserCloudPolicyStoreChromeOS::OnPolicyToStoreValidated,
                 weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStoreChromeOS::Load() {
  // Cancel all pending requests.
  weak_factory_.InvalidateWeakPtrs();
  session_manager_client_->RetrieveUserPolicy(
      base::Bind(&UserCloudPolicyStoreChromeOS::OnPolicyRetrieved,
                 weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStoreChromeOS::OnPolicyRetrieved(
    const std::string& policy_blob) {
  if (policy_blob.empty()) {
    // Policy fetch failed. Try legacy caches if we haven't done that already.
    if (!legacy_caches_loaded_ && legacy_loader_.get()) {
      legacy_caches_loaded_ = true;
      legacy_loader_->Load(
          base::Bind(&UserCloudPolicyStoreChromeOS::OnLegacyLoadFinished,
                     weak_factory_.GetWeakPtr()));
    } else {
      // session_manager doesn't have policy. Adjust internal state and notify
      // the world about the policy update.
      policy_.reset();
      NotifyStoreLoaded();
    }
    return;
  }

  // Policy is supplied by session_manager. Disregard legacy data from now on.
  legacy_loader_.reset();

  scoped_ptr<em::PolicyFetchResponse> policy(new em::PolicyFetchResponse());
  if (!policy->ParseFromString(policy_blob)) {
    status_ = STATUS_PARSE_ERROR;
    NotifyStoreError();
    return;
  }

  Validate(policy.Pass(),
           base::Bind(&UserCloudPolicyStoreChromeOS::OnRetrievedPolicyValidated,
                      weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStoreChromeOS::OnRetrievedPolicyValidated(
    UserCloudPolicyValidator* validator) {
  validation_status_ = validator->status();
  if (!validator->success()) {
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  InstallPolicy(validator->policy_data().Pass(), validator->payload().Pass());
  status_ = STATUS_OK;

  // Policy has been loaded successfully. This indicates that new-style policy
  // is working, so the legacy cache directory can be removed.
  if (!legacy_cache_dir_.empty()) {
    content::BrowserThread::PostBlockingPoolTask(
        FROM_HERE,
        base::Bind(&UserCloudPolicyStoreChromeOS::RemoveLegacyCacheDir,
                   legacy_cache_dir_));
    legacy_cache_dir_.clear();
  }
  NotifyStoreLoaded();
}

void UserCloudPolicyStoreChromeOS::OnPolicyToStoreValidated(
    UserCloudPolicyValidator* validator) {
  validation_status_ = validator->status();
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

  session_manager_client_->StoreUserPolicy(
      policy_blob,
      base::Bind(&UserCloudPolicyStoreChromeOS::OnPolicyStored,
                 weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStoreChromeOS::OnPolicyStored(bool success) {
  if (!success) {
    status_ = STATUS_STORE_ERROR;
    NotifyStoreError();
  } else {
    // TODO(mnissler): Once we do signature verifications, we'll have to reload
    // the key at this point to account for key rotations.
    Load();
  }
}

void UserCloudPolicyStoreChromeOS::Validate(
    scoped_ptr<em::PolicyFetchResponse> policy,
    const UserCloudPolicyValidator::CompletionCallback& callback) {
  // Configure the validator.
  scoped_ptr<UserCloudPolicyValidator> validator =
      CreateValidator(policy.Pass());
  validator->ValidateUsername(username_);

  // TODO(mnissler): Do a signature check here as well. The key is stored by
  // session_manager in the root-owned cryptohome area, which is currently
  // inaccessible to Chrome though.

  // Start validation. The Validator will free itself once validation is
  // complete.
  validator.release()->StartValidation(callback);
}

void UserCloudPolicyStoreChromeOS::OnLegacyLoadFinished(
    const std::string& dm_token,
    const std::string& device_id,
    Status status,
    scoped_ptr<em::PolicyFetchResponse> policy) {
  status_ = status;
  if (policy.get()) {
    Validate(policy.Pass(),
             base::Bind(&UserCloudPolicyStoreChromeOS::OnLegacyPolicyValidated,
                        weak_factory_.GetWeakPtr(),
                        dm_token, device_id));
  } else {
    InstallLegacyTokens(dm_token, device_id);
  }
}

void UserCloudPolicyStoreChromeOS::OnLegacyPolicyValidated(
    const std::string& dm_token,
    const std::string& device_id,
    UserCloudPolicyValidator* validator) {
  validation_status_ = validator->status();
  if (validator->success()) {
    status_ = STATUS_OK;
    InstallPolicy(validator->policy_data().Pass(),
                  validator->payload().Pass());

    // Clear the public key version. The public key version field would
    // otherwise indicate that we have key installed in the store when in fact
    // we haven't. This may result in policy updates failing signature
    // verification.
    policy_->clear_public_key_version();
  } else {
    status_ = STATUS_VALIDATION_ERROR;
  }

  InstallLegacyTokens(dm_token, device_id);
}

void UserCloudPolicyStoreChromeOS::InstallLegacyTokens(
    const std::string& dm_token,
    const std::string& device_id) {
  // Write token and device ID to |policy_|, giving them precedence over the
  // policy blob. This is to match the legacy behavior, which used token and
  // device id exclusively from the token cache file.
  if (!dm_token.empty() && !device_id.empty()) {
    if (!policy_.get())
      policy_.reset(new em::PolicyData());
    policy_->set_request_token(dm_token);
    policy_->set_device_id(device_id);
  }

  // Tell the rest of the world that the policy load completed.
  NotifyStoreLoaded();
}

// static
void UserCloudPolicyStoreChromeOS::RemoveLegacyCacheDir(const FilePath& dir) {
  if (file_util::PathExists(dir) && !file_util::Delete(dir, true))
    LOG(ERROR) << "Failed to remove cache dir " << dir.value();
}

}  // namespace policy
