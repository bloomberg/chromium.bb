// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/mock_cloud_policy_client.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

MockCloudPolicyClient::MockCloudPolicyClient()
    : CloudPolicyClient("", "", USER_AFFILIATION_NONE, NULL, NULL) {}

MockCloudPolicyClient::~MockCloudPolicyClient() {}

void MockCloudPolicyClient::SetDMToken(const std::string& token) {
  dm_token_ = token;
}

void MockCloudPolicyClient::SetPolicy(const PolicyNamespaceKey& policy_ns_key,
                                      const em::PolicyFetchResponse& policy) {
  em::PolicyFetchResponse*& response = responses_[policy_ns_key];
  delete response;
  response = new enterprise_management::PolicyFetchResponse(policy);
}

void MockCloudPolicyClient::SetStatus(DeviceManagementStatus status) {
  status_ = status;
}

}  // namespace policy
