// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_STORE_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_STORE_CHROMEOS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store_base.h"
#include "components/signin/core/account_id/account_id.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromeos {
class CryptohomeClient;
}

namespace policy {

// Implements a policy store backed by the Chrome OS' session_manager, which
// takes care of persisting policy to disk and is accessed via DBus calls
// through SessionManagerClient.
// TODO(tnagel): Rename class to reflect that it can store Active Directory
// policy as well. Also think about whether it would make more sense to keep
// cloud and AD policy stores separate and to extract the common functionality
// somewhere else.
class UserCloudPolicyStoreChromeOS : public UserCloudPolicyStoreBase {
 public:
  // Policy validation is relaxed when |is_active_directory| is set, most
  // notably signature validation is disabled.  It is essential that this flag
  // is only set when install attributes are locked into Active Directory mode.
  UserCloudPolicyStoreChromeOS(
      chromeos::CryptohomeClient* cryptohome_client,
      chromeos::SessionManagerClient* session_manager_client,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const AccountId& account_id,
      const base::FilePath& user_policy_key_dir,
      bool is_active_directory);
  ~UserCloudPolicyStoreChromeOS() override;

  // CloudPolicyStore:
  void Store(const enterprise_management::PolicyFetchResponse& policy) override;
  void Load() override;

  // Loads the policy synchronously on the current thread.
  void LoadImmediately();

 private:
  // Starts validation of |policy| before storing it.
  void ValidatePolicyForStore(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy);

  // Completion handler for policy validation on the Store() path.
  // Starts a store operation if the validation succeeded.
  void OnPolicyToStoreValidated(UserCloudPolicyValidator* validator);

  // Called back from SessionManagerClient for policy store operations.
  void OnPolicyStored(bool success);

  // Called back from SessionManagerClient for policy load operations.
  void OnPolicyRetrieved(
      const std::string& policy_blob,
      chromeos::SessionManagerClient::RetrievePolicyResponseType response);

  // Starts validation of the loaded |policy| before installing it.
  void ValidateRetrievedPolicy(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy);

  // Completion handler for policy validation on the Load() path. Installs the
  // policy and publishes it if validation succeeded.
  void OnRetrievedPolicyValidated(UserCloudPolicyValidator* validator);

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

  std::unique_ptr<UserCloudPolicyValidator> CreateValidatorForLoad(
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy);

  chromeos::CryptohomeClient* cryptohome_client_;
  chromeos::SessionManagerClient* session_manager_client_;
  const AccountId account_id_;
  base::FilePath user_policy_key_dir_;
  bool is_active_directory_;

  // The current key used to verify signatures of policy. This value is loaded
  // from the key cache file (which is owned and kept up to date by the Chrome
  // OS session manager). This is, generally, different from
  // |policy_signature_public_key_|, which always corresponds to the currently
  // effective policy.
  std::string cached_policy_key_;
  bool cached_policy_key_loaded_ = false;
  base::FilePath cached_policy_key_path_;

  base::WeakPtrFactory<UserCloudPolicyStoreChromeOS> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStoreChromeOS);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_CLOUD_POLICY_STORE_CHROMEOS_H_
