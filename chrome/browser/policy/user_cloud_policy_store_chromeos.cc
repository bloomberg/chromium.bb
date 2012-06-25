// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_cloud_policy_store_chromeos.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "chrome/browser/policy/user_policy_disk_cache.h"
#include "chrome/browser/policy/user_policy_token_cache.h"
#include "chrome/common/net/gaia/gaia_auth_util.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/browser_thread.h"

namespace em = enterprise_management;

namespace policy {

namespace {
// Grace interval for policy timestamp checks, in seconds.
const int kTimestampGraceIntervalSeconds = 60;
}

// Decodes a CloudPolicySettings object into a policy map. The implementation is
// generated code in policy/cloud_policy_generated.cc.
void DecodePolicy(const em::CloudPolicySettings& policy,
                  PolicyMap* policies);

// Helper class for loading legacy policy caches.
class LegacyPolicyCacheLoader : public UserPolicyTokenCache::Delegate,
                                public UserPolicyDiskCache::Delegate {
 public:
  typedef base::Callback<void(const std::string&,
                              const std::string&,
                              bool has_policy,
                              const em::PolicyFetchResponse&,
                              CloudPolicyStore::Status)> Callback;

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
  em::PolicyFetchResponse policy_;
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
    if (policy.has_cloud_policy()) {
      has_policy_ = true;
      policy_.CopyFrom(policy.cloud_policy());
    }
  } else {
    LOG(WARNING) << "Failed to load legacy policy cache: " << result;
  }
  policy_cache_ = NULL;
  CheckLoadFinished();
}

void LegacyPolicyCacheLoader::CheckLoadFinished() {
  if (!token_loader_.get() && !policy_cache_.get())
    callback_.Run(dm_token_, device_id_, has_policy_, policy_, status_);
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
      return CloudPolicyStore::STATUS_PERSIST_LOAD_ERROR;
  }
  NOTREACHED();
  return CloudPolicyStore::STATUS_OK;
}

UserCloudPolicyStoreChromeOS::UserCloudPolicyStoreChromeOS(
    chromeos::SessionManagerClient* session_manager_client,
    const FilePath& legacy_token_cache_file,
    const FilePath& legacy_policy_cache_file)
    : session_manager_client_(session_manager_client),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      legacy_cache_dir_(legacy_token_cache_file.DirName()),
      legacy_loader_(new LegacyPolicyCacheLoader(legacy_token_cache_file,
                                                 legacy_policy_cache_file)),
      legacy_caches_loaded_(false) {}

UserCloudPolicyStoreChromeOS::~UserCloudPolicyStoreChromeOS() {}

void UserCloudPolicyStoreChromeOS::Store(
    const em::PolicyFetchResponse& policy) {
  if (!Validate(policy, NULL, NULL)) {
    NotifyStoreError();
    return;
  }

  std::string policy_data;
  if (!policy.SerializeToString(&policy_data)) {
    status_ = STATUS_PERSIST_SERIALIZE_ERROR;
    NotifyStoreError();
    return;
  }

  session_manager_client_->StoreUserPolicy(
      policy_data,
      base::Bind(&UserCloudPolicyStoreChromeOS::OnPolicyStored,
                 weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStoreChromeOS::Load() {
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
      // the world that about the policy update.
      policy_.reset();
      NotifyStoreLoaded();
    }
    return;
  }

  // Policy is supplied by session_manager. Disregard legacy data from now on.
  legacy_loader_.reset();

  em::PolicyFetchResponse policy;
  if (!policy.ParseFromString(policy_blob)) {
    status_ = STATUS_PERSIST_PARSE_ERROR;
    NotifyStoreError();
    return;
  }

  if (!InstallPolicy(policy)) {
    NotifyStoreError();
    return;
  }

  // Policy has been loaded successfully. This indicates that new-style policy
  // is working, so the legacy cache directory can be removed.
  status_ = STATUS_OK;
  if (!legacy_cache_dir_.empty()) {
    content::BrowserThread::PostBlockingPoolTask(
        FROM_HERE,
        base::Bind(&UserCloudPolicyStoreChromeOS::RemoveLegacyCacheDir,
                   legacy_cache_dir_));
    legacy_cache_dir_.clear();
  }
  NotifyStoreLoaded();
}

void UserCloudPolicyStoreChromeOS::OnPolicyStored(bool success) {
  if (!success) {
    status_ = STATUS_PERSIST_STORE_ERROR;
    NotifyStoreError();
  } else {
    Load();
  }
}

bool UserCloudPolicyStoreChromeOS::InstallPolicy(
    const em::PolicyFetchResponse& policy) {
  scoped_ptr<em::PolicyData> new_policy(new em::PolicyData());
  em::CloudPolicySettings cloud_policy;
  if (!Validate(policy, new_policy.get(), &cloud_policy))
    return false;

  // Decode the payload.
  PolicyMap new_policy_map;
  DecodePolicy(cloud_policy, &new_policy_map);

  policy_.swap(new_policy);
  policy_map_.Swap(&new_policy_map);
  return true;
}

bool UserCloudPolicyStoreChromeOS::Validate(
    const em::PolicyFetchResponse& policy,
    em::PolicyData* policy_data_ptr,
    em::CloudPolicySettings* cloud_policy_ptr) {
  // Check for error codes in PolicyFetchResponse.
  if ((policy.has_error_code() && policy.error_code() != 200) ||
      (policy.has_error_message() && !policy.error_message().empty())) {
    LOG(ERROR) << "Error in policy blob."
               << " code: " << policy.error_code()
               << " message: " << policy.error_message();
    status_ = STATUS_VALIDATION_ERROR_CODE_PRESENT;
    return false;
  }

  // TODO(mnissler): Signature verification would go here.

  // Parse policy data.
  em::PolicyData policy_data;
  if (!policy_data.ParseFromString(policy.policy_data()) ||
      !policy_data.IsInitialized()) {
    LOG(ERROR) << "Failed to parse policy response";
    status_ = STATUS_VALIDATION_PAYLOAD_PARSE_ERROR;
    return false;
  }

  // Policy type check.
  if (!policy_data.has_policy_type() ||
      policy_data.policy_type() != dm_protocol::kChromeUserPolicyType) {
    LOG(ERROR) << "Invalid policy type " << policy_data.policy_type();
    status_ = STATUS_VALIDATION_POLICY_TYPE;
    return false;
  }

  // Timestamp should be from the past. We allow for a 1-minute grace interval
  // to cover clock drift.
  base::Time policy_time =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMilliseconds(policy_data.timestamp());
  base::Time max_valid_time =
      base::Time::NowFromSystemTime() +
      base::TimeDelta::FromSeconds(kTimestampGraceIntervalSeconds);
  if (!policy_data.has_timestamp() ||
      (policy_time > max_valid_time)) {
    LOG(ERROR) << "Invalid timestamp " << policy_data.timestamp();
    status_ = STATUS_VALIDATION_TIMESTAMP;
    return false;
  }

  // Check the DM token.
  if (!policy_data.has_request_token() ||
      (policy_.get() &&
       policy_->has_request_token() &&
       policy_data.request_token() != policy_->request_token())) {
    LOG(ERROR) << "Invalid DM token " << policy_data.request_token();
    status_ = STATUS_VALIDATION_TOKEN;
    return false;
  }

  // Make sure the username corresponds to the currently logged-in user.
  if (!CheckPolicyUsername(policy_data)) {
    LOG(ERROR) << "Invalid username " << policy_data.username();
    status_ = STATUS_VALIDATION_USERNAME;
    return false;
  }

  // Parse cloud policy.
  em::CloudPolicySettings cloud_policy;
  if (!policy_data.has_policy_value() ||
      !cloud_policy.ParseFromString(policy_data.policy_value()) ||
      !cloud_policy.IsInitialized()) {
    LOG(ERROR) << "Failed to decode policy protobuf";
    status_ = STATUS_VALIDATION_POLICY_PARSE_ERROR;
    return false;
  }

  // Success.
  if (policy_data_ptr)
    policy_data_ptr->Swap(&policy_data);
  if (cloud_policy_ptr)
    cloud_policy_ptr->Swap(&cloud_policy);

  return true;
}

bool UserCloudPolicyStoreChromeOS::CheckPolicyUsername(
    const em::PolicyData& policy) {
  if (!policy.has_username())
    return false;

  chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  if (!user_manager->IsUserLoggedIn())
    return false;

  std::string policy_username =
      gaia::CanonicalizeEmail(gaia::SanitizeEmail(policy.username()));
  const chromeos::User& user = chromeos::UserManager::Get()->GetLoggedInUser();
  return user.email() == policy_username;
}

void UserCloudPolicyStoreChromeOS::OnLegacyLoadFinished(
    const std::string& dm_token,
    const std::string& device_id,
    bool has_policy,
    const em::PolicyFetchResponse& policy,
    Status status) {
  status_ = status;
  if (has_policy && InstallPolicy(policy)) {
    // Clear the public key version. The public key version field would
    // otherwise indicate that we have key installed in the store when in fact
    // we haven't. This may result in policy updates failing signature
    // verification.
    policy_->clear_public_key_version();
  }

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
