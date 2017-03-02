// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/subresource_filter/core/common/proto/rules.pb.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

class AsyncDocumentSubresourceFilterTest : public ::testing::Test {
 public:
  AsyncDocumentSubresourceFilterTest() = default;

 protected:
  void SetUp() override {
    std::vector<proto::UrlRule> rules;
    rules.push_back(testing::CreateWhitelistRuleForDocument(
        "whitelisted.subframe.com", proto::ACTIVATION_TYPE_GENERICBLOCK,
        {"example.com"}));
    rules.push_back(testing::CreateSuffixRule("disallowed.html"));

    ASSERT_NO_FATAL_FAILURE(test_ruleset_creator_.CreateRulesetWithRules(
        rules, &test_ruleset_pair_));

    dealer_handle_.reset(
        new VerifiedRulesetDealer::Handle(blocking_task_runner_));
  }

  void TearDown() override {
    dealer_handle_.reset(nullptr);
    RunUntilIdle();
  }

  const testing::TestRuleset& ruleset() const {
    return test_ruleset_pair_.indexed;
  }

  void RunUntilIdle() {
    base::RunLoop().RunUntilIdle();
    while (blocking_task_runner_->HasPendingTask()) {
      blocking_task_runner_->RunUntilIdle();
      base::RunLoop().RunUntilIdle();
    }
  }

  VerifiedRulesetDealer::Handle* dealer_handle() {
    return dealer_handle_.get();
  }

  std::unique_ptr<VerifiedRuleset::Handle> CreateRulesetHandle() {
    return base::MakeUnique<VerifiedRuleset::Handle>(dealer_handle());
  }

 private:
  testing::TestRulesetCreator test_ruleset_creator_;
  testing::TestRulesetPair test_ruleset_pair_;

  // Note: ADSF assumes a task runner is associated with the current thread.
  // Instantiate a MessageLoop on the current thread and use RunLoop to handle
  // the replies ADSF tasks generate.
  base::MessageLoop message_loop_;
  scoped_refptr<base::TestSimpleTaskRunner> blocking_task_runner_ =
      new base::TestSimpleTaskRunner;

  std::unique_ptr<VerifiedRulesetDealer::Handle> dealer_handle_;

  DISALLOW_COPY_AND_ASSIGN(AsyncDocumentSubresourceFilterTest);
};

namespace {

class TestActivationStateCallbackReceiver {
 public:
  TestActivationStateCallbackReceiver() = default;

  base::Callback<void(ActivationState)> callback() {
    return base::Bind(&TestActivationStateCallbackReceiver::Callback,
                      base::Unretained(this));
  }
  void ExpectReceivedOnce(ActivationState expected_state) const {
    ASSERT_EQ(1, callback_count_);
    EXPECT_EQ(expected_state, last_activation_state_);
  }

 private:
  void Callback(ActivationState activation_state) {
    ++callback_count_;
    last_activation_state_ = activation_state;
  }

  ActivationState last_activation_state_;
  int callback_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestActivationStateCallbackReceiver);
};

class TestCallbackReceiver {
 public:
  TestCallbackReceiver() = default;

  base::Closure closure() {
    return base::Bind(&TestCallbackReceiver::Callback, base::Unretained(this));
  }
  int callback_count() const { return callback_count_; }

 private:
  void Callback() { ++callback_count_; }

  int callback_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestCallbackReceiver);
};

class LoadPolicyCallbackReceiver {
 public:
  LoadPolicyCallbackReceiver() = default;

  AsyncDocumentSubresourceFilter::LoadPolicyCallback callback() {
    return base::Bind(&LoadPolicyCallbackReceiver::Callback,
                      base::Unretained(this));
  }
  void ExpectReceivedOnce(LoadPolicy load_policy) const {
    ASSERT_EQ(1, callback_count_);
    EXPECT_EQ(load_policy, last_load_policy_);
  }

 private:
  void Callback(LoadPolicy load_policy) {
    ++callback_count_;
    last_load_policy_ = load_policy;
  }

  int callback_count_ = 0;
  LoadPolicy last_load_policy_;

  DISALLOW_COPY_AND_ASSIGN(LoadPolicyCallbackReceiver);
};

}  // namespace

TEST_F(AsyncDocumentSubresourceFilterTest, ActivationStateIsReported) {
  dealer_handle()->SetRulesetFile(testing::TestRuleset::Open(ruleset()));
  auto ruleset_handle = CreateRulesetHandle();

  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("http://example.com"), ActivationLevel::ENABLED, false);

  TestActivationStateCallbackReceiver activation_state;
  auto filter = base::MakeUnique<AsyncDocumentSubresourceFilter>(
      ruleset_handle.get(), std::move(params), activation_state.callback(),
      base::OnceClosure());

  RunUntilIdle();
  activation_state.ExpectReceivedOnce(
      ActivationState(ActivationLevel::ENABLED));
}

TEST_F(AsyncDocumentSubresourceFilterTest, ActivationStateIsComputedCorrectly) {
  dealer_handle()->SetRulesetFile(testing::TestRuleset::Open(ruleset()));
  auto ruleset_handle = CreateRulesetHandle();

  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("http://whitelisted.subframe.com"), ActivationLevel::ENABLED, false);
  params.parent_document_origin = url::Origin(GURL("http://example.com"));

  TestActivationStateCallbackReceiver activation_state;
  auto filter = base::MakeUnique<AsyncDocumentSubresourceFilter>(
      ruleset_handle.get(), std::move(params), activation_state.callback(),
      base::OnceClosure());

  RunUntilIdle();

  ActivationState expected_activation_state(ActivationLevel::ENABLED);
  expected_activation_state.generic_blocking_rules_disabled = true;
  activation_state.ExpectReceivedOnce(expected_activation_state);
}

TEST_F(AsyncDocumentSubresourceFilterTest, DisabledForCorruptRuleset) {
  testing::TestRuleset::CorruptByFilling(ruleset(), 0, 100, 0xFF);
  dealer_handle()->SetRulesetFile(testing::TestRuleset::Open(ruleset()));

  auto ruleset_handle = CreateRulesetHandle();

  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("http://example.com"), ActivationLevel::ENABLED, false);

  TestActivationStateCallbackReceiver activation_state;
  auto filter = base::MakeUnique<AsyncDocumentSubresourceFilter>(
      ruleset_handle.get(), std::move(params), activation_state.callback(),
      base::OnceClosure());

  RunUntilIdle();
  activation_state.ExpectReceivedOnce(
      ActivationState(ActivationLevel::DISABLED));
}

TEST_F(AsyncDocumentSubresourceFilterTest, GetLoadPolicyForSubdocument) {
  dealer_handle()->SetRulesetFile(testing::TestRuleset::Open(ruleset()));
  auto ruleset_handle = CreateRulesetHandle();

  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("http://example.com"), ActivationLevel::ENABLED, false);

  TestActivationStateCallbackReceiver activation_state;
  auto filter = base::MakeUnique<AsyncDocumentSubresourceFilter>(
      ruleset_handle.get(), std::move(params), activation_state.callback(),
      base::OnceClosure());

  LoadPolicyCallbackReceiver load_policy_1;
  LoadPolicyCallbackReceiver load_policy_2;
  filter->GetLoadPolicyForSubdocument(GURL("http://example.com/allowed.html"),
                                      load_policy_1.callback());
  filter->GetLoadPolicyForSubdocument(
      GURL("http://example.com/disallowed.html"), load_policy_2.callback());

  RunUntilIdle();
  load_policy_1.ExpectReceivedOnce(LoadPolicy::ALLOW);
  load_policy_2.ExpectReceivedOnce(LoadPolicy::DISALLOW);
}

TEST_F(AsyncDocumentSubresourceFilterTest, FirstDisallowedLoadIsReported) {
  dealer_handle()->SetRulesetFile(testing::TestRuleset::Open(ruleset()));
  auto ruleset_handle = CreateRulesetHandle();

  TestCallbackReceiver first_disallowed_load_receiver;
  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("http://example.com"), ActivationLevel::ENABLED, false);

  TestActivationStateCallbackReceiver activation_state;
  auto filter = base::MakeUnique<AsyncDocumentSubresourceFilter>(
      ruleset_handle.get(), std::move(params), activation_state.callback(),
      first_disallowed_load_receiver.closure());

  LoadPolicyCallbackReceiver load_policy_1;
  filter->GetLoadPolicyForSubdocument(GURL("http://example.com/allowed.html"),
                                      load_policy_1.callback());
  RunUntilIdle();
  load_policy_1.ExpectReceivedOnce(LoadPolicy::ALLOW);
  EXPECT_EQ(0, first_disallowed_load_receiver.callback_count());

  LoadPolicyCallbackReceiver load_policy_2;
  filter->GetLoadPolicyForSubdocument(
      GURL("http://example.com/disallowed.html"), load_policy_2.callback());
  RunUntilIdle();
  load_policy_2.ExpectReceivedOnce(LoadPolicy::DISALLOW);
  EXPECT_EQ(0, first_disallowed_load_receiver.callback_count());

  filter->ReportDisallowedLoad();
  EXPECT_EQ(1, first_disallowed_load_receiver.callback_count());
  RunUntilIdle();
}

}  // namespace subresource_filter
