// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CROS_USER_POLICY_CACHE_H_
#define CHROME_BROWSER_POLICY_CROS_USER_POLICY_CACHE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/cloud_policy_cache_base.h"
#include "chrome/browser/policy/user_policy_disk_cache.h"
#include "chrome/browser/policy/user_policy_token_cache.h"

namespace chromeos {
class SessionManagerClient;
}

namespace policy {

class CloudPolicyDataStore;

// User policy cache that talks to the ChromeOS login library in order to store
// and fetch policy data.
class CrosUserPolicyCache : public CloudPolicyCacheBase,
                            public UserPolicyTokenCache::Delegate,
                            public UserPolicyDiskCache::Delegate {
 public:
  CrosUserPolicyCache(chromeos::SessionManagerClient* session_manager_client,
                      CloudPolicyDataStore* data_store,
                      bool wait_for_policy_fetch,
                      const FilePath& legacy_token_cache_file,
                      const FilePath& legacy_policy_cache_file);
  virtual ~CrosUserPolicyCache();

  // CloudPolicyCacheBase implementation.
  virtual void Load() OVERRIDE;
  virtual bool SetPolicy(
      const enterprise_management::PolicyFetchResponse& policy) OVERRIDE;
  virtual void SetUnmanaged() OVERRIDE;
  virtual void SetFetchingDone() OVERRIDE;

 protected:
  virtual bool DecodePolicyData(
      const enterprise_management::PolicyData& policy_data,
      PolicyMap* policies) OVERRIDE;

 private:
  class StorePolicyOperation;
  class RetrievePolicyOperation;

  // UserPolicyTokenLoader::Delegate:
  virtual void OnTokenLoaded(const std::string& token,
                             const std::string& device_id) OVERRIDE;

  // UserPolicyDiskCache::Delegate:
  virtual void OnDiskCacheLoaded(
      UserPolicyDiskCache::LoadResult result,
      const enterprise_management::CachedCloudPolicyResponse& policy) OVERRIDE;

  // Used as a callback for the policy store operation.
  void OnPolicyStored(bool result);

  // Callback for the initial policy load. Installs the policy and passes the
  // loaded token and device ID to the data store.
  void OnPolicyLoadDone(
      bool result,
      const enterprise_management::PolicyFetchResponse& policy);

  // Callback for the policy retrieval operation run to reload the policy after
  // new policy has been successfully stored. Installs the new policy in the
  // cache and publishes it if successful.
  void OnPolicyReloadDone(
      bool result,
      const enterprise_management::PolicyFetchResponse& policy);

  void CancelStore();
  void CancelRetrieve();

  // Checks whether the disk cache and (if requested) the policy fetch
  // (including the DBus roundtrips) has completed and generates ready or
  // fetching done notifications if this is the case.
  void CheckIfDone();

  // Installs legacy policy, mangling it to remove any public keys, public key
  // versions and signatures. This is done so on the next policy fetch the
  // server ships down a new policy to be sent down to session_manager.
  void InstallLegacyPolicy(
      const enterprise_management::PolicyFetchResponse& policy);

  // Removes the legacy cache dir.
  static void RemoveLegacyCacheDir(const FilePath& dir);

  chromeos::SessionManagerClient* session_manager_client_;
  CloudPolicyDataStore* data_store_;

  // Whether a policy fetch is pending before readiness is asserted.
  bool pending_policy_fetch_;
  bool pending_disk_cache_load_;

  // Storage and retrieval operations that are currently in flight.
  StorePolicyOperation* store_operation_;
  RetrievePolicyOperation* retrieve_operation_;

  // TODO(mnissler): Remove all the legacy policy support members below after
  // the number of pre-M20 clients drops back to zero.
  FilePath legacy_cache_dir_;

  base::WeakPtrFactory<UserPolicyTokenCache::Delegate>
      legacy_token_cache_delegate_factory_;
  scoped_refptr<UserPolicyTokenLoader> legacy_token_loader_;

  base::WeakPtrFactory<UserPolicyDiskCache::Delegate>
      legacy_policy_cache_delegate_factory_;
  scoped_refptr<UserPolicyDiskCache> legacy_policy_cache_;

  DISALLOW_COPY_AND_ASSIGN(CrosUserPolicyCache);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CROS_USER_POLICY_CACHE_H_
