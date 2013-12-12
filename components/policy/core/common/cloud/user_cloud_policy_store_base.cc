// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_cloud_policy_store_base.h"

#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/policy_map.h"
#include "policy/proto/cloud_policy.pb.h"

namespace policy {

// Decodes a CloudPolicySettings object into a policy map. The implementation is
// generated code in policy/cloud_policy_generated.cc.
void DecodePolicy(const enterprise_management::CloudPolicySettings& policy,
                  base::WeakPtr<CloudExternalDataManager> external_data_manager,
                  PolicyMap* policies);

UserCloudPolicyStoreBase::UserCloudPolicyStoreBase(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : background_task_runner_(background_task_runner) {}

UserCloudPolicyStoreBase::~UserCloudPolicyStoreBase() {
}

scoped_ptr<UserCloudPolicyValidator> UserCloudPolicyStoreBase::CreateValidator(
    scoped_ptr<enterprise_management::PolicyFetchResponse> policy,
    CloudPolicyValidatorBase::ValidateTimestampOption timestamp_option) {
  // Configure the validator.
  UserCloudPolicyValidator* validator =
      UserCloudPolicyValidator::Create(policy.Pass(), background_task_runner_);
  validator->ValidatePolicyType(GetChromeUserPolicyType());
  validator->ValidateAgainstCurrentPolicy(
      policy_.get(),
      timestamp_option,
      CloudPolicyValidatorBase::DM_TOKEN_REQUIRED);
  validator->ValidatePayload();
  return scoped_ptr<UserCloudPolicyValidator>(validator);
}

void UserCloudPolicyStoreBase::InstallPolicy(
    scoped_ptr<enterprise_management::PolicyData> policy_data,
    scoped_ptr<enterprise_management::CloudPolicySettings> payload) {
  // Decode the payload.
  policy_map_.Clear();
  DecodePolicy(*payload, external_data_manager(), &policy_map_);
  policy_ = policy_data.Pass();
}

}  // namespace policy
