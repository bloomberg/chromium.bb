// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_STORE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_STORE_H_

#include "base/macros.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class MockCloudPolicyStore : public CloudPolicyStore {
 public:
  MockCloudPolicyStore();
  ~MockCloudPolicyStore() override;

  MOCK_METHOD1(Store, void(const enterprise_management::PolicyFetchResponse&));
  MOCK_METHOD0(Load, void(void));

  void InitPolicyData();

  enterprise_management::PolicyData* GetMutablePolicyData() {
    return policy_.get();
  }

  // Publish the protected members.
  using CloudPolicyStore::NotifyStoreLoaded;
  using CloudPolicyStore::NotifyStoreError;

  using CloudPolicyStore::policy_map_;
  using CloudPolicyStore::policy_;
  using CloudPolicyStore::status_;
  using CloudPolicyStore::invalidation_version_;
  using CloudPolicyStore::policy_signature_public_key_;
  using CloudPolicyStore::validation_result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudPolicyStore);
};

class MockCloudPolicyStoreObserver : public CloudPolicyStore::Observer {
 public:
  MockCloudPolicyStoreObserver();
  ~MockCloudPolicyStoreObserver() override;

  MOCK_METHOD1(OnStoreLoaded, void(CloudPolicyStore* store));
  MOCK_METHOD1(OnStoreError, void(CloudPolicyStore* store));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudPolicyStoreObserver);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_STORE_H_
