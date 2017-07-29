// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client_request.h"
#include "components/subresource_filter/content/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_level.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/url_pattern_index/proto/rules.pb.h"
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

const char kUrlA[] = "https://example_a.com";
const char kUrlB[] = "https://example_b.com";
const char kUrlC[] = "https://example_c.com";
const char kUrlD[] = "https://example_d.com";

char kURL[] = "http://example.test/";
char kURLWithParams[] = "http://example.test/?v=10";
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
const char kMatchesPatternHistogramName[] =
    "SubresourceFilter.PageLoad.FinalURLMatch.";
const char kNavigationChainSize[] =
    "SubresourceFilter.PageLoad.RedirectChainLength.";

class MockSubresourceFilterClient : public SubresourceFilterClient {
 public:
  MockSubresourceFilterClient() = default;
  ~MockSubresourceFilterClient() override = default;

  // Mocks have trouble with move-only types passed in the constructor.
  void set_ruleset_dealer(
      std::unique_ptr<VerifiedRulesetDealer::Handle> ruleset_dealer) {
    ruleset_dealer_ = std::move(ruleset_dealer);
  }

  bool OnPageActivationComputed(content::NavigationHandle* handle,
                                bool activated) override {
    DCHECK(handle->IsInMainFrame());
    return whitelisted_hosts_.count(handle->GetURL().host());
  }

  void WhitelistInCurrentWebContents(const GURL& url) override {
    ASSERT_TRUE(url.SchemeIsHTTPOrHTTPS());
    whitelisted_hosts_.insert(url.host());
  }

  VerifiedRulesetDealer::Handle* GetRulesetDealer() override {
    return ruleset_dealer_.get();
  }

  MOCK_METHOD1(ToggleNotificationVisibility, void(bool));
  MOCK_METHOD0(ForceActivationInCurrentWebContents, bool());

  void ClearWhitelist() { whitelisted_hosts_.clear(); }

 private:
  std::set<std::string> whitelisted_hosts_;

  std::unique_ptr<VerifiedRulesetDealer::Handle> ruleset_dealer_;

  DISALLOW_COPY_AND_ASSIGN(MockSubresourceFilterClient);
};

std::string GetSuffixForList(const ActivationList& type) {
  switch (type) {
    case ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL:
      return "SocialEngineeringAdsInterstitial";
    case ActivationList::PHISHING_INTERSTITIAL:
      return "PhishingInterstitial";
    case ActivationList::SUBRESOURCE_FILTER:
      return "SubresourceFilterOnly";
    case ActivationList::NONE:
      return std::string();
  }
  return std::string();
}

struct ActivationListTestData {
  const char* const activation_list;
  ActivationList activation_list_type;
  safe_browsing::SBThreatType threat_type;
  safe_browsing::ThreatPatternType threat_type_metadata;
};

const ActivationListTestData kActivationListTestData[] = {
    {subresource_filter::kActivationListSocialEngineeringAdsInterstitial,
     ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {subresource_filter::kActivationListPhishingInterstitial,
     ActivationList::PHISHING_INTERSTITIAL,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::NONE},
    {subresource_filter::kActivationListSubresourceFilter,
     ActivationList::SUBRESOURCE_FILTER,
     safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
     safe_browsing::ThreatPatternType::NONE},
};

void ExpectSampleForSuffix(const std::string& suffix,
                           const std::string& match_suffix,
                           const base::HistogramTester& tester) {
  tester.ExpectUniqueSample(kMatchesPatternHistogramName + suffix,
                            (suffix == match_suffix), 1);
}

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
    Configure();
    test_io_task_runner_ = new base::TestMockTimeTaskRunner();
    // Note: Using NiceMock to allow uninteresting calls and suppress warnings.
    std::vector<url_pattern_index::proto::UrlRule> rules;
    rules.push_back(testing::CreateSuffixRule("disallowed.html"));
    ASSERT_NO_FATAL_FAILURE(test_ruleset_creator_.CreateRulesetWithRules(
        rules, &test_ruleset_pair_));
    auto ruleset_dealer = base::MakeUnique<VerifiedRulesetDealer::Handle>(
        base::MessageLoop::current()->task_runner());
    ruleset_dealer->SetRulesetFile(
        testing::TestRuleset::Open(test_ruleset_pair_.indexed));
    client_ =
        base::MakeUnique<::testing::NiceMock<MockSubresourceFilterClient>>();
    client_->set_ruleset_dealer(std::move(ruleset_dealer));
    ContentSubresourceFilterDriverFactory::CreateForWebContents(
        RenderViewHostTestHarness::web_contents(), client_.get());
    fake_safe_browsing_database_ = new FakeSafeBrowsingDatabaseManager();
    NavigateAndCommit(GURL("https://test.com"));
    Observe(RenderViewHostTestHarness::web_contents());
  }

  virtual void Configure() {
    scoped_configuration_.ResetConfiguration(Configuration(
        ActivationLevel::ENABLED, ActivationScope::ACTIVATION_LIST,
        ActivationList::SUBRESOURCE_FILTER));
  }

  void TearDown() override {
    client_.reset();

    // RunUntilIdle() must be called multiple times to flush any outstanding
    // cross-thread interactions.
    // TODO(csharrison): Clean up test teardown logic.
    RunUntilIdle();
    RunUntilIdle();

    // RunUntilIdle() called once more, to delete the database on the IO thread.
    fake_safe_browsing_database_ = nullptr;
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
    if (navigation_handle->IsInMainFrame()) {
      navigation_handle->RegisterThrottleForTesting(
          base::MakeUnique<SubresourceFilterSafeBrowsingActivationThrottle>(
              navigation_handle, client(), test_io_task_runner_,
              fake_safe_browsing_database_));
    }
    std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
    factory()->throttle_manager()->MaybeAppendNavigationThrottles(
        navigation_handle, &throttles);
    for (auto& it : throttles) {
      navigation_handle->RegisterThrottleForTesting(std::move(it));
    }
  }

  // Returns the frame host the navigation committed in, or nullptr if it did
  // not succeed.
  content::RenderFrameHost* CreateAndNavigateDisallowedSubframe(
      content::RenderFrameHost* parent) {
    auto* subframe =
        content::RenderFrameHostTester::For(parent)->AppendChild("subframe");
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL("https://example.test/disallowed.html"), subframe);
    simulator->Commit();
    return simulator->GetLastThrottleCheckResult() ==
                   content::NavigationThrottle::PROCEED
               ? simulator->GetFinalRenderFrameHost()
               : nullptr;
  }

  content::RenderFrameHost* SimulateNavigateAndCommit(
      std::vector<GURL> navigation_chain,
      content::RenderFrameHost* rfh) {
    SimulateStart(navigation_chain.front(), rfh);
    for (auto it = navigation_chain.begin() + 1; it != navigation_chain.end();
         ++it) {
      SimulateRedirectAndExpectProceed(*it);
    }
    SimulateCommitAndExpectProceed();
    return navigation_simulator_->GetFinalRenderFrameHost();
  }

  content::NavigationThrottle::ThrottleCheckResult SimulateStart(
      const GURL& first_url,
      content::RenderFrameHost* rfh) {
    navigation_simulator_ =
        content::NavigationSimulator::CreateRendererInitiated(first_url, rfh);
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

  content::NavigationThrottle::ThrottleCheckResult SimulateCommit(
      content::NavigationSimulator* simulator) {
    // Need to post a task to flush the IO thread because calling Commit()
    // blocks until the throttle checks are complete.
    // TODO(csharrison): Consider adding finer grained control to the
    // NavigationSimulator by giving it an option to be driven by a
    // TestMockTimeTaskRunner. Also see https://crbug.com/703346.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&base::TestMockTimeTaskRunner::RunUntilIdle,
                              base::Unretained(test_io_task_runner_.get())));
    simulator->Commit();
    return simulator->GetLastThrottleCheckResult();
  }

  void SimulateStartAndExpectProceed(const GURL& first_url) {
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              SimulateStart(first_url, main_rfh()));
  }

  void SimulateRedirectAndExpectProceed(const GURL& new_url) {
    EXPECT_EQ(content::NavigationThrottle::PROCEED, SimulateRedirect(new_url));
  }

  void SimulateCommitAndExpectProceed() {
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              SimulateCommit(navigation_simulator()));
  }

  void ConfigureForMatch(const GURL& url,
                         safe_browsing::SBThreatType pattern_type =
                             safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
                         safe_browsing::ThreatPatternType metadata =
                             safe_browsing::ThreatPatternType::NONE) {
    fake_safe_browsing_database_->AddBlacklistedUrl(url, pattern_type,
                                                    metadata);
  }

  FakeSafeBrowsingDatabaseManager* fake_safe_browsing_database() {
    return fake_safe_browsing_database_.get();
  }

  void ClearAllBlacklistedUrls() {
    fake_safe_browsing_database_->RemoveAllBlacklistedUrls();
  }

  // With a null database the throttle becomes pass-through.
  void UsePassThroughThrottle() { fake_safe_browsing_database_ = nullptr; }

  void RunUntilIdle() {
    base::RunLoop().RunUntilIdle();
    test_io_task_runner_->RunUntilIdle();
  }

  content::NavigationSimulator* navigation_simulator() {
    return navigation_simulator_.get();
  }

  const base::HistogramTester& tester() const { return tester_; }

  MockSubresourceFilterClient* client() { return client_.get(); }

  base::TestMockTimeTaskRunner* test_io_task_runner() const {
    return test_io_task_runner_.get();
  }

  testing::ScopedSubresourceFilterConfigurator* scoped_configuration() {
    return &scoped_configuration_;
  }

 private:
  base::FieldTrialList field_trial_list_;
  testing::ScopedSubresourceFilterConfigurator scoped_configuration_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_io_task_runner_;

  testing::TestRulesetCreator test_ruleset_creator_;
  testing::TestRulesetPair test_ruleset_pair_;

  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;
  std::unique_ptr<MockSubresourceFilterClient> client_;
  scoped_refptr<FakeSafeBrowsingDatabaseManager> fake_safe_browsing_database_;
  base::HistogramTester tester_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterSafeBrowsingActivationThrottleTest);
};

class SubresourceFilterSafeBrowsingActivationThrottleParamTest
    : public SubresourceFilterSafeBrowsingActivationThrottleTest,
      public ::testing::WithParamInterface<ActivationListTestData> {
 public:
  SubresourceFilterSafeBrowsingActivationThrottleParamTest() {}
  ~SubresourceFilterSafeBrowsingActivationThrottleParamTest() override {}

  void Configure() override {
    const ActivationListTestData& test_data = GetParam();
    scoped_configuration()->ResetConfiguration(Configuration(
        ActivationLevel::ENABLED, ActivationScope::ACTIVATION_LIST,
        test_data.activation_list_type));
  }

  void ConfigureForMatchParam(const GURL& url) {
    const ActivationListTestData& test_data = GetParam();
    ConfigureForMatch(url, test_data.threat_type,
                      test_data.threat_type_metadata);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      SubresourceFilterSafeBrowsingActivationThrottleParamTest);
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

struct ActivationScopeTestData {
  ActivationDecision expected_activation_decision;
  bool url_matches_activation_list;
  ActivationScope activation_scope;
};

const ActivationScopeTestData kActivationScopeTestData[] = {
    {ActivationDecision::ACTIVATED, false /* url_matches_activation_list */,
     ActivationScope::ALL_SITES},
    {ActivationDecision::ACTIVATED, true /* url_matches_activation_list */,
     ActivationScope::ALL_SITES},
    {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
     true /* url_matches_activation_list */, ActivationScope::NO_SITES},
    {ActivationDecision::ACTIVATED, true /* url_matches_activation_list */,
     ActivationScope::ACTIVATION_LIST},
    {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
     false /* url_matches_activation_list */, ActivationScope::ACTIVATION_LIST},
};

class SubresourceFilterSafeBrowsingActivationThrottleScopeTest
    : public SubresourceFilterSafeBrowsingActivationThrottleTest,
      public ::testing::WithParamInterface<ActivationScopeTestData> {
 public:
  SubresourceFilterSafeBrowsingActivationThrottleScopeTest() {}
  ~SubresourceFilterSafeBrowsingActivationThrottleScopeTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      SubresourceFilterSafeBrowsingActivationThrottleScopeTest);
};

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       PassThroughThrottle) {
  UsePassThroughThrottle();
  SimulateNavigateAndCommit({GURL(kURL), GURL(kRedirectURL)}, main_rfh());
  EXPECT_EQ(ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
            factory()->GetActivationDecisionForLastCommittedPageLoad());

  scoped_configuration()->ResetConfiguration(
      Configuration(ActivationLevel::ENABLED, ActivationScope::ALL_SITES));
  SimulateNavigateAndCommit({GURL(kURL), GURL(kRedirectURL)}, main_rfh());
  EXPECT_EQ(ActivationDecision::ACTIVATED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());

  scoped_configuration()->ResetConfiguration(
      Configuration(ActivationLevel::ENABLED, ActivationScope::NO_SITES));
  SimulateNavigateAndCommit({GURL(kURL), GURL(kRedirectURL)}, main_rfh());
  EXPECT_EQ(ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest, NoConfigs) {
  scoped_configuration()->ResetConfiguration(std::vector<Configuration>());
  SimulateNavigateAndCommit({GURL(kURL)}, main_rfh());
  EXPECT_EQ(ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       MultipleSimultaneousConfigs) {
  Configuration config1(ActivationLevel::DRYRUN, ActivationScope::NO_SITES);
  config1.activation_conditions.priority = 2;

  Configuration config2(ActivationLevel::DISABLED,
                        ActivationScope::ACTIVATION_LIST,
                        ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL);
  config2.activation_conditions.priority = 1;

  Configuration config3(ActivationLevel::ENABLED, ActivationScope::ALL_SITES);
  config3.activation_options.should_whitelist_site_on_reload = true;
  config3.activation_conditions.priority = 0;

  scoped_configuration()->ResetConfiguration({config1, config2, config3});

  // Should match |config2| and |config3|, the former with the higher priority.
  GURL match_url(kUrlA);
  GURL non_match_url(kUrlB);
  ConfigureForMatch(match_url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
                    safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);
  SimulateNavigateAndCommit({match_url}, main_rfh());
  EXPECT_EQ(ActivationDecision::ACTIVATION_DISABLED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());

  // Should match |config3|.
  SimulateNavigateAndCommit({non_match_url}, main_rfh());
  EXPECT_EQ(ActivationDecision::ACTIVATED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());

  // Should match |config3|, but a reload, so this should get whitelisted.
  auto reload_simulator = content::NavigationSimulator::CreateRendererInitiated(
      non_match_url, main_rfh());
  reload_simulator->SetTransition(ui::PAGE_TRANSITION_RELOAD);
  reload_simulator->Start();
  SimulateCommit(reload_simulator.get());
  EXPECT_EQ(ActivationDecision::URL_WHITELISTED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ActivationLevelDisabled_NoActivation) {
  scoped_configuration()->ResetConfiguration(
      Configuration(ActivationLevel::DISABLED, ActivationScope::ACTIVATION_LIST,
                    ActivationList::SUBRESOURCE_FILTER));
  GURL url(kURL);

  SimulateNavigateAndCommit({url}, main_rfh());
  EXPECT_EQ(ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
            factory()->GetActivationDecisionForLastCommittedPageLoad());

  ConfigureForMatch(url);
  SimulateNavigateAndCommit({url}, main_rfh());
  EXPECT_EQ(ActivationDecision::ACTIVATION_DISABLED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());

  // Whitelisting occurs last, so the decision should still be DISABLED.
  factory()->client()->WhitelistInCurrentWebContents(url);
  SimulateNavigateAndCommit({url}, main_rfh());
  EXPECT_EQ(ActivationDecision::ACTIVATION_DISABLED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       AllSiteEnabled_Activates) {
  scoped_configuration()->ResetConfiguration(
      Configuration(ActivationLevel::ENABLED, ActivationScope::ALL_SITES));
  GURL url(kURL);
  SimulateNavigateAndCommit({url}, main_rfh());
  EXPECT_EQ(ActivationDecision::ACTIVATED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());

  ConfigureForMatch(url);
  SimulateNavigateAndCommit({url}, main_rfh());
  EXPECT_EQ(ActivationDecision::ACTIVATED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());

  // Adding performance measurement should keep activation.
  Configuration config_with_perf(
      Configuration(ActivationLevel::ENABLED, ActivationScope::ALL_SITES));
  config_with_perf.activation_options.performance_measurement_rate = 1.0;
  scoped_configuration()->ResetConfiguration(std::move(config_with_perf));
  SimulateNavigateAndCommit({url}, main_rfh());
  EXPECT_EQ(ActivationDecision::ACTIVATED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       NavigationFails_NoActivation) {
  EXPECT_EQ(ActivationDecision::ACTIVATION_DISABLED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  content::NavigationSimulator::NavigateAndFailFromDocument(
      GURL(kURL), net::ERR_TIMED_OUT, main_rfh());
  EXPECT_EQ(ActivationDecision::ACTIVATION_DISABLED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       NotificationVisibility) {
  GURL url(kURL);
  ConfigureForMatch(url);
  EXPECT_CALL(*client(), ToggleNotificationVisibility(false)).Times(1);
  content::RenderFrameHost* rfh = SimulateNavigateAndCommit({url}, main_rfh());

  EXPECT_CALL(*client(), ToggleNotificationVisibility(true)).Times(1);
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(rfh));
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       SuppressNotificationVisibility) {
  Configuration config(ActivationLevel::ENABLED, ActivationScope::ALL_SITES);
  config.activation_options.should_suppress_notifications = true;
  scoped_configuration()->ResetConfiguration(std::move(config));

  GURL url(kURL);
  content::RenderFrameHost* rfh = SimulateNavigateAndCommit({url}, main_rfh());
  EXPECT_CALL(*client(), ToggleNotificationVisibility(::testing::_)).Times(0);
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(rfh));
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       WhitelistSiteOnReload) {
  const struct {
    content::Referrer referrer;
    ui::PageTransition transition;
    ActivationDecision expected_activation_decision;
  } kTestCases[] = {
      {content::Referrer(), ui::PAGE_TRANSITION_LINK,
       ActivationDecision::ACTIVATED},
      {content::Referrer(GURL(kUrlA), blink::kWebReferrerPolicyDefault),
       ui::PAGE_TRANSITION_LINK, ActivationDecision::ACTIVATED},
      {content::Referrer(GURL(kURL), blink::kWebReferrerPolicyDefault),
       ui::PAGE_TRANSITION_LINK, ActivationDecision::URL_WHITELISTED},
      {content::Referrer(), ui::PAGE_TRANSITION_RELOAD,
       ActivationDecision::URL_WHITELISTED}};

  Configuration config(ActivationLevel::ENABLED, ActivationScope::ALL_SITES);
  config.activation_options.should_whitelist_site_on_reload = true;
  scoped_configuration()->ResetConfiguration(std::move(config));

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("referrer = \"")
                 << test_case.referrer.url << "\""
                 << " transition = \"" << test_case.transition << "\"");

    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(kURL), main_rfh());
    simulator->SetTransition(test_case.transition);
    simulator->SetReferrer(test_case.referrer);
    SimulateCommit(simulator.get());
    EXPECT_EQ(test_case.expected_activation_decision,
              factory()->GetActivationDecisionForLastCommittedPageLoad());
    // Verify that if the first URL failed to activate, subsequent same-origin
    // navigations also fail to activate.
    simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(kURLWithParams), main_rfh());
    SimulateCommit(simulator.get());
    EXPECT_EQ(test_case.expected_activation_decision,
              factory()->GetActivationDecisionForLastCommittedPageLoad());
  }
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ActivateForFrameState) {
  const struct {
    ActivationDecision activation_decision;
    ActivationLevel activation_level;
  } kTestCases[] = {
      {ActivationDecision::ACTIVATED, ActivationLevel::DRYRUN},
      {ActivationDecision::ACTIVATED, ActivationLevel::ENABLED},
      {ActivationDecision::ACTIVATION_DISABLED, ActivationLevel::DISABLED},
  };
  for (const auto& test_data : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "activation_decision "
                 << static_cast<int>(test_data.activation_decision)
                 << " activation_level " << test_data.activation_level);
    client()->ClearWhitelist();
    scoped_configuration()->ResetConfiguration(Configuration(
        test_data.activation_level, ActivationScope::ACTIVATION_LIST,
        ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL));
    const GURL url(kURLWithParams);
    ConfigureForMatch(url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
                      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);
    SimulateNavigateAndCommit({url}, main_rfh());
    EXPECT_EQ(test_data.activation_decision,
              factory()->GetActivationDecisionForLastCommittedPageLoad());

    // Whitelisting is only applied when the page will otherwise activate.
    client()->WhitelistInCurrentWebContents(url);
    ActivationDecision decision =
        test_data.activation_level == ActivationLevel::DISABLED
            ? test_data.activation_decision
            : ActivationDecision::URL_WHITELISTED;
    SimulateNavigateAndCommit({url}, main_rfh());
    EXPECT_EQ(decision,
              factory()->GetActivationDecisionForLastCommittedPageLoad());
  }
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest, ActivationList) {
  const struct {
    ActivationDecision expected_activation_decision;
    ActivationList activation_list;
    safe_browsing::SBThreatType threat_type;
    safe_browsing::ThreatPatternType threat_type_metadata;
  } kTestCases[] = {
      {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET, ActivationList::NONE,
       safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
       ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
       safe_browsing::ThreatPatternType::NONE},
      {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
       ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
       safe_browsing::ThreatPatternType::MALWARE_LANDING},
      {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
       ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
       safe_browsing::ThreatPatternType::MALWARE_DISTRIBUTION},
      {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
       ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_API_ABUSE,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
       ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_BLACKLISTED_RESOURCE,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
       ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
       ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_BINARY_MALWARE,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
       ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_UNWANTED,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
       ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
       ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
       ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_SAFE,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {ActivationDecision::ACTIVATED, ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
       safe_browsing::ThreatPatternType::NONE},
      {ActivationDecision::ACTIVATED,
       ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {ActivationDecision::ACTIVATED, ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {ActivationDecision::ACTIVATED, ActivationList::SUBRESOURCE_FILTER,
       safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       safe_browsing::ThreatPatternType::NONE},
      {ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
       ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       safe_browsing::ThreatPatternType::NONE},
  };
  const GURL test_url("https://matched_url.com/");
  for (const auto& test_case : kTestCases) {
    scoped_configuration()->ResetConfiguration(Configuration(
        ActivationLevel::ENABLED, ActivationScope::ACTIVATION_LIST,
        test_case.activation_list));
    ClearAllBlacklistedUrls();
    ConfigureForMatch(test_url, test_case.threat_type,
                      test_case.threat_type_metadata);
    SimulateNavigateAndCommit({GURL(kUrlA), GURL(kUrlB), GURL(kUrlC), test_url},
                              main_rfh());
    EXPECT_EQ(test_case.expected_activation_decision,
              factory()->GetActivationDecisionForLastCommittedPageLoad());
  }
}

// Regression test for an issue where synchronous failure from the SB database
// caused a double cancel. This is DCHECKed in the fake database.
TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       SynchronousResponse) {
  const GURL url(kURL);
  fake_safe_browsing_database()->set_synchronous_failure();
  SimulateStartAndExpectProceed(url);
  SimulateCommitAndExpectProceed();
  tester().ExpectTotalCount(kMatchesPatternHistogramNameSubresourceFilterSuffix,
                            0);
}

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleScopeTest,
       ActivateForScopeType) {
  const ActivationScopeTestData& test_data = GetParam();
  scoped_configuration()->ResetConfiguration(
      Configuration(ActivationLevel::ENABLED, test_data.activation_scope,
                    ActivationList::SUBRESOURCE_FILTER));

  const GURL test_url(kURLWithParams);
  if (test_data.url_matches_activation_list)
    ConfigureForMatch(test_url);
  SimulateNavigateAndCommit({test_url}, main_rfh());
  EXPECT_EQ(test_data.expected_activation_decision,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  if (test_data.url_matches_activation_list) {
    factory()->client()->WhitelistInCurrentWebContents(test_url);
    ActivationDecision expected_decision =
        test_data.expected_activation_decision;
    if (expected_decision == ActivationDecision::ACTIVATED)
      expected_decision = ActivationDecision::URL_WHITELISTED;
    SimulateNavigateAndCommit({test_url}, main_rfh());
    EXPECT_EQ(expected_decision,
              factory()->GetActivationDecisionForLastCommittedPageLoad());
  }
};

// Only main frames with http/https schemes should activate.
TEST_P(SubresourceFilterSafeBrowsingActivationThrottleScopeTest,
       ActivateForSupportedUrlScheme) {
  const ActivationScopeTestData& test_data = GetParam();
  scoped_configuration()->ResetConfiguration(
      Configuration(ActivationLevel::ENABLED, test_data.activation_scope,
                    ActivationList::SUBRESOURCE_FILTER));

  // data URLs are also not supported, but not listed here, as it's not possible
  // for a page to redirect to them after https://crbug.com/594215 is fixed.
  const char* unsupported_urls[] = {"ftp://example.com/", "chrome://settings",
                                    "chrome-extension://some-extension",
                                    "file:///var/www/index.html"};
  const char* supported_urls[] = {"http://example.test",
                                  "https://example.test"};
  for (auto* url : unsupported_urls) {
    SCOPED_TRACE(url);
    if (test_data.url_matches_activation_list)
      ConfigureForMatch(GURL(url));
    SimulateNavigateAndCommit({GURL(url)}, main_rfh());
    ActivationDecision expected_decision =
        ActivationDecision::UNSUPPORTED_SCHEME;
    // We only log UNSUPPORTED_SCHEME if the navigation would have otherwise
    // activated. Note that non http/s URLs will never match an activation list.
    if (test_data.expected_activation_decision ==
            ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET ||
        test_data.activation_scope == ActivationScope::ACTIVATION_LIST) {
      expected_decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET;
    }
    EXPECT_EQ(expected_decision,
              factory()->GetActivationDecisionForLastCommittedPageLoad());
  }

  for (auto* url : supported_urls) {
    SCOPED_TRACE(url);
    if (test_data.url_matches_activation_list)
      ConfigureForMatch(GURL(url));
    SimulateNavigateAndCommit({GURL(url)}, main_rfh());
    EXPECT_EQ(test_data.expected_activation_decision,
              factory()->GetActivationDecisionForLastCommittedPageLoad());
  }
};

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       ListNotMatched_NoActivation) {
  const ActivationListTestData& test_data = GetParam();
  const GURL url(kURL);
  const std::string suffix(GetSuffixForList(test_data.activation_list_type));
  SimulateStartAndExpectProceed(url);
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  ExpectSampleForSuffix("SocialEngineeringAdsInterstitial", std::string(),
                        tester());
  ExpectSampleForSuffix("PhishingInterstitial", std::string(), tester());
  ExpectSampleForSuffix("SubresourceFilterOnly", std::string(), tester());

  tester().ExpectTotalCount(kSafeBrowsingNavigationDelay, 1);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelayNoSpeculation, 1);
  tester().ExpectTotalCount(kSafeBrowsingCheckTime, 1);
  tester().ExpectTotalCount(kNavigationChainSize + suffix, 0);
}

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       ListMatched_Activation) {
  const ActivationListTestData& test_data = GetParam();
  const GURL url(kURL);
  ConfigureForMatchParam(url);
  SimulateStartAndExpectProceed(url);
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(ActivationDecision::ACTIVATED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  const std::string suffix(GetSuffixForList(test_data.activation_list_type));
  ExpectSampleForSuffix("SocialEngineeringAdsInterstitial", suffix, tester());
  ExpectSampleForSuffix("PhishingInterstitial", suffix, tester());
  ExpectSampleForSuffix("SubresourceFilterOnly", suffix, tester());
  tester().ExpectUniqueSample(kNavigationChainSize + suffix, 1, 1);
}

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       ListNotMatchedAfterRedirect_NoActivation) {
  const ActivationListTestData& test_data = GetParam();
  const GURL url(kURL);
  const std::string suffix(GetSuffixForList(test_data.activation_list_type));
  SimulateStartAndExpectProceed(url);
  SimulateRedirectAndExpectProceed(GURL(kRedirectURL));
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  ExpectSampleForSuffix("SocialEngineeringAdsInterstitial", std::string(),
                        tester());
  ExpectSampleForSuffix("PhishingInterstitial", std::string(), tester());
  ExpectSampleForSuffix("SubresourceFilterOnly", std::string(), tester());
  tester().ExpectTotalCount(kNavigationChainSize + suffix, 0);
}

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       ListMatchedAfterRedirect_Activation) {
  const ActivationListTestData& test_data = GetParam();
  const GURL url(kURL);
  const std::string suffix(GetSuffixForList(test_data.activation_list_type));
  ConfigureForMatchParam(GURL(kRedirectURL));
  SimulateStartAndExpectProceed(url);
  SimulateRedirectAndExpectProceed(GURL(kRedirectURL));
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(ActivationDecision::ACTIVATED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  tester().ExpectUniqueSample(kNavigationChainSize + suffix, 2, 1);
  ExpectSampleForSuffix("SocialEngineeringAdsInterstitial", suffix, tester());
  ExpectSampleForSuffix("PhishingInterstitial", suffix, tester());
  ExpectSampleForSuffix("SubresourceFilterOnly", suffix, tester());
}

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       ListNotMatchedAndTimeout_NoActivation) {
  const ActivationListTestData& test_data = GetParam();
  const GURL url(kURL);
  const std::string suffix(GetSuffixForList(test_data.activation_list_type));
  fake_safe_browsing_database()->SimulateTimeout();
  SimulateStartAndExpectProceed(url);

  // Flush the pending tasks on the IO thread, so the delayed task surely gets
  // posted.
  test_io_task_runner()->RunUntilIdle();

  // Expect one delayed task, and fast forward time.
  base::TimeDelta expected_delay =
      SubresourceFilterSafeBrowsingClientRequest::kCheckURLTimeout;
  EXPECT_EQ(expected_delay, test_io_task_runner()->NextPendingTaskDelay());
  test_io_task_runner()->FastForwardBy(expected_delay);
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  tester().ExpectTotalCount(kMatchesPatternHistogramNameSubresourceFilterSuffix,
                            0);
  tester().ExpectTotalCount(kNavigationChainSizeSubresourceFilterSuffix, 0);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelay, 1);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelayNoSpeculation, 1);
  tester().ExpectTotalCount(kSafeBrowsingCheckTime, 1);
}

// Flaky on Win, Chromium and Linux. http://crbug.com/748524
TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       DISABLED_ListMatchedOnStart_NoDelay) {
  const ActivationListTestData& test_data = GetParam();
  const GURL url(kURL);
  ConfigureForMatchParam(url);
  SimulateStartAndExpectProceed(url);

  // Get the database result back before commit.
  RunUntilIdle();

  SimulateCommitAndExpectProceed();
  EXPECT_EQ(ActivationDecision::ACTIVATED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  const std::string suffix(GetSuffixForList(test_data.activation_list_type));
  ExpectSampleForSuffix("SocialEngineeringAdsInterstitial", suffix, tester());
  ExpectSampleForSuffix("PhishingInterstitial", suffix, tester());
  ExpectSampleForSuffix("SubresourceFilterOnly", suffix, tester());
  tester().ExpectUniqueSample(kNavigationChainSize + suffix, 1, 1);

  tester().ExpectTimeBucketCount(kSafeBrowsingNavigationDelay,
                                 base::TimeDelta::FromMilliseconds(0), 1);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelayNoSpeculation, 1);
}

// Flaky on Win, Chromium and Linux. http://crbug.com/748524
TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       DISABLED_ListMatchedOnRedirect_NoDelay) {
  const ActivationListTestData& test_data = GetParam();
  const GURL url(kURL);
  const GURL redirect_url(kRedirectURL);
  ConfigureForMatchParam(redirect_url);

  SimulateStartAndExpectProceed(url);
  SimulateRedirectAndExpectProceed(redirect_url);

  // Get the database result back before commit.
  RunUntilIdle();

  SimulateCommitAndExpectProceed();
  EXPECT_EQ(ActivationDecision::ACTIVATED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  const std::string suffix(GetSuffixForList(test_data.activation_list_type));
  ExpectSampleForSuffix("SocialEngineeringAdsInterstitial", suffix, tester());
  ExpectSampleForSuffix("PhishingInterstitial", suffix, tester());
  ExpectSampleForSuffix("SubresourceFilterOnly", suffix, tester());
  tester().ExpectUniqueSample(kNavigationChainSize + suffix, 2, 1);

  tester().ExpectTimeBucketCount(kSafeBrowsingNavigationDelay,
                                 base::TimeDelta::FromMilliseconds(0), 1);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelayNoSpeculation, 1);
  tester().ExpectTotalCount(kSafeBrowsingCheckTime, 2);
}

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       ListMatchedOnStartWithRedirect_NoActivation) {
  const GURL url(kURL);
  const GURL redirect_url(kRedirectURL);
  ConfigureForMatchParam(url);

  // These two lines also test how the database client reacts to two requests
  // happening one after another.
  SimulateStartAndExpectProceed(url);
  SimulateRedirectAndExpectProceed(redirect_url);

  // Get the database result back before commit.
  RunUntilIdle();

  SimulateCommitAndExpectProceed();
  EXPECT_EQ(ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  tester().ExpectTotalCount(kMatchesPatternHistogramNameSubresourceFilterSuffix,
                            0);
  tester().ExpectTotalCount(kNavigationChainSizeSubresourceFilterSuffix, 0);
  tester().ExpectTimeBucketCount(kSafeBrowsingNavigationDelay,
                                 base::TimeDelta::FromMilliseconds(0), 1);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelayNoSpeculation, 1);
}

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       RedirectPatternTest) {
  struct RedirectRedirectChainMatchPatternTestData {
    std::vector<bool> blacklisted_urls;
    std::vector<GURL> navigation_chain;
  } kRedirectRecordedHistogramsTestData[] = {
      {{false}, {GURL(kUrlA)}},
      {{true}, {GURL(kUrlA)}},
      {{false, false}, {GURL(kUrlA), GURL(kUrlB)}},
      {{false, true}, {GURL(kUrlA), GURL(kUrlB)}},
      {{true, false}, {GURL(kUrlA), GURL(kUrlB)}},
      {{true, true}, {GURL(kUrlA), GURL(kUrlB)}},
      {{false, false, false}, {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)}},
      {{false, false, true}, {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)}},
      {{false, true, false}, {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)}},
      {{false, true, true}, {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)}},
      {{true, false, false}, {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)}},
      {{true, false, true}, {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)}},
      {{true, true, false}, {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)}},
      {{true, true, true}, {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)}},
      {{false, true, false, false},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC), GURL(kUrlD)}},
  };

  for (const auto& test_data : kRedirectRecordedHistogramsTestData) {
    base::HistogramTester histogram_tester;
    ClearAllBlacklistedUrls();
    auto it = test_data.navigation_chain.begin();
    for (size_t i = 0u; i < test_data.blacklisted_urls.size(); ++i) {
      if (test_data.blacklisted_urls[i])
        ConfigureForMatchParam(test_data.navigation_chain[i]);
    }
    SimulateStartAndExpectProceed(*it);
    for (++it; it != test_data.navigation_chain.end(); ++it)
      SimulateRedirectAndExpectProceed(*it);
    SimulateCommitAndExpectProceed();

    // Verify histograms
    const std::string suffix_param(
        GetSuffixForList(GetParam().activation_list_type));
    auto check_histogram = [&](std::string suffix) {
      bool matches =
          suffix == suffix_param && test_data.blacklisted_urls.back();
      histogram_tester.ExpectBucketCount(kMatchesPatternHistogramName + suffix,
                                         matches, 1);

      if (matches) {
        histogram_tester.ExpectBucketCount(kNavigationChainSize + suffix,
                                           test_data.navigation_chain.size(),
                                           1);
      } else {
        histogram_tester.ExpectTotalCount(kNavigationChainSize + suffix, 0);
      }
    };

    check_histogram("SocialEngineeringAdsInterstitial");
    check_histogram("PhishingInterstitial");
    check_histogram("SubresourceFilterOnly");
  }
}

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleTestWithCancelling,
       Cancel) {
  const GURL url(kURL);
  SCOPED_TRACE(::testing::Message() << "CancelTime: " << cancel_time()
                                    << " ResultSynchrony: " << result_sync());
  ConfigureForMatch(url);
  content::NavigationThrottle::ThrottleCheckResult result =
      SimulateStart(url, main_rfh());
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

  result = SimulateCommit(navigation_simulator());
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

INSTANTIATE_TEST_CASE_P(
    ActivationLevelTest,
    SubresourceFilterSafeBrowsingActivationThrottleParamTest,
    ::testing::ValuesIn(kActivationListTestData));

INSTANTIATE_TEST_CASE_P(
    ActivationScopeTest,
    SubresourceFilterSafeBrowsingActivationThrottleScopeTest,
    ::testing::ValuesIn(kActivationScopeTestData));

}  // namespace subresource_filter
