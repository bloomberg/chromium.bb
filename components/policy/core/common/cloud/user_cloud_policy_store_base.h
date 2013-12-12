// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_STORE_BASE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_STORE_BASE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/policy_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

// Base class that implements common cross-platform UserCloudPolicyStore
// functionality.
class POLICY_EXPORT UserCloudPolicyStoreBase : public CloudPolicyStore {
 public:
  explicit UserCloudPolicyStoreBase(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  virtual ~UserCloudPolicyStoreBase();

 protected:
  // Creates a validator configured to validate a user policy. The caller owns
  // the resulting object until StartValidation() is invoked.
  scoped_ptr<UserCloudPolicyValidator> CreateValidator(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy,
      CloudPolicyValidatorBase::ValidateTimestampOption option);

  // Sets |policy_data| and |payload| as the active policy.
  void InstallPolicy(
      scoped_ptr<enterprise_management::PolicyData> policy_data,
      scoped_ptr<enterprise_management::CloudPolicySettings> payload);

  scoped_refptr<base::SequencedTaskRunner> background_task_runner() const {
    return background_task_runner_;
  }

 private:
  // Task runner for background file operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStoreBase);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_STORE_BASE_H_
