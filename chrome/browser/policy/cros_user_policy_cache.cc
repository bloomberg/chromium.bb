// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cros_user_policy_cache.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace em = enterprise_management;

namespace policy {

// Decodes a CloudPolicySettings object into a policy map. The implementation is
// generated code in policy/cloud_policy_generated.cc.
void DecodePolicy(const em::CloudPolicySettings& policy, PolicyMap* policies);

// Takes care of sending a new policy blob to session manager and reports back
// the status through a callback.
class CrosUserPolicyCache::StorePolicyOperation {
 public:
  typedef base::Callback<void(bool)> StatusCallback;

  StorePolicyOperation(
      const em::PolicyFetchResponse& policy,
      chromeos::SessionManagerClient* session_manager_client,
      const StatusCallback& callback);

  // Executes the operation.
  void Run();

  // Cancels a pending callback.
  void Cancel();

  const em::PolicyFetchResponse& policy() { return policy_; }

 private:
  // StorePolicyOperation manages its own lifetime.
  ~StorePolicyOperation() {}

  // A callback function suitable for passing to session_manager_client.
  void OnPolicyStored(bool result);

  em::PolicyFetchResponse policy_;
  chromeos::SessionManagerClient* session_manager_client_;
  StatusCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(StorePolicyOperation);
};

CrosUserPolicyCache::StorePolicyOperation::StorePolicyOperation(
    const em::PolicyFetchResponse& policy,
    chromeos::SessionManagerClient* session_manager_client,
    const StatusCallback& callback)
    : policy_(policy),
      session_manager_client_(session_manager_client),
      callback_(callback) {}

void CrosUserPolicyCache::StorePolicyOperation::Run() {
  std::string serialized;
  if (!policy_.SerializeToString(&serialized)) {
    LOG(ERROR) << "Failed to serialize policy protobuf!";
    if (!callback_.is_null())
      callback_.Run(false);
    delete this;
    return;
  }
  session_manager_client_->StoreUserPolicy(
      serialized,
      base::Bind(&CrosUserPolicyCache::StorePolicyOperation::OnPolicyStored,
                 base::Unretained(this)));
}

void CrosUserPolicyCache::StorePolicyOperation::Cancel() {
  callback_.Reset();
}

void CrosUserPolicyCache::StorePolicyOperation::OnPolicyStored(bool result) {
  if (!callback_.is_null())
    callback_.Run(result);
  delete this;
}

class CrosUserPolicyCache::RetrievePolicyOperation {
 public:
  typedef base::Callback<void(bool, const em::PolicyFetchResponse&)>
      ResultCallback;

  RetrievePolicyOperation(
      chromeos::SessionManagerClient* session_manager_client,
      const ResultCallback& callback);

  // Executes the operation.
  void Run();

  // Cancels a pending callback.
  void Cancel();

 private:
  // RetrievePolicyOperation manages its own lifetime.
  ~RetrievePolicyOperation() {}

  // Decodes the policy data and triggers a signature check.
  void OnPolicyRetrieved(const std::string& policy_blob);

  chromeos::SessionManagerClient* session_manager_client_;
  ResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(RetrievePolicyOperation);
};

CrosUserPolicyCache::RetrievePolicyOperation::RetrievePolicyOperation(
    chromeos::SessionManagerClient* session_manager_client,
    const ResultCallback& callback)
    : session_manager_client_(session_manager_client),
      callback_(callback) {}

void CrosUserPolicyCache::RetrievePolicyOperation::Run() {
  session_manager_client_->RetrieveUserPolicy(
      base::Bind(
          &CrosUserPolicyCache::RetrievePolicyOperation::OnPolicyRetrieved,
          base::Unretained(this)));
}

void CrosUserPolicyCache::RetrievePolicyOperation::Cancel() {
  callback_.Reset();
}

void CrosUserPolicyCache::RetrievePolicyOperation::OnPolicyRetrieved(
    const std::string& policy_blob) {
  bool status = true;
  em::PolicyFetchResponse policy;
  if (!policy.ParseFromString(policy_blob) ||
      !policy.has_policy_data() ||
      !policy.has_policy_data_signature()) {
    LOG(ERROR) << "Failed to decode policy";
    status = false;
  }

  if (!callback_.is_null())
    callback_.Run(status, policy);
  delete this;
}

CrosUserPolicyCache::CrosUserPolicyCache(
    chromeos::SessionManagerClient* session_manager_client,
    CloudPolicyDataStore* data_store,
    bool wait_for_policy_fetch,
    const FilePath& legacy_token_cache_file,
    const FilePath& legacy_policy_cache_file)
    : session_manager_client_(session_manager_client),
      data_store_(data_store),
      pending_policy_fetch_(wait_for_policy_fetch),
      pending_disk_cache_load_(true),
      store_operation_(NULL),
      retrieve_operation_(NULL),
      legacy_cache_dir_(legacy_token_cache_file.DirName()),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          legacy_token_cache_delegate_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          legacy_policy_cache_delegate_factory_(this)) {
  DCHECK_EQ(legacy_token_cache_file.DirName().value(),
            legacy_policy_cache_file.DirName().value());

  legacy_token_loader_ =
      new UserPolicyTokenLoader(
          legacy_token_cache_delegate_factory_.GetWeakPtr(),
          legacy_token_cache_file);
  legacy_policy_cache_ =
      new UserPolicyDiskCache(
          legacy_policy_cache_delegate_factory_.GetWeakPtr(),
          legacy_policy_cache_file);
}

CrosUserPolicyCache::~CrosUserPolicyCache() {
  CancelStore();
  CancelRetrieve();
}

void CrosUserPolicyCache::Load() {
  retrieve_operation_ =
      new RetrievePolicyOperation(
          session_manager_client_,
          base::Bind(&CrosUserPolicyCache::OnPolicyLoadDone,
                     base::Unretained(this)));
  retrieve_operation_->Run();
}

bool CrosUserPolicyCache::SetPolicy(const em::PolicyFetchResponse& policy) {
  CancelStore();
  set_last_policy_refresh_time(base::Time::NowFromSystemTime());
  pending_policy_fetch_ = true;
  store_operation_ =
      new StorePolicyOperation(policy,
                               session_manager_client_,
                               base::Bind(&CrosUserPolicyCache::OnPolicyStored,
                                          base::Unretained(this)));
  store_operation_->Run();
  return true;
}

void CrosUserPolicyCache::SetUnmanaged() {
  base::Time now(base::Time::NowFromSystemTime());
  SetUnmanagedInternal(now);

  // Construct a policy blob with unmanaged state.
  em::PolicyData policy_data;
  policy_data.set_policy_type(data_store_->policy_type());
  policy_data.set_timestamp((now - base::Time::UnixEpoch()).InMilliseconds());
  policy_data.set_state(em::PolicyData::UNMANAGED);

  em::PolicyFetchResponse policy;
  if (!policy_data.SerializeToString(policy.mutable_policy_data())) {
    LOG(ERROR) << "Failed to serialize policy_data";
    return;
  }

  SetPolicy(policy);
}

void CrosUserPolicyCache::SetFetchingDone() {
  // If there is a pending policy store or reload, wait for that to complete
  // before reporting fetching done.
  if (store_operation_ || retrieve_operation_)
    return;

  pending_policy_fetch_ = false;
  CheckIfDone();
}

bool CrosUserPolicyCache::DecodePolicyData(const em::PolicyData& policy_data,
                                           PolicyMap* policies) {
  em::CloudPolicySettings policy;
  if (!policy.ParseFromString(policy_data.policy_value())) {
    LOG(WARNING) << "Failed to parse CloudPolicySettings protobuf.";
    return false;
  }
  DecodePolicy(policy, policies);
  return true;
}

void CrosUserPolicyCache::OnTokenLoaded(const std::string& token,
                                        const std::string& device_id) {
  if (token.empty())
    LOG(WARNING) << "Failed to load legacy token cache";

  data_store_->set_device_id(device_id);
  data_store_->SetDeviceToken(token, true);
}

void CrosUserPolicyCache::OnDiskCacheLoaded(
    UserPolicyDiskCache::LoadResult result,
    const em::CachedCloudPolicyResponse& policy) {
  if (result == UserPolicyDiskCache::LOAD_RESULT_SUCCESS) {
    if (policy.unmanaged())
      SetUnmanagedInternal(base::Time::FromTimeT(policy.timestamp()));
    else if (policy.has_cloud_policy())
      InstallLegacyPolicy(policy.cloud_policy());
  } else {
    LOG(WARNING) << "Failed to load legacy policy cache: " << result;
  }

  pending_disk_cache_load_ = false;
  CheckIfDone();
}

void CrosUserPolicyCache::OnPolicyStored(bool result) {
  DCHECK(store_operation_);
  CancelStore();
  if (result) {
    // Policy is stored successfully, reload from session_manager and apply.
    // This helps us making sure we only use policy that session_manager has
    // checked and confirmed to be good.
    CancelRetrieve();
    retrieve_operation_ =
        new RetrievePolicyOperation(
            session_manager_client_,
            base::Bind(&CrosUserPolicyCache::OnPolicyReloadDone,
                       base::Unretained(this)));
    retrieve_operation_->Run();

    // Now that the new policy blob is installed, remove the old cache dir.
    if (!legacy_cache_dir_.empty()) {
      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                              base::Bind(&RemoveLegacyCacheDir,
                                         legacy_cache_dir_));
    }
  } else {
    LOG(ERROR) << "Failed to store user policy.";
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    pending_policy_fetch_ = false;
    CheckIfDone();
  }
}

void CrosUserPolicyCache::OnPolicyLoadDone(
    bool result,
    const em::PolicyFetchResponse& policy) {
  DCHECK(retrieve_operation_);
  CancelRetrieve();
  if (!result) {
    LOG(WARNING) << "No user policy present, trying legacy caches.";
    legacy_token_loader_->Load();
    legacy_policy_cache_->Load();
    return;
  }

  // We have new-style policy, no need to clean up.
  legacy_cache_dir_.clear();

  em::PolicyData policy_data;
  if (!policy_data.ParseFromString(policy.policy_data())) {
    LOG(WARNING) << "Failed to parse PolicyData protobuf.";
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    data_store_->SetDeviceToken(std::string(), true);
  } else if (policy_data.request_token().empty() ||
             policy_data.username().empty() ||
             policy_data.device_id().empty()) {
    LOG(WARNING) << "Policy protobuf is missing credentials";
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
    data_store_->SetDeviceToken(std::string(), true);
  } else {
    data_store_->set_device_id(policy_data.device_id());
    data_store_->SetDeviceToken(policy_data.request_token(), true);
    if (SetPolicyInternal(policy, NULL, true))
      set_last_policy_refresh_time(base::Time::NowFromSystemTime());
  }

  pending_disk_cache_load_ = false;
  CheckIfDone();
}

void CrosUserPolicyCache::OnPolicyReloadDone(
    bool result,
    const em::PolicyFetchResponse& policy) {
  DCHECK(retrieve_operation_);
  CancelRetrieve();
  if (result) {
    if (SetPolicyInternal(policy, NULL, false))
      set_last_policy_refresh_time(base::Time::NowFromSystemTime());
  } else {
    InformNotifier(CloudPolicySubsystem::LOCAL_ERROR,
                   CloudPolicySubsystem::POLICY_LOCAL_ERROR);
  }
  pending_policy_fetch_ = false;
  CheckIfDone();
}

void CrosUserPolicyCache::CancelStore() {
  if (store_operation_) {
    store_operation_->Cancel();
    store_operation_ = NULL;
  }
}

void CrosUserPolicyCache::CancelRetrieve() {
  if (retrieve_operation_) {
    retrieve_operation_->Cancel();
    retrieve_operation_ = NULL;
  }
}

void CrosUserPolicyCache::CheckIfDone() {
  if (!pending_policy_fetch_ && !pending_disk_cache_load_) {
    if (!IsReady())
      SetReady();
    CloudPolicyCacheBase::SetFetchingDone();
  }
}

void CrosUserPolicyCache::InstallLegacyPolicy(
    const em::PolicyFetchResponse& policy) {
  em::PolicyFetchResponse mutable_policy(policy);
  mutable_policy.clear_policy_data_signature();
  mutable_policy.clear_new_public_key();
  mutable_policy.clear_new_public_key_signature();
  em::PolicyData policy_data;
  if (!policy_data.ParseFromString(mutable_policy.policy_data())) {
    LOG(ERROR) << "Failed to parse policy data.";
    return;
  }

  policy_data.clear_public_key_version();
  if (!policy_data.SerializeToString(mutable_policy.mutable_policy_data())) {
    LOG(ERROR) << "Failed to serialize policy data.";
    return;
  }

  base::Time timestamp;
  if (SetPolicyInternal(mutable_policy, &timestamp, true))
    set_last_policy_refresh_time(timestamp);
}

// static
void CrosUserPolicyCache::RemoveLegacyCacheDir(const FilePath& dir) {
  if (!file_util::Delete(dir, true))
    PLOG(ERROR) << "Failed to remove " << dir.value();
}

}  // namespace policy
