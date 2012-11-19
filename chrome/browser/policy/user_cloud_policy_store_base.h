// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_STORE_BASE_H_
#define CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_STORE_BASE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/cloud_policy_store.h"

namespace policy {

// Base class that implements common cross-platform UserCloudPolicyStore
// functionality.
class UserCloudPolicyStoreBase : public CloudPolicyStore {
 public:
  UserCloudPolicyStoreBase();
  virtual ~UserCloudPolicyStoreBase();

 protected:
  // Creates a validator configured to validate a user policy. The caller owns
  // the resulting object until StartValidation() is invoked.
  scoped_ptr<UserCloudPolicyValidator> CreateValidator(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy);

  // Sets |policy_data| and |payload| as the active policy.
  void InstallPolicy(
      scoped_ptr<enterprise_management::PolicyData> policy_data,
      scoped_ptr<enterprise_management::CloudPolicySettings> payload);

 private:
  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStoreBase);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_USER_CLOUD_POLICY_STORE_BASE_H_
