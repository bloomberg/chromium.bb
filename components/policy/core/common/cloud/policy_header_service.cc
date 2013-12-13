// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/policy_header_service.h"

#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_header_io_helper.h"

namespace policy {

PolicyHeaderService::PolicyHeaderService(const std::string& server_url,
                                         CloudPolicyStore* user_policy_store,
                                         CloudPolicyStore* device_policy_store)
    : server_url_(server_url),
      user_policy_store_(user_policy_store),
      device_policy_store_(device_policy_store) {
  user_policy_store_->AddObserver(this);
  if (device_policy_store_)
    device_policy_store_->AddObserver(this);
}

PolicyHeaderService::~PolicyHeaderService() {
  user_policy_store_->RemoveObserver(this);
  if (device_policy_store_)
    device_policy_store_->RemoveObserver(this);
}

scoped_ptr<PolicyHeaderIOHelper>
PolicyHeaderService::CreatePolicyHeaderIOHelper(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  std::string initial_header_value = CreateHeaderValue();
  scoped_ptr<PolicyHeaderIOHelper> helper = make_scoped_ptr(
      new PolicyHeaderIOHelper(server_url_, initial_header_value, task_runner));
  helpers_.push_back(helper.get());
  return helper.Pass();
}

std::string PolicyHeaderService::CreateHeaderValue() {
  // TODO(atwilson): Extract policy information and generate correct header.
  return "";
}

void PolicyHeaderService::OnStoreLoaded(CloudPolicyStore* store) {
  // If we have a PolicyHeaderIOHelper, notify it of the new header value.
  if (!helpers_.empty()) {
    std::string new_header = CreateHeaderValue();
    for (std::vector<PolicyHeaderIOHelper*>::const_iterator it =
             helpers_.begin(); it != helpers_.end(); ++it) {
      (*it)->UpdateHeader(new_header);
    }
  }
}

void PolicyHeaderService::OnStoreError(CloudPolicyStore* store) {
  // Do nothing on errors.
}

}  // namespace policy
