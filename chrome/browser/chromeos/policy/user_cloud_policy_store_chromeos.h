// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_STORE_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_STORE_CHROMEOS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store_base.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {
class CryptohomeClient;
class SessionManagerClient;
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
class UserCloudPolicyStoreChromeOS : public UserCloudPolicyStoreBase {
 public:
  UserCloudPolicyStoreChromeOS(
      chromeos::CryptohomeClient* cryptohome_client,
      chromeos::SessionManagerClient* session_manager_client,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const std::string& username,
      const base::FilePath& user_policy_key_dir,
      const base::FilePath& legacy_token_cache_file,
      const base::FilePath& legacy_policy_cache_file);
  virtual ~UserCloudPolicyStoreChromeOS();

  // CloudPolicyStore:
  virtual void Store(
      const enterprise_management::PolicyFetchResponse& policy) OVERRIDE;
  virtual void Load() OVERRIDE;

  // Loads the policy synchronously on the current thread.
  void LoadImmediately();

 private:
  // Starts validation of |policy| before storing it.
  void ValidatePolicyForStore(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy);

  // Completion handler for policy validation on the Store() path.
  // Starts a store operation if the validation succeeded.
  void OnPolicyToStoreValidated(UserCloudPolicyValidator* validator);

  // Called back from SessionManagerClient for policy store operations.
  void OnPolicyStored(bool);

  // Called back from SessionManagerClient for policy load operations.
  void OnPolicyRetrieved(const std::string& policy_blob);

  // Starts validation of the loaded |policy| before installing it.
  void ValidateRetrievedPolicy(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy);

  // Completion handler for policy validation on the Load() path. Installs the
  // policy and publishes it if validation succeeded.
  void OnRetrievedPolicyValidated(UserCloudPolicyValidator* validator);

  // Callback for loading legacy caches.
  void OnLegacyLoadFinished(
      const std::string& dm_token,
      const std::string& device_id,
      Status status,
      scoped_ptr<enterprise_management::PolicyFetchResponse>);

  // Completion callback for legacy policy validation.
  void OnLegacyPolicyValidated(const std::string& dm_token,
                               const std::string& device_id,
                               UserCloudPolicyValidator* validator);

  // Installs legacy tokens.
  void InstallLegacyTokens(const std::string& dm_token,
                           const std::string& device_id);

  // Removes the passed-in legacy cache directory.
  static void RemoveLegacyCacheDir(const base::FilePath& dir);

  // Invokes |callback| after reloading |policy_key_|.
  void ReloadPolicyKey(const base::Closure& callback);

  // Reads the contents of |path| into |key|.
  static void LoadPolicyKey(const base::FilePath& path,
                            std::string* key);

  // Callback for the key reloading.
  void OnPolicyKeyReloaded(std::string* key,
                           const base::Closure& callback);

  // Invokes |callback| after creating |policy_key_|, if it hasn't been created
  // yet; otherwise invokes |callback| immediately.
  void EnsurePolicyKeyLoaded(const base::Closure& callback);

  // Callback for getting the sanitized username from |cryptohome_client_|.
  void OnGetSanitizedUsername(const base::Closure& callback,
                              chromeos::DBusMethodCallStatus call_status,
                              const std::string& sanitized_username);

  scoped_ptr<UserCloudPolicyValidator> CreateValidatorForLoad(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy);

  chromeos::CryptohomeClient* cryptohome_client_;
  chromeos::SessionManagerClient* session_manager_client_;
  const std::string username_;
  base::FilePath user_policy_key_dir_;

  base::WeakPtrFactory<UserCloudPolicyStoreChromeOS> weak_factory_;

  // TODO(mnissler): Remove all the legacy policy support members below after
  // the number of pre-M20 clients drops back to zero.
  base::FilePath legacy_cache_dir_;
  scoped_ptr<LegacyPolicyCacheLoader> legacy_loader_;
  bool legacy_caches_loaded_;

  bool policy_key_loaded_;
  base::FilePath policy_key_path_;
  std::string policy_key_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStoreChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_STORE_CHROMEOS_H_
