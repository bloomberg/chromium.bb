// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client_request.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/activation_level.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/cancelling_navigation_throttle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

char kURL[] = "http://example.test/";
char kRedirectURL[] = "http://redirect.test/";

// Names of navigation chain patterns histogram.
const char kMatchesPatternHistogramNameSubresourceFilterSuffix[] =
    "SubresourceFilter.PageLoad.RedirectChainMatchPattern."
    "SubresourceFilterOnly";
const char kNavigationChainSizeSubresourceFilterSuffix[] =
    "SubresourceFilter.PageLoad.RedirectChainLength.SubresourceFilterOnly";
const char kSafeBrowsingNavigationDelay[] =
    "SubresourceFilter.PageLoad.SafeBrowsingDelay";
const char kSafeBrowsingNavigationDelayNoSpeculation[] =
    "SubresourceFilter.PageLoad.SafeBrowsingDelay.NoRedirectSpeculation";
const char kSafeBrowsingCheckTime[] =
    "SubresourceFilter.SafeBrowsing.CheckTime";

// Human readable representation of expected redirect chain match patterns.
// The explanations for the buckets given for the following redirect chain:
// A->B->C->D, where A is initial URL and D is a final URL.
enum RedirectChainMatchPattern {
  EMPTY,             // No histograms were recorded.
  F0M0L1,            // D is a Safe Browsing match.
  F0M1L0,            // B or C, or both are Safe Browsing matches.
  F0M1L1,            // B or C, or both and D are Safe Browsing matches.
  F1M0L0,            // A is Safe Browsing match
  F1M0L1,            // A and D are Safe Browsing matches.
  F1M1L0,            // B and/or C and A are Safe Browsing matches.
  F1M1L1,            // B and/or C and A and D are Safe Browsing matches.
  NO_REDIRECTS_HIT,  // Redirect chain consists of single URL, aka no redirects
                     // has happened, and this URL was a Safe Browsing hit.
  NUM_HIT_PATTERNS,
};

class MockSubresourceFilterClient
    : public subresource_filter::SubresourceFilterClient {
 public:
  MockSubresourceFilterClient() {}

  ~MockSubresourceFilterClient() override = default;

  MOCK_METHOD1(ToggleNotificationVisibility, void(bool));
  MOCK_METHOD2(OnPageActivationComputed,
               bool(content::NavigationHandle*, bool));
  MOCK_METHOD1(WhitelistByContentSettings, void(const GURL&));
  MOCK_METHOD1(WhitelistInCurrentWebContents, void(const GURL&));
  MOCK_METHOD0(GetRulesetDealer, VerifiedRulesetDealer::Handle*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSubresourceFilterClient);
};

// Throttle to call WillProcessResponse on the factory, which is otherwise
// called by the ThrottleManager.
class TestForwardingNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit TestForwardingNavigationThrottle(content::NavigationHandle* handle)
      : content::NavigationThrottle(handle) {}
  ~TestForwardingNavigationThrottle() override {}

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override {
    content::WebContents* web_contents = navigation_handle()->GetWebContents();
    ContentSubresourceFilterDriverFactory* factory =
        ContentSubresourceFilterDriverFactory::FromWebContents(web_contents);
    factory->WillProcessResponse(navigation_handle());
    return content::NavigationThrottle::PROCEED;
  }
  const char* GetNameForLogging() override {
    return "TestForwardingNavigationThrottle";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestForwardingNavigationThrottle);
};

}  //  namespace

class SubresourceFilterSafeBrowsingActivationThrottleTest
    : public content::RenderViewHostTestHarness,
      public content::WebContentsObserver {
 public:
  SubresourceFilterSafeBrowsingActivationThrottleTest()
      : field_trial_list_(nullptr) {}
  ~SubresourceFilterSafeBrowsingActivationThrottleTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    scoped_configuration_.ResetConfiguration(Configuration(
        ActivationLevel::ENABLED, ActivationScope::ACTIVATION_LIST,
        ActivationList::SUBRESOURCE_FILTER));
    test_io_task_runner_ = new base::TestMockTimeTaskRunner();
    // Note: Using NiceMock to allow uninteresting calls and suppress warnings.
    client_ =
        base::MakeUnique<::testing::NiceMock<MockSubresourceFilterClient>>();
    ContentSubresourceFilterDriverFactory::CreateForWebContents(
        RenderViewHostTestHarness::web_contents(), client_.get());
    fake_safe_browsing_database_ = new FakeSafeBrowsingDatabaseManager();
    NavigateAndCommit(GURL("https://test.com"));
    Observe(RenderViewHostTestHarness::web_contents());
  }

  void TearDown() override {
    RunUntilIdle();
    content::RenderViewHostTestHarness::TearDown();
  }

  ContentSubresourceFilterDriverFactory* factory() {
    return ContentSubresourceFilterDriverFactory::FromWebContents(
        RenderViewHostTestHarness::web_contents());
  }

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    ASSERT_TRUE(navigation_handle->IsInMainFrame());
    navigation_handle->RegisterThrottleForTesting(
        base::MakeUnique<SubresourceFilterSafeBrowsingActivationThrottle>(
            navigation_handle, test_io_task_runner_,
            fake_safe_browsing_database_));
    navigation_handle->RegisterThrottleForTesting(
        base::MakeUnique<TestForwardingNavigationThrottle>(navigation_handle));
  }

  content::NavigationThrottle::ThrottleCheckResult SimulateStart() {
    navigation_simulator_->Start();
    auto result = navigation_simulator_->GetLastThrottleCheckResult();
    if (result == content::NavigationThrottle::CANCEL)
      navigation_simulator_.reset();
    return result;
  }

  content::NavigationThrottle::ThrottleCheckResult SimulateRedirect(
      const GURL& new_url) {
    navigation_simulator_->Redirect(new_url);
    auto result = navigation_simulator_->GetLastThrottleCheckResult();
    if (result == content::NavigationThrottle::CANCEL)
      navigation_simulator_.reset();
    return result;
  }

  content::NavigationThrottle::ThrottleCheckResult SimulateCommit() {
    // Need to post a task to flush the IO thread because calling Commit()
    // blocks until the throttle checks are complete.
    // TODO(csharrison): Consider adding finer grained control to the
    // NavigationSimulator by giving it an option to be driven by a
    // TestMockTimeTaskRunner. Also see https://crbug.com/703346.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&base::TestMockTimeTaskRunner::RunUntilIdle,
                              base::Unretained(test_io_task_runner_.get())));
    navigation_simulator_->Commit();
    auto result = navigation_simulator_->GetLastThrottleCheckResult();
    if (result == content::NavigationThrottle::CANCEL)
      navigation_simulator_.reset();
    return result;
  }

  void SimulateStartAndExpectProceed() {
    EXPECT_EQ(content::NavigationThrottle::PROCEED, SimulateStart());
  }

  void SimulateRedirectAndExpectProceed(const GURL& new_url) {
    EXPECT_EQ(content::NavigationThrottle::PROCEED, SimulateRedirect(new_url));
  }

  void SimulateCommitAndExpectProceed() {
    EXPECT_EQ(content::NavigationThrottle::PROCEED, SimulateCommit());
  }

  void CreateTestNavigationForMainFrame(const GURL& first_url) {
    navigation_simulator_ =
        content::NavigationSimulator::CreateRendererInitiated(first_url,
                                                              main_rfh());
  }

  void ConfigureAsSubresourceFilterOnlyURL(const GURL& url) {
    fake_safe_browsing_database_->AddBlacklistedUrl(
        url, safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER);
  }

  void SimulateTimeout() { fake_safe_browsing_database_->SimulateTimeout(); }

  void RunUntilIdle() {
    test_io_task_runner_->RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  const base::HistogramTester& tester() const { return tester_; }

  base::TestMockTimeTaskRunner* test_io_task_runner() const {
    return test_io_task_runner_.get();
  }

 private:
  base::FieldTrialList field_trial_list_;
  testing::ScopedSubresourceFilterConfigurator scoped_configuration_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_io_task_runner_;
  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;
  std::unique_ptr<SubresourceFilterClient> client_;
  scoped_refptr<FakeSafeBrowsingDatabaseManager> fake_safe_browsing_database_;
  base::HistogramTester tester_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterSafeBrowsingActivationThrottleTest);
};

class SubresourceFilterSafeBrowsingActivationThrottleTestWithCancelling
    : public SubresourceFilterSafeBrowsingActivationThrottleTest,
      public ::testing::WithParamInterface<
          std::tuple<content::CancellingNavigationThrottle::CancelTime,
                     content::CancellingNavigationThrottle::ResultSynchrony>> {
 public:
  SubresourceFilterSafeBrowsingActivationThrottleTestWithCancelling() {
    std::tie(cancel_time_, result_sync_) = GetParam();
  }
  ~SubresourceFilterSafeBrowsingActivationThrottleTestWithCancelling()
      override {}

  void DidStartNavigation(content::NavigationHandle* handle) override {
    handle->RegisterThrottleForTesting(
        base::MakeUnique<content::CancellingNavigationThrottle>(
            handle, cancel_time_, result_sync_));
    SubresourceFilterSafeBrowsingActivationThrottleTest::DidStartNavigation(
        handle);
  }

  content::CancellingNavigationThrottle::CancelTime cancel_time() {
    return cancel_time_;
  }

  content::CancellingNavigationThrottle::ResultSynchrony result_sync() {
    return result_sync_;
  }

 private:
  content::CancellingNavigationThrottle::CancelTime cancel_time_;
  content::CancellingNavigationThrottle::ResultSynchrony result_sync_;

  DISALLOW_COPY_AND_ASSIGN(
      SubresourceFilterSafeBrowsingActivationThrottleTestWithCancelling);
};

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ListNotMatched_NoActivation) {
  const GURL url(kURL);
  CreateTestNavigationForMainFrame(url);
  SimulateStartAndExpectProceed();
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(ContentSubresourceFilterDriverFactory::ActivationDecision::
                ACTIVATION_CONDITIONS_NOT_MET,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  tester().ExpectTotalCount(kMatchesPatternHistogramNameSubresourceFilterSuffix,
                            0);
  tester().ExpectTotalCount(kNavigationChainSizeSubresourceFilterSuffix, 0);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelay, 1);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelayNoSpeculation, 1);
  tester().ExpectTotalCount(kSafeBrowsingCheckTime, 1);
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ListMatched_Activation) {
  const GURL url(kURL);
  ConfigureAsSubresourceFilterOnlyURL(url);
  CreateTestNavigationForMainFrame(url);
  SimulateStartAndExpectProceed();
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(
      ContentSubresourceFilterDriverFactory::ActivationDecision::ACTIVATED,
      factory()->GetActivationDecisionForLastCommittedPageLoad());
  tester().ExpectUniqueSample(
      kMatchesPatternHistogramNameSubresourceFilterSuffix, NO_REDIRECTS_HIT, 1);
  tester().ExpectUniqueSample(kNavigationChainSizeSubresourceFilterSuffix, 1,
                              1);
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ListNotMatchedAfterRedirect_NoActivation) {
  const GURL url(kURL);
  CreateTestNavigationForMainFrame(url);
  SimulateStartAndExpectProceed();
  SimulateRedirectAndExpectProceed(GURL(kRedirectURL));
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(ContentSubresourceFilterDriverFactory::ActivationDecision::
                ACTIVATION_CONDITIONS_NOT_MET,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  tester().ExpectTotalCount(kMatchesPatternHistogramNameSubresourceFilterSuffix,
                            0);
  tester().ExpectTotalCount(kNavigationChainSizeSubresourceFilterSuffix, 0);
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ListMatchedAfterRedirect_Activation) {
  const GURL url(kURL);
  ConfigureAsSubresourceFilterOnlyURL(GURL(kRedirectURL));
  CreateTestNavigationForMainFrame(url);
  SimulateStartAndExpectProceed();
  SimulateRedirectAndExpectProceed(GURL(kRedirectURL));
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(
      ContentSubresourceFilterDriverFactory::ActivationDecision::ACTIVATED,
      factory()->GetActivationDecisionForLastCommittedPageLoad());
  tester().ExpectUniqueSample(
      kMatchesPatternHistogramNameSubresourceFilterSuffix, F0M0L1, 1);
  tester().ExpectUniqueSample(kNavigationChainSizeSubresourceFilterSuffix, 2,
                              1);
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ListNotMatchedAndTimeout_NoActivation) {
  const GURL url(kURL);
  SimulateTimeout();
  CreateTestNavigationForMainFrame(url);
  SimulateStartAndExpectProceed();

  // Flush the pending tasks on the IO thread, so the delayed task surely gets
  // posted.
  test_io_task_runner()->RunUntilIdle();

  // Expect one delayed task, and fast forward time.
  base::TimeDelta expected_delay =
      SubresourceFilterSafeBrowsingClientRequest::kCheckURLTimeout;
  EXPECT_EQ(expected_delay, test_io_task_runner()->NextPendingTaskDelay());
  test_io_task_runner()->FastForwardBy(expected_delay);
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(ContentSubresourceFilterDriverFactory::ActivationDecision::
                ACTIVATION_CONDITIONS_NOT_MET,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  tester().ExpectTotalCount(kMatchesPatternHistogramNameSubresourceFilterSuffix,
                            0);
  tester().ExpectTotalCount(kNavigationChainSizeSubresourceFilterSuffix, 0);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelay, 1);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelayNoSpeculation, 1);
  tester().ExpectTotalCount(kSafeBrowsingCheckTime, 1);
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ListMatchedOnStart_NoDelay) {
  const GURL url(kURL);
  ConfigureAsSubresourceFilterOnlyURL(url);
  CreateTestNavigationForMainFrame(url);
  SimulateStartAndExpectProceed();

  // Get the database result back before commit.
  RunUntilIdle();

  SimulateCommitAndExpectProceed();
  EXPECT_EQ(
      ContentSubresourceFilterDriverFactory::ActivationDecision::ACTIVATED,
      factory()->GetActivationDecisionForLastCommittedPageLoad());
  tester().ExpectUniqueSample(
      kMatchesPatternHistogramNameSubresourceFilterSuffix, NO_REDIRECTS_HIT, 1);
  tester().ExpectUniqueSample(kNavigationChainSizeSubresourceFilterSuffix, 1,
                              1);
  tester().ExpectTimeBucketCount(kSafeBrowsingNavigationDelay,
                                 base::TimeDelta::FromMilliseconds(0), 1);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelayNoSpeculation, 1);
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ListMatchedOnRedirect_NoDelay) {
  const GURL url(kURL);
  const GURL redirect_url(kRedirectURL);
  ConfigureAsSubresourceFilterOnlyURL(redirect_url);
  CreateTestNavigationForMainFrame(url);

  SimulateStartAndExpectProceed();
  SimulateRedirectAndExpectProceed(redirect_url);

  // Get the database result back before commit.
  RunUntilIdle();

  SimulateCommitAndExpectProceed();
  EXPECT_EQ(
      ContentSubresourceFilterDriverFactory::ActivationDecision::ACTIVATED,
      factory()->GetActivationDecisionForLastCommittedPageLoad());
  tester().ExpectUniqueSample(
      kMatchesPatternHistogramNameSubresourceFilterSuffix, F0M0L1, 1);
  tester().ExpectUniqueSample(kNavigationChainSizeSubresourceFilterSuffix, 2,
                              1);
  tester().ExpectTimeBucketCount(kSafeBrowsingNavigationDelay,
                                 base::TimeDelta::FromMilliseconds(0), 1);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelayNoSpeculation, 1);
  tester().ExpectTotalCount(kSafeBrowsingCheckTime, 2);
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ListMatchedOnStartWithRedirect_NoActivation) {
  const GURL url(kURL);
  const GURL redirect_url(kRedirectURL);
  ConfigureAsSubresourceFilterOnlyURL(url);
  CreateTestNavigationForMainFrame(url);

  // These two lines also test how the database client reacts to two requests
  // happening one after another.
  SimulateStartAndExpectProceed();
  SimulateRedirectAndExpectProceed(redirect_url);

  // Get the database result back before commit.
  RunUntilIdle();

  SimulateCommitAndExpectProceed();
  EXPECT_EQ(ContentSubresourceFilterDriverFactory::ActivationDecision::
                ACTIVATION_CONDITIONS_NOT_MET,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  tester().ExpectTotalCount(kMatchesPatternHistogramNameSubresourceFilterSuffix,
                            0);
  tester().ExpectTotalCount(kNavigationChainSizeSubresourceFilterSuffix, 0);
  tester().ExpectTimeBucketCount(kSafeBrowsingNavigationDelay,
                                 base::TimeDelta::FromMilliseconds(0), 1);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelayNoSpeculation, 1);
}

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleTestWithCancelling,
       Cancel) {
  const GURL url(kURL);
  SCOPED_TRACE(::testing::Message() << "CancelTime: " << cancel_time()
                                    << " ResultSynchrony: " << result_sync());
  ConfigureAsSubresourceFilterOnlyURL(url);
  CreateTestNavigationForMainFrame(url);

  content::NavigationThrottle::ThrottleCheckResult result = SimulateStart();
  if (cancel_time() ==
      content::CancellingNavigationThrottle::WILL_START_REQUEST) {
    EXPECT_EQ(content::NavigationThrottle::CANCEL, result);
    tester().ExpectTotalCount(kSafeBrowsingNavigationDelay, 0);
    return;
  }
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result);

  result = SimulateRedirect(GURL(kRedirectURL));
  if (cancel_time() ==
      content::CancellingNavigationThrottle::WILL_REDIRECT_REQUEST) {
    EXPECT_EQ(content::NavigationThrottle::CANCEL, result);
    tester().ExpectTotalCount(kSafeBrowsingNavigationDelay, 0);
    return;
  }
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result);

  base::RunLoop().RunUntilIdle();

  result = SimulateCommit();
  EXPECT_EQ(content::NavigationThrottle::CANCEL, result);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelay, 0);
}

INSTANTIATE_TEST_CASE_P(
    CancelMethod,
    SubresourceFilterSafeBrowsingActivationThrottleTestWithCancelling,
    ::testing::Combine(
        ::testing::Values(
            content::CancellingNavigationThrottle::WILL_START_REQUEST,
            content::CancellingNavigationThrottle::WILL_REDIRECT_REQUEST,
            content::CancellingNavigationThrottle::WILL_PROCESS_RESPONSE),
        ::testing::Values(
            content::CancellingNavigationThrottle::SYNCHRONOUS,
            content::CancellingNavigationThrottle::ASYNCHRONOUS)));

}  // namespace subresource_filter
