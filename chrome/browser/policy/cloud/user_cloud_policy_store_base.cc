// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_cloud_policy_store_base.h"

#include "chrome/browser/policy/cloud/cloud_external_data_manager.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/proto/cloud_policy.pb.h"

namespace policy {

// Decodes a CloudPolicySettings object into a policy map. The implementation is
// generated code in policy/cloud_policy_generated.cc.
void DecodePolicy(const enterprise_management::CloudPolicySettings& policy,
                  base::WeakPtr<CloudExternalDataManager> external_data_manager,
                  PolicyMap* policies);

UserCloudPolicyStoreBase::UserCloudPolicyStoreBase() {
}

UserCloudPolicyStoreBase::~UserCloudPolicyStoreBase() {
}

scoped_ptr<UserCloudPolicyValidator> UserCloudPolicyStoreBase::CreateValidator(
    scoped_ptr<enterprise_management::PolicyFetchResponse> policy,
    CloudPolicyValidatorBase::ValidateTimestampOption timestamp_option) {
  // Configure the validator.
  UserCloudPolicyValidator* validator =
      UserCloudPolicyValidator::Create(policy.Pass());
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
