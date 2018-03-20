// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/activation_state_computing_navigation_throttle.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/test/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter_test_utils.h"
#include "components/subresource_filter/core/common/activation_level.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace proto = url_pattern_index::proto;

class ActivationStateComputingNavigationThrottleTest
    : public content::RenderViewHostTestHarness,
      public content::WebContentsObserver {
 public:
  ActivationStateComputingNavigationThrottleTest()
      : simple_task_runner_(
            base::MakeRefCounted<base::TestSimpleTaskRunner>()) {}
  ~ActivationStateComputingNavigationThrottleTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.first"));
    InitializeRuleset();
    Observe(RenderViewHostTestHarness::web_contents());
  }

  void TearDown() override {
    ruleset_handle_.reset();
    dealer_handle_.reset();

    // The various ruleset classes post tasks to delete their blocking task
    // runners. Make sure that happens now to avoid test-only leaks.
    base::RunLoop().RunUntilIdle();
    simple_task_runner()->RunUntilIdle();
    content::RenderViewHostTestHarness::TearDown();
  }

  void InitializeRuleset() {
    std::vector<proto::UrlRule> rules;
    rules.push_back(testing::CreateWhitelistRuleForDocument(
        "whitelisted.com", proto::ACTIVATION_TYPE_DOCUMENT,
        {"allow-child-to-be-whitelisted.com",
         "whitelisted-generic-with-disabled-child.com"}));

    rules.push_back(testing::CreateWhitelistRuleForDocument(
        "whitelisted-generic.com", proto::ACTIVATION_TYPE_GENERICBLOCK,
        {"allow-child-to-be-whitelisted.com"}));

    rules.push_back(testing::CreateWhitelistRuleForDocument(
        "whitelisted-generic-with-disabled-child.com",
        proto::ACTIVATION_TYPE_GENERICBLOCK,
        {"allow-child-to-be-whitelisted.com"}));

    rules.push_back(testing::CreateWhitelistRuleForDocument(
        "whitelisted-always.com", proto::ACTIVATION_TYPE_DOCUMENT));

    ASSERT_NO_FATAL_FAILURE(test_ruleset_creator_.CreateRulesetWithRules(
        rules, &test_ruleset_pair_));

    // Make the blocking task runner run on the current task runner for the
    // tests, to ensure that the NavigationSimulator properly runs all necessary
    // tasks while waiting for throttle checks to finish.
    InitializeRulesetHandles(base::SequencedTaskRunnerHandle::Get());
  }

  void NavigateAndCommitMainFrameWithPageActivationState(
      const GURL& document_url,
      const ActivationState& page_activation) {
    CreateTestNavigationForMainFrame(document_url);
    SimulateStartAndExpectToProceed();

    NotifyPageActivation(page_activation);
    SimulateCommitAndExpectToProceed();
  }

  void CreateTestNavigationForMainFrame(const GURL& first_url) {
    navigation_simulator_ =
        content::NavigationSimulator::CreateRendererInitiated(first_url,
                                                              main_rfh());
  }

  void CreateSubframeAndInitTestNavigation(
      const GURL& first_url,
      content::RenderFrameHost* parent,
      const ActivationState& parent_activation_state) {
    ASSERT_TRUE(parent);
    parent_activation_state_ = parent_activation_state;
    content::RenderFrameHost* navigating_subframe =
        content::RenderFrameHostTester::For(parent)->AppendChild("subframe");
    navigation_simulator_ =
        content::NavigationSimulator::CreateRendererInitiated(
            first_url, navigating_subframe);
  }

  void SimulateStartAndExpectToProceed() {
    ASSERT_TRUE(navigation_simulator_);
    navigation_simulator_->Start();
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              navigation_simulator_->GetLastThrottleCheckResult());
  }

  void SimulateRedirectAndExpectToProceed(const GURL& new_url) {
    navigation_simulator_->Redirect(new_url);
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              navigation_simulator_->GetLastThrottleCheckResult());
  }

  void SimulateCommitAndExpectToProceed() {
    navigation_simulator_->Commit();
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              navigation_simulator_->GetLastThrottleCheckResult());
  }

  void InitializeRulesetHandles(
      scoped_refptr<base::SequencedTaskRunner> ruleset_task_runner) {
    dealer_handle_ = std::make_unique<VerifiedRulesetDealer::Handle>(
        std::move(ruleset_task_runner));
    dealer_handle_->TryOpenAndSetRulesetFile(test_ruleset_pair_.indexed.path,
                                             base::DoNothing());
    ruleset_handle_ =
        std::make_unique<VerifiedRuleset::Handle>(dealer_handle_.get());
  }

  void NotifyPageActivation(ActivationState state) {
    test_throttle_->NotifyPageActivationWithRuleset(ruleset_handle_.get(),
                                                    state);
  }

  ActivationState last_activation_state() {
    EXPECT_TRUE(last_activation_state_.has_value());
    return last_activation_state_.value_or(
        ActivationState(ActivationLevel::DISABLED));
  }

  content::RenderFrameHost* last_committed_frame_host() {
    return last_committed_frame_host_;
  }

  scoped_refptr<base::TestSimpleTaskRunner> simple_task_runner() {
    return simple_task_runner_;
  }

 protected:
  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    std::unique_ptr<ActivationStateComputingNavigationThrottle> throttle =
        navigation_handle->IsInMainFrame()
            ? ActivationStateComputingNavigationThrottle::CreateForMainFrame(
                  navigation_handle)
            : ActivationStateComputingNavigationThrottle::CreateForSubframe(
                  navigation_handle, ruleset_handle_.get(),
                  parent_activation_state_.value());
    test_throttle_ = throttle.get();
    navigation_handle->RegisterThrottleForTesting(std::move(throttle));
  }

  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!test_throttle_)
      return;
    ASSERT_EQ(navigation_handle, test_throttle_->navigation_handle());
    if (test_throttle_->filter())
      test_throttle_->WillSendActivationToRenderer();

    if (auto filter = test_throttle_->ReleaseFilter()) {
      EXPECT_NE(ActivationLevel::DISABLED,
                filter->activation_state().activation_level);
      last_activation_state_ = filter->activation_state();
    } else {
      last_activation_state_ = ActivationState(ActivationLevel::DISABLED);
    }
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!test_throttle_)
      return;
    last_committed_frame_host_ = navigation_handle->GetRenderFrameHost();
    test_throttle_ = nullptr;
  }

 private:
  testing::TestRulesetCreator test_ruleset_creator_;
  testing::TestRulesetPair test_ruleset_pair_;

  std::unique_ptr<VerifiedRulesetDealer::Handle> dealer_handle_;
  std::unique_ptr<VerifiedRuleset::Handle> ruleset_handle_;

  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;

  scoped_refptr<base::TestSimpleTaskRunner> simple_task_runner_;

  // Owned by the current navigation.
  ActivationStateComputingNavigationThrottle* test_throttle_;
  base::Optional<ActivationState> last_activation_state_;
  base::Optional<ActivationState> parent_activation_state_;

  // Needed for potential cross process navigations which swap hosts.
  content::RenderFrameHost* last_committed_frame_host_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ActivationStateComputingNavigationThrottleTest);
};

typedef ActivationStateComputingNavigationThrottleTest
    ActivationStateComputingThrottleMainFrameTest;
typedef ActivationStateComputingNavigationThrottleTest
    ActivationStateComputingThrottleSubFrameTest;

TEST_F(ActivationStateComputingThrottleMainFrameTest, Activate) {
  NavigateAndCommitMainFrameWithPageActivationState(
      GURL("http://example.test/"), ActivationState(ActivationLevel::ENABLED));
  ActivationState state = last_activation_state();
  EXPECT_EQ(ActivationLevel::ENABLED, state.activation_level);
  EXPECT_FALSE(state.filtering_disabled_for_document);
}

TEST_F(ActivationStateComputingThrottleMainFrameTest,
       NoPageActivationNotification_NoActivation) {
  CreateTestNavigationForMainFrame(GURL("http://example.test/"));
  SimulateStartAndExpectToProceed();
  SimulateRedirectAndExpectToProceed(GURL("http://example.test/?v=1"));

  // Never send NotifyPageActivation.
  SimulateCommitAndExpectToProceed();

  ActivationState state = last_activation_state();
  EXPECT_EQ(ActivationLevel::DISABLED, state.activation_level);
}

TEST_F(ActivationStateComputingThrottleMainFrameTest,
       WhitelistDoesNotApply_CausesActivation) {
  NavigateAndCommitMainFrameWithPageActivationState(
      GURL("http://allow-child-to-be-whitelisted.com/"),
      ActivationState(ActivationLevel::ENABLED));

  NavigateAndCommitMainFrameWithPageActivationState(
      GURL("http://whitelisted.com/"),
      ActivationState(ActivationLevel::ENABLED));

  ActivationState state = last_activation_state();
  EXPECT_FALSE(state.filtering_disabled_for_document);
  EXPECT_FALSE(state.generic_blocking_rules_disabled);
  EXPECT_EQ(ActivationLevel::ENABLED, state.activation_level);
}

TEST_F(ActivationStateComputingThrottleMainFrameTest,
       Whitelisted_DisablesFiltering) {
  NavigateAndCommitMainFrameWithPageActivationState(
      GURL("http://whitelisted-always.com/"),
      ActivationState(ActivationLevel::ENABLED));

  ActivationState state = last_activation_state();
  EXPECT_TRUE(state.filtering_disabled_for_document);
  EXPECT_FALSE(state.generic_blocking_rules_disabled);
  EXPECT_EQ(ActivationLevel::ENABLED, state.activation_level);
}

TEST_F(ActivationStateComputingThrottleSubFrameTest, Activate) {
  NavigateAndCommitMainFrameWithPageActivationState(
      GURL("http://example.test/"), ActivationState(ActivationLevel::ENABLED));

  CreateSubframeAndInitTestNavigation(GURL("http://example.child/"),
                                      last_committed_frame_host(),
                                      last_activation_state());
  SimulateStartAndExpectToProceed();
  SimulateRedirectAndExpectToProceed(GURL("http://example.child/?v=1"));
  SimulateCommitAndExpectToProceed();

  ActivationState state = last_activation_state();
  EXPECT_EQ(ActivationLevel::ENABLED, state.activation_level);
  EXPECT_FALSE(state.filtering_disabled_for_document);
  EXPECT_FALSE(state.generic_blocking_rules_disabled);
}

TEST_F(ActivationStateComputingThrottleSubFrameTest,
       WhitelistDoesNotApply_CausesActivation) {
  NavigateAndCommitMainFrameWithPageActivationState(
      GURL("http://disallows-child-to-be-whitelisted.com/"),
      ActivationState(ActivationLevel::ENABLED));

  CreateSubframeAndInitTestNavigation(GURL("http://whitelisted.com/"),
                                      last_committed_frame_host(),
                                      last_activation_state());
  SimulateStartAndExpectToProceed();
  SimulateCommitAndExpectToProceed();

  ActivationState state = last_activation_state();
  EXPECT_EQ(ActivationLevel::ENABLED, state.activation_level);
  EXPECT_FALSE(state.filtering_disabled_for_document);
  EXPECT_FALSE(state.generic_blocking_rules_disabled);
}

TEST_F(ActivationStateComputingThrottleSubFrameTest,
       Whitelisted_DisableDocumentFiltering) {
  NavigateAndCommitMainFrameWithPageActivationState(
      GURL("http://allow-child-to-be-whitelisted.com/"),
      ActivationState(ActivationLevel::ENABLED));

  CreateSubframeAndInitTestNavigation(GURL("http://whitelisted.com/"),
                                      last_committed_frame_host(),
                                      last_activation_state());
  SimulateStartAndExpectToProceed();
  SimulateCommitAndExpectToProceed();

  ActivationState state = last_activation_state();
  EXPECT_TRUE(state.filtering_disabled_for_document);
  EXPECT_FALSE(state.generic_blocking_rules_disabled);
  EXPECT_EQ(ActivationLevel::ENABLED, state.activation_level);
}

TEST_F(ActivationStateComputingThrottleSubFrameTest,
       Whitelisted_DisablesGenericRules) {
  NavigateAndCommitMainFrameWithPageActivationState(
      GURL("http://allow-child-to-be-whitelisted.com/"),
      ActivationState(ActivationLevel::ENABLED));

  CreateSubframeAndInitTestNavigation(GURL("http://whitelisted-generic.com/"),
                                      last_committed_frame_host(),
                                      last_activation_state());
  SimulateStartAndExpectToProceed();
  SimulateCommitAndExpectToProceed();

  ActivationState state = last_activation_state();
  EXPECT_FALSE(state.filtering_disabled_for_document);
  EXPECT_TRUE(state.generic_blocking_rules_disabled);
  EXPECT_EQ(ActivationLevel::ENABLED, state.activation_level);
}

TEST_F(ActivationStateComputingThrottleSubFrameTest, DryRunIsPropagated) {
  NavigateAndCommitMainFrameWithPageActivationState(
      GURL("http://example.test/"), ActivationState(ActivationLevel::DRYRUN));
  EXPECT_EQ(ActivationLevel::DRYRUN, last_activation_state().activation_level);

  CreateSubframeAndInitTestNavigation(GURL("http://example.child/"),
                                      last_committed_frame_host(),
                                      last_activation_state());
  SimulateStartAndExpectToProceed();
  SimulateRedirectAndExpectToProceed(GURL("http://example.child/?v=1"));
  SimulateCommitAndExpectToProceed();

  ActivationState state = last_activation_state();
  EXPECT_EQ(ActivationLevel::DRYRUN, state.activation_level);
  EXPECT_FALSE(state.filtering_disabled_for_document);
  EXPECT_FALSE(state.generic_blocking_rules_disabled);
}

TEST_F(ActivationStateComputingThrottleSubFrameTest,
       DryRunWithLoggingIsPropagated) {
  ActivationState page_state(ActivationLevel::DRYRUN);
  page_state.enable_logging = true;
  NavigateAndCommitMainFrameWithPageActivationState(
      GURL("http://example.test/"), page_state);
  EXPECT_EQ(ActivationLevel::DRYRUN, last_activation_state().activation_level);

  CreateSubframeAndInitTestNavigation(GURL("http://example.child/"),
                                      last_committed_frame_host(),
                                      last_activation_state());
  SimulateStartAndExpectToProceed();
  SimulateRedirectAndExpectToProceed(GURL("http://example.child/?v=1"));
  SimulateCommitAndExpectToProceed();

  ActivationState state = last_activation_state();
  EXPECT_EQ(ActivationLevel::DRYRUN, state.activation_level);
  EXPECT_TRUE(state.enable_logging);
  EXPECT_FALSE(state.filtering_disabled_for_document);
  EXPECT_FALSE(state.generic_blocking_rules_disabled);
}

TEST_F(ActivationStateComputingThrottleSubFrameTest, DisabledStatePropagated) {
  NavigateAndCommitMainFrameWithPageActivationState(
      GURL("http://allow-child-to-be-whitelisted.com/"),
      ActivationState(ActivationLevel::ENABLED));

  CreateSubframeAndInitTestNavigation(GURL("http://whitelisted.com"),
                                      last_committed_frame_host(),
                                      last_activation_state());
  SimulateStartAndExpectToProceed();
  SimulateCommitAndExpectToProceed();

  CreateSubframeAndInitTestNavigation(GURL("http://example.test/"),
                                      last_committed_frame_host(),
                                      last_activation_state());
  SimulateStartAndExpectToProceed();
  SimulateCommitAndExpectToProceed();

  ActivationState state = last_activation_state();
  EXPECT_EQ(ActivationLevel::ENABLED, state.activation_level);
  EXPECT_TRUE(state.filtering_disabled_for_document);
  EXPECT_FALSE(state.generic_blocking_rules_disabled);
}

TEST_F(ActivationStateComputingThrottleSubFrameTest, DisabledStatePropagated2) {
  NavigateAndCommitMainFrameWithPageActivationState(
      GURL("http://allow-child-to-be-whitelisted.com/"),
      ActivationState(ActivationLevel::ENABLED));

  CreateSubframeAndInitTestNavigation(
      GURL("http://whitelisted-generic-with-disabled-child.com/"),
      last_committed_frame_host(), last_activation_state());
  SimulateStartAndExpectToProceed();
  SimulateCommitAndExpectToProceed();

  ActivationState state = last_activation_state();
  EXPECT_FALSE(state.filtering_disabled_for_document);
  EXPECT_TRUE(state.generic_blocking_rules_disabled);

  CreateSubframeAndInitTestNavigation(GURL("http://whitelisted.com/"),
                                      last_committed_frame_host(),
                                      last_activation_state());
  SimulateStartAndExpectToProceed();
  SimulateCommitAndExpectToProceed();

  state = last_activation_state();
  EXPECT_EQ(ActivationLevel::ENABLED, state.activation_level);
  EXPECT_TRUE(state.filtering_disabled_for_document);
  EXPECT_TRUE(state.generic_blocking_rules_disabled);
}

TEST_F(ActivationStateComputingThrottleSubFrameTest, DelayMetrics) {
  base::HistogramTester histogram_tester;
  NavigateAndCommitMainFrameWithPageActivationState(
      GURL("http://example.test/"), ActivationState(ActivationLevel::ENABLED));
  ActivationState state = last_activation_state();
  EXPECT_EQ(ActivationLevel::ENABLED, state.activation_level);
  EXPECT_FALSE(state.filtering_disabled_for_document);

  const char kActivationDelay[] =
      "SubresourceFilter.DocumentLoad.ActivationComputingDelay";
  const char kActivationDelayMainFrame[] =
      "SubresourceFilter.DocumentLoad.ActivationComputingDelay.MainFrame";
  histogram_tester.ExpectTotalCount(kActivationDelay, 1);
  histogram_tester.ExpectTotalCount(kActivationDelayMainFrame, 1);

  // Subframe activation should not log main frame metrics.
  CreateSubframeAndInitTestNavigation(GURL("http://example.test/"),
                                      last_committed_frame_host(),
                                      last_activation_state());
  SimulateStartAndExpectToProceed();
  SimulateCommitAndExpectToProceed();
  histogram_tester.ExpectTotalCount(kActivationDelay, 2);
  histogram_tester.ExpectTotalCount(kActivationDelayMainFrame, 1);

  // No page activation should imply no delay.
  CreateTestNavigationForMainFrame(GURL("http://example.test2/"));
  SimulateStartAndExpectToProceed();
  SimulateCommitAndExpectToProceed();

  state = last_activation_state();
  EXPECT_EQ(ActivationLevel::DISABLED, state.activation_level);
  histogram_tester.ExpectTotalCount(kActivationDelay, 2);
  histogram_tester.ExpectTotalCount(kActivationDelayMainFrame, 1);
}

TEST_F(ActivationStateComputingThrottleSubFrameTest, SpeculativeSubframes) {
  // Use the activation performance metric as a proxy for how many times
  // activation computation occurred.
  base::HistogramTester histogram_tester;
  const char kActivationCPU[] =
      "SubresourceFilter.DocumentLoad.Activation.CPUDuration";

  // Main frames don't do speculative lookups, a navigation commit should only
  // trigger a single ruleset lookup.
  CreateTestNavigationForMainFrame(GURL("http://example.test/"));
  SimulateStartAndExpectToProceed();
  histogram_tester.ExpectTotalCount(kActivationCPU, 0);

  SimulateRedirectAndExpectToProceed(GURL("http://example.test2/"));
  histogram_tester.ExpectTotalCount(kActivationCPU, 0);

  NotifyPageActivation(ActivationState(ActivationLevel::ENABLED));
  SimulateCommitAndExpectToProceed();
  histogram_tester.ExpectTotalCount(kActivationCPU, 1);

  CreateSubframeAndInitTestNavigation(GURL("http://example.test/"),
                                      last_committed_frame_host(),
                                      last_activation_state());
  // For subframes, do a ruleset lookup at the start and every redirect.
  SimulateStartAndExpectToProceed();
  histogram_tester.ExpectTotalCount(kActivationCPU, 2);

  SimulateRedirectAndExpectToProceed(GURL("http://example.test2/"));
  histogram_tester.ExpectTotalCount(kActivationCPU, 3);

  // No ruleset lookup required at commit because we've already checked the
  // latest URL.
  SimulateCommitAndExpectToProceed();
  histogram_tester.ExpectTotalCount(kActivationCPU, 3);
}

TEST_F(ActivationStateComputingThrottleSubFrameTest,
       SpeculativeSubframesWithDelay) {
  InitializeRulesetHandles(simple_task_runner());

  // TODO(csharrison): Once crbug.com/822275 is resolved, implement this via a
  // Wait() style interface on NavigationSimulator. Right now it is very hacky:
  // essentially we post a task to finish tasks on |simple_task_runner_| before
  // a navigation stage (when necessary). NavigationSimulator internals pump the
  // run loop which runs this task after throttles run.
  auto task_runner = simple_task_runner();
  auto post_advance = [task_runner]() {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&base::TestSimpleTaskRunner::RunPendingTasks,
                                  base::RetainedRef(task_runner)));
  };

  // Use the activation performance metric as a proxy for how many times
  // activation computation occurred.
  base::HistogramTester histogram_tester;
  const char kActivationCPU[] =
      "SubresourceFilter.DocumentLoad.Activation.CPUDuration";

  // Main frames don't do speculative lookups, a navigation commit should only
  // trigger a single ruleset lookup.
  CreateTestNavigationForMainFrame(GURL("http://example.test/"));
  SimulateStartAndExpectToProceed();
  histogram_tester.ExpectTotalCount(kActivationCPU, 0);

  SimulateRedirectAndExpectToProceed(GURL("http://example.test2/"));
  histogram_tester.ExpectTotalCount(kActivationCPU, 0);

  NotifyPageActivation(ActivationState(ActivationLevel::ENABLED));
  post_advance();
  SimulateCommitAndExpectToProceed();
  histogram_tester.ExpectTotalCount(kActivationCPU, 1);

  CreateSubframeAndInitTestNavigation(GURL("http://example.test/"),
                                      last_committed_frame_host(),
                                      last_activation_state());

  // Simulate slow ruleset checks for the subframe, these should not delay the
  // navigation until commit time.
  SimulateStartAndExpectToProceed();
  histogram_tester.ExpectTotalCount(kActivationCPU, 1);

  // Calling redirect should ensure that the throttle does not receive the
  // results of the check, but the task to actually perform the check will still
  // happen.
  SimulateRedirectAndExpectToProceed(GURL("http://example.test2/"));
  histogram_tester.ExpectTotalCount(kActivationCPU, 1);

  // Finish the checks dispatched in the start and redirect phase when the
  // navigation is ready to commit.
  post_advance();
  SimulateCommitAndExpectToProceed();
  histogram_tester.ExpectTotalCount(kActivationCPU, 3);
}

}  // namespace subresource_filter
