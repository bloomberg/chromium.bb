// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/asynchronous_policy_loader.h"
#include "chrome/browser/policy/asynchronous_policy_test_base.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_store_interface.h"
#include "chrome/browser/policy/file_based_policy_provider.h"
#include "chrome/browser/policy/mock_configuration_policy_store.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;
using testing::Return;

namespace policy {

class FileBasedPolicyProviderDelegateMock
    : public FileBasedPolicyProvider::ProviderDelegate {
 public:
  FileBasedPolicyProviderDelegateMock()
      : FileBasedPolicyProvider::ProviderDelegate(FilePath()) {}
  MOCK_METHOD0(Load, DictionaryValue*());
  MOCK_METHOD0(GetLastModification, base::Time());
};

TEST_F(AsynchronousPolicyTestBase, ProviderInit) {
  base::Time last_modified;
  FileBasedPolicyProviderDelegateMock* provider_delegate =
      new FileBasedPolicyProviderDelegateMock();
  EXPECT_CALL(*provider_delegate, GetLastModification()).WillRepeatedly(
      Return(last_modified));
  InSequence s;
  EXPECT_CALL(*provider_delegate, Load()).WillOnce(Return(
      new DictionaryValue));
  DictionaryValue* policies = new DictionaryValue();
  policies->SetBoolean(policy::key::kSyncDisabled, true);
  // A second call to Load gets triggered during the provider's construction
  // when the file watcher is initialized, since this file may have changed
  // between the initial load and creating watcher.
  EXPECT_CALL(*provider_delegate, Load()).WillOnce(Return(policies));
  FileBasedPolicyProvider provider(
      ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList(),
      provider_delegate);
  loop_.RunAllPending();
  EXPECT_CALL(*store_, Apply(policy::kPolicySyncDisabled, _)).Times(1);
  provider.Provide(store_.get());
}

TEST_F(AsynchronousPolicyTestBase, ProviderRefresh) {
  base::Time last_modified;
  FileBasedPolicyProviderDelegateMock* provider_delegate =
      new FileBasedPolicyProviderDelegateMock();
  EXPECT_CALL(*provider_delegate, GetLastModification()).WillRepeatedly(
      Return(last_modified));
  InSequence s;
  EXPECT_CALL(*provider_delegate, Load()).WillOnce(Return(
      new DictionaryValue));
  FileBasedPolicyProvider file_based_provider(
          ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList(),
          provider_delegate);
  // A second call to Load gets triggered during the provider's construction
  // when the file watcher is initialized, since this file may have changed
  // between the initial load and creating watcher.
  EXPECT_CALL(*provider_delegate, Load()).WillOnce(Return(
      new DictionaryValue));
  loop_.RunAllPending();
  // A third and final call to Load is made by the explicit Reload. This
  // should be the one that provides the current policy.
  DictionaryValue* policies = new DictionaryValue();
  policies->SetBoolean(policy::key::kSyncDisabled, true);
  EXPECT_CALL(*provider_delegate, Load()).WillOnce(Return(policies));
  file_based_provider.loader()->Reload();
  loop_.RunAllPending();
  EXPECT_CALL(*store_, Apply(policy::kPolicySyncDisabled, _)).Times(1);
  file_based_provider.Provide(store_.get());
}

}  // namespace policy
