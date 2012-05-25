// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_STORE_CHROMEOS_H_
#define CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_STORE_CHROMEOS_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/cloud_policy_store.h"

namespace chromeos {
class SessionManagerClient;
}

namespace enterprise_management {
class CloudPolicySettings;
}

namespace policy {

class LegacyPolicyCacheLoader;

// Implements a cloud policy store backed by the Chrome OS' session_manager,
// which takes care of persisting policy to disk and is accessed via DBus calls
// through SessionManagerClient.
//
// Additionally, this class drives legacy UserPolicyTokenCache and
// UserPolicyDiskCache instances, migrating policy from these to session_manager
// storage on the fly.
class UserCloudPolicyStoreChromeOS : public CloudPolicyStore {
 public:
  UserCloudPolicyStoreChromeOS(
      chromeos::SessionManagerClient* session_manager_client,
      const FilePath& legacy_token_cache_file,
      const FilePath& legacy_policy_cache_file);
  virtual ~UserCloudPolicyStoreChromeOS();

  // CloudPolicyStore:
  virtual void Store(
      const enterprise_management::PolicyFetchResponse& policy) OVERRIDE;
  virtual void Load() OVERRIDE;

 private:
  // Called back from SessionManagerClient for policy load operations.
  void OnPolicyRetrieved(const std::string& policy_blob);

  // Called back from SessionManagerClient for policy store operations.
  void OnPolicyStored(bool);

  // Decodes and installs |policy|. This may fail for various reasons, in which
  // case this function updates |status_| and returns false.
  bool InstallPolicy(const enterprise_management::PolicyFetchResponse& policy);

  // Validates a policy blob. This makes sure the policy blob is indeed user
  // policy, is issued to the correct user, the DM Token matches what we already
  // have, and its timestamp is in the past. Updates |status_| appropriately and
  // returns false if any issues are found. On success, |policy_data_ptr| and
  // |cloud_policy_ptr| will be filled in with valid data and true is returned.
  //
  // TODO(mnissler): Performing a signature check here as well would be good,
  // but that requires access to the policy key managed by session_manager,
  // which is stored in a file currently not accessible to Chrome.
  bool Validate(
      const enterprise_management::PolicyFetchResponse& policy,
      enterprise_management::PolicyData* policy_data_ptr,
      enterprise_management::CloudPolicySettings* cloud_policy_ptr);

  // Checks whether the in |policy| matches the currently logged-in user.
  bool CheckPolicyUsername(const enterprise_management::PolicyData& policy);

  // Callback for loading legacy caches.
  void OnLegacyLoadFinished(
      const std::string& dm_token,
      const std::string& device_id,
      bool has_policy,
      const enterprise_management::PolicyFetchResponse& policy,
      Status status);

  // Removes the passed-in legacy cache directory.
  static void RemoveLegacyCacheDir(const FilePath& dir);

  chromeos::SessionManagerClient* session_manager_client_;

  base::WeakPtrFactory<UserCloudPolicyStoreChromeOS> weak_factory_;

  // TODO(mnissler): Remove all the legacy policy support members below after
  // the number of pre-M20 clients drops back to zero.
  FilePath legacy_cache_dir_;
  scoped_ptr<LegacyPolicyCacheLoader> legacy_loader_;
  bool legacy_caches_loaded_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStoreChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_STORE_CHROMEOS_H_
