// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/location.h"
#include "base/test/bind_test_util.h"
#include "net/network_error_logging/mock_persistent_nel_store.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {
const url::Origin kOrigin = url::Origin::Create(GURL("https://example.test/"));

NetworkErrorLoggingService::NELPolicy MakePolicyForOrigin(url::Origin origin) {
  NetworkErrorLoggingService::NELPolicy policy;
  policy.origin = std::move(origin);
  policy.expires = base::Time();
  policy.last_used = base::Time();

  return policy;
}

void RunClosureOnNELPoliciesLoaded(
    base::OnceClosure closure,
    std::vector<NetworkErrorLoggingService::NELPolicy>* policies_out,
    std::vector<NetworkErrorLoggingService::NELPolicy> loaded_policies) {
  std::move(closure).Run();
  loaded_policies.swap(*policies_out);
}

// Makes a NELPoliciesLoadedCallback that will fail if it's never run before
// destruction.
MockPersistentNELStore::NELPoliciesLoadedCallback
MakeExpectedRunNELPoliciesLoadedCallback(
    std::vector<NetworkErrorLoggingService::NELPolicy>* policies_out) {
  base::OnceClosure closure = base::MakeExpectedRunClosure(FROM_HERE);
  return base::BindOnce(&RunClosureOnNELPoliciesLoaded, std::move(closure),
                        policies_out);
}

// Test that FinishLoading() runs the callback.
TEST(MockPersistentNELStoreTest, FinishLoading) {
  MockPersistentNELStore store;
  MockPersistentNELStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NELPolicy> loaded_policies;

  store.LoadNELPolicies(
      MakeExpectedRunNELPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNELStore::Command::Type::LOAD_NEL_POLICIES);

  store.FinishLoading(true /* load_success */);
  EXPECT_EQ(0u, loaded_policies.size());

  EXPECT_EQ(1u, store.GetAllCommands().size());
  EXPECT_TRUE(store.VerifyCommands(expected_commands));
  EXPECT_EQ("LOAD; ", store.GetDebugString());

  // Test should not crash because the callback has been run.
}

TEST(MockPersistentNELStoreTest, PreStoredPolicies) {
  MockPersistentNELStore store;
  MockPersistentNELStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NELPolicy> loaded_policies;

  std::vector<NetworkErrorLoggingService::NELPolicy> prestored_policies = {
      MakePolicyForOrigin(kOrigin)};
  store.SetPrestoredPolicies(std::move(prestored_policies));
  EXPECT_EQ(1, store.StoredPoliciesCount());

  store.LoadNELPolicies(
      MakeExpectedRunNELPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNELStore::Command::Type::LOAD_NEL_POLICIES);
  store.FinishLoading(true /* load_success */);
  ASSERT_EQ(1u, loaded_policies.size());
  EXPECT_EQ(kOrigin, loaded_policies[0].origin);

  EXPECT_EQ(1u, store.GetAllCommands().size());
  EXPECT_TRUE(store.VerifyCommands(expected_commands));
  EXPECT_EQ("LOAD; ", store.GetDebugString());
}

// Failed load should yield empty vector of policies.
TEST(MockPersistentNELStoreTest, FailedLoad) {
  MockPersistentNELStore store;
  MockPersistentNELStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NELPolicy> loaded_policies;

  std::vector<NetworkErrorLoggingService::NELPolicy> prestored_policies = {
      MakePolicyForOrigin(kOrigin)};
  store.SetPrestoredPolicies(std::move(prestored_policies));
  EXPECT_EQ(1, store.StoredPoliciesCount());

  store.LoadNELPolicies(
      MakeExpectedRunNELPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNELStore::Command::Type::LOAD_NEL_POLICIES);
  store.FinishLoading(false /* load_success */);
  // The pre-stored policy is not returned because loading failed.
  EXPECT_EQ(0u, loaded_policies.size());

  EXPECT_EQ(1u, store.GetAllCommands().size());
  EXPECT_TRUE(store.VerifyCommands(expected_commands));
  EXPECT_EQ("LOAD; ", store.GetDebugString());
}

TEST(MockPersistentNELStoreTest, Add) {
  MockPersistentNELStore store;
  MockPersistentNELStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NELPolicy> loaded_policies;

  store.LoadNELPolicies(
      MakeExpectedRunNELPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNELStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_EQ(1u, store.GetAllCommands().size());

  store.FinishLoading(true /* load_success */);
  EXPECT_EQ(0u, loaded_policies.size());

  NetworkErrorLoggingService::NELPolicy policy = MakePolicyForOrigin(kOrigin);
  store.AddNELPolicy(policy);
  expected_commands.emplace_back(
      MockPersistentNELStore::Command::Type::ADD_NEL_POLICY, policy);
  // Add operation will be queued; the policy has not actually been stored yet
  EXPECT_EQ(0, store.StoredPoliciesCount());
  EXPECT_EQ(2u, store.GetAllCommands().size());

  store.Flush();
  expected_commands.emplace_back(MockPersistentNELStore::Command::Type::FLUSH);
  EXPECT_EQ(1, store.StoredPoliciesCount());

  EXPECT_EQ(3u, store.GetAllCommands().size());
  EXPECT_TRUE(store.VerifyCommands(expected_commands));
  EXPECT_EQ("LOAD; ADD(" + kOrigin.Serialize() + "); FLUSH; ",
            store.GetDebugString());
}

TEST(MockPersistentNELStoreTest, AddThenDelete) {
  MockPersistentNELStore store;
  MockPersistentNELStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NELPolicy> loaded_policies;

  store.LoadNELPolicies(
      MakeExpectedRunNELPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNELStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_EQ(1u, store.GetAllCommands().size());

  store.FinishLoading(true /* load_success */);
  EXPECT_EQ(0u, loaded_policies.size());

  NetworkErrorLoggingService::NELPolicy policy = MakePolicyForOrigin(kOrigin);
  store.AddNELPolicy(policy);
  expected_commands.emplace_back(
      MockPersistentNELStore::Command::Type::ADD_NEL_POLICY, policy);
  EXPECT_EQ(2u, store.GetAllCommands().size());

  store.DeleteNELPolicy(policy);
  expected_commands.emplace_back(
      MockPersistentNELStore::Command::Type::DELETE_NEL_POLICY, policy);
  EXPECT_EQ(3u, store.GetAllCommands().size());

  store.Flush();
  expected_commands.emplace_back(MockPersistentNELStore::Command::Type::FLUSH);
  EXPECT_EQ(0, store.StoredPoliciesCount());
  EXPECT_EQ(4u, store.GetAllCommands().size());

  EXPECT_TRUE(store.VerifyCommands(expected_commands));
  EXPECT_EQ("LOAD; ADD(" + kOrigin.Serialize() +
                "); "
                "DELETE(" +
                kOrigin.Serialize() + "); FLUSH; ",
            store.GetDebugString());
}

TEST(MockPersistentNELStoreTest, AddFlushThenDelete) {
  MockPersistentNELStore store;
  MockPersistentNELStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NELPolicy> loaded_policies;

  store.LoadNELPolicies(
      MakeExpectedRunNELPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNELStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_EQ(1u, store.GetAllCommands().size());

  store.FinishLoading(true /* load_success */);
  EXPECT_EQ(0u, loaded_policies.size());

  NetworkErrorLoggingService::NELPolicy policy = MakePolicyForOrigin(kOrigin);
  store.AddNELPolicy(policy);
  expected_commands.emplace_back(
      MockPersistentNELStore::Command::Type::ADD_NEL_POLICY, policy);
  EXPECT_EQ(2u, store.GetAllCommands().size());

  store.Flush();
  expected_commands.emplace_back(MockPersistentNELStore::Command::Type::FLUSH);
  EXPECT_EQ(1, store.StoredPoliciesCount());
  EXPECT_EQ(3u, store.GetAllCommands().size());

  store.DeleteNELPolicy(policy);
  expected_commands.emplace_back(
      MockPersistentNELStore::Command::Type::DELETE_NEL_POLICY, policy);
  EXPECT_EQ(4u, store.GetAllCommands().size());

  store.Flush();
  expected_commands.emplace_back(MockPersistentNELStore::Command::Type::FLUSH);
  EXPECT_EQ(0, store.StoredPoliciesCount());
  EXPECT_EQ(5u, store.GetAllCommands().size());

  EXPECT_TRUE(store.VerifyCommands(expected_commands));
  EXPECT_EQ("LOAD; ADD(" + kOrigin.Serialize() +
                "); FLUSH; "
                "DELETE(" +
                kOrigin.Serialize() + "); FLUSH; ",
            store.GetDebugString());
}

TEST(MockPersistentNELStoreTest, AddThenUpdate) {
  MockPersistentNELStore store;
  MockPersistentNELStore::CommandList expected_commands;
  std::vector<NetworkErrorLoggingService::NELPolicy> loaded_policies;

  store.LoadNELPolicies(
      MakeExpectedRunNELPoliciesLoadedCallback(&loaded_policies));
  expected_commands.emplace_back(
      MockPersistentNELStore::Command::Type::LOAD_NEL_POLICIES);
  EXPECT_EQ(1u, store.GetAllCommands().size());

  store.FinishLoading(true /* load_success */);

  NetworkErrorLoggingService::NELPolicy policy = MakePolicyForOrigin(kOrigin);
  store.AddNELPolicy(policy);
  expected_commands.emplace_back(
      MockPersistentNELStore::Command::Type::ADD_NEL_POLICY, policy);
  EXPECT_EQ(2u, store.GetAllCommands().size());

  store.UpdateNELPolicyAccessTime(policy);
  expected_commands.emplace_back(
      MockPersistentNELStore::Command::Type::UPDATE_NEL_POLICY, policy);
  EXPECT_EQ(3u, store.GetAllCommands().size());

  store.Flush();
  expected_commands.emplace_back(MockPersistentNELStore::Command::Type::FLUSH);
  EXPECT_EQ(1, store.StoredPoliciesCount());
  EXPECT_EQ(4u, store.GetAllCommands().size());

  EXPECT_TRUE(store.VerifyCommands(expected_commands));
  EXPECT_EQ("LOAD; ADD(" + kOrigin.Serialize() +
                "); "
                "UPDATE(" +
                kOrigin.Serialize() + "); FLUSH; ",
            store.GetDebugString());
}

}  // namespace

}  // namespace net
