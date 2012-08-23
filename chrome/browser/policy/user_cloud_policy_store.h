// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_STORE_H_
#define CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_STORE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/user_cloud_policy_store_base.h"

namespace policy {

// Implements a cloud policy store that is stored in a simple file in the user's
// profile directory. This is used on (non-chromeos) platforms that do not have
// a secure storage implementation.
class UserCloudPolicyStore : public UserCloudPolicyStoreBase {
 public:
  // Creates a policy store associated with the user signed in to this
  // |profile|.
  UserCloudPolicyStore(Profile* profile, const FilePath& policy_file);
  virtual ~UserCloudPolicyStore();

  // CloudPolicyStore implementation.
  virtual void Load() OVERRIDE;
  virtual void Store(
      const enterprise_management::PolicyFetchResponse& policy) OVERRIDE;

 protected:
  virtual void RemoveStoredPolicy() OVERRIDE;

 private:

  // Callback invoked when a new policy has been loaded from disk.
  void PolicyLoaded(struct PolicyLoadResult policy_load_result);

  // Starts policy blob validation. |callback| is invoked once validation is
  // complete.
  void Validate(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy,
      const UserCloudPolicyValidator::CompletionCallback& callback);

  // Callback invoked to install a just-loaded policy after validation has
  // finished.
  void InstallLoadedPolicyAfterValidation(UserCloudPolicyValidator* validator);

  // Callback invoked to store the policy after validation has finished.
  void StorePolicyAfterValidation(UserCloudPolicyValidator* validator);

  // WeakPtrFactory used to create callbacks for validating and storing policy.
  base::WeakPtrFactory<UserCloudPolicyStore> weak_factory_;

  // Weak pointer to the profile associated with this store.
  Profile* profile_;

  // Path to file where we store persisted policy.
  FilePath backing_file_path_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStore);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_STORE_H_
