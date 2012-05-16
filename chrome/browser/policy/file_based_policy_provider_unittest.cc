// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/policy/asynchronous_policy_loader.h"
#include "chrome/browser/policy/asynchronous_policy_test_base.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/file_based_policy_provider.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InSequence;
using testing::Return;
using testing::_;

namespace policy {

class FileBasedPolicyProviderDelegateMock
    : public FileBasedPolicyProvider::ProviderDelegate {
 public:
  FileBasedPolicyProviderDelegateMock()
      : FileBasedPolicyProvider::ProviderDelegate(FilePath()) {}

  // Load() returns a scoped_ptr<PolicyBundle> but it can't be mocked because
  // scoped_ptr is moveable but not copyable. This override forwards the
  // call to MockLoad() which returns a PolicyBundle*, and returns a copy
  // wrapped in a passed scoped_ptr.
  virtual scoped_ptr<PolicyBundle> Load() OVERRIDE {
    scoped_ptr<PolicyBundle> bundle;
    PolicyBundle* loaded = MockLoad();
    if (loaded) {
      bundle.reset(new PolicyBundle());
      bundle->CopyFrom(*loaded);
    }
    return bundle.Pass();
  }

  MOCK_METHOD0(MockLoad, PolicyBundle*());
  MOCK_METHOD0(GetLastModification, base::Time());
};

TEST_F(AsynchronousPolicyTestBase, ProviderInit) {
  base::Time last_modified;
  FileBasedPolicyProviderDelegateMock* provider_delegate =
      new FileBasedPolicyProviderDelegateMock();
  EXPECT_CALL(*provider_delegate, GetLastModification()).WillRepeatedly(
      Return(last_modified));
  InSequence s;
  PolicyBundle empty_bundle;
  EXPECT_CALL(*provider_delegate, MockLoad()).WillOnce(Return(&empty_bundle));
  PolicyBundle bundle;
  bundle.Get(POLICY_DOMAIN_CHROME, "")
      .Set(key::kSyncDisabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           base::Value::CreateBooleanValue(true));
  // A second call to Load gets triggered during the provider's construction
  // when the file watcher is initialized, since this file may have changed
  // between the initial load and creating watcher.
  EXPECT_CALL(*provider_delegate, MockLoad()).WillOnce(Return(&bundle));
  FileBasedPolicyProvider provider(GetChromePolicyDefinitionList(),
                                   provider_delegate);
  loop_.RunAllPending();
  EXPECT_TRUE(provider.policies().Equals(bundle));
}

TEST_F(AsynchronousPolicyTestBase, ProviderRefresh) {
  base::Time last_modified;
  FileBasedPolicyProviderDelegateMock* provider_delegate =
      new FileBasedPolicyProviderDelegateMock();
  EXPECT_CALL(*provider_delegate, GetLastModification()).WillRepeatedly(
      Return(last_modified));
  InSequence s;
  PolicyBundle empty_bundle;
  EXPECT_CALL(*provider_delegate, MockLoad()).WillOnce(Return(&empty_bundle));
  FileBasedPolicyProvider file_based_provider(GetChromePolicyDefinitionList(),
                                              provider_delegate);
  // A second call to Load gets triggered during the provider's construction
  // when the file watcher is initialized, since this file may have changed
  // between the initial load and creating watcher.
  EXPECT_CALL(*provider_delegate, MockLoad()).WillOnce(Return(&empty_bundle));
  loop_.RunAllPending();
  // A third and final call to Load is made by the explicit Reload. This
  // should be the one that provides the current policy.
  PolicyBundle bundle;
  bundle.Get(POLICY_DOMAIN_CHROME, "")
      .Set(key::kSyncDisabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
           base::Value::CreateBooleanValue(true));
  EXPECT_CALL(*provider_delegate, MockLoad()).WillOnce(Return(&bundle));
  MockConfigurationPolicyObserver observer;
  ConfigurationPolicyObserverRegistrar registrar;
  registrar.Init(&file_based_provider, &observer);
  EXPECT_CALL(observer, OnUpdatePolicy(&file_based_provider)).Times(1);
  file_based_provider.RefreshPolicies();
  loop_.RunAllPending();
  EXPECT_TRUE(file_based_provider.policies().Equals(bundle));
}

}  // namespace policy
