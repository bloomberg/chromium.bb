// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/test/histogram_tester.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

char kURL[] = "http://example.test/";
char kRedirectURL[] = "http://foo.test/";

// Names of navigation chain patterns histogram.
const char kMatchesPatternHistogramNameSubresourceFilterSuffix[] =
    "SubresourceFilter.PageLoad.RedirectChainMatchPattern."
    "SubresourceFilterOnly";
const char kNavigationChainSizeSubresourceFilterSuffix[] =
    "SubresourceFilter.PageLoad.RedirectChainLength.SubresourceFilterOnly";

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

// Database manager that allows any URL to be configured as blacklisted for
// testing.
class FakeSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  FakeSafeBrowsingDatabaseManager() : simulate_timeout_(false) {}

  void AddBlacklistedUrl(const GURL& url,
                         safe_browsing::SBThreatType threat_type) {
    url_to_threat_type_[url] = threat_type;
  }

  void SimulateTimeout() { simulate_timeout_ = true; }

 protected:
  ~FakeSafeBrowsingDatabaseManager() override {}

  bool CheckUrlForSubresourceFilter(const GURL& url, Client* client) override {
    if (simulate_timeout_)
      return false;
    if (!url_to_threat_type_.count(url))
      return true;

    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&Client::OnCheckBrowseUrlResult, base::Unretained(client),
                   url, url_to_threat_type_[url],
                   safe_browsing::ThreatMetadata()));
    return false;
  }

  bool CheckResourceUrl(const GURL& url, Client* client) override {
    return true;
  }

  bool IsSupported() const override { return true; }
  bool ChecksAreAlwaysAsync() const override { return false; }
  bool CanCheckResourceType(
      content::ResourceType /* resource_type */) const override {
    return true;
  }

  safe_browsing::ThreatSource GetThreatSource() const override {
    return safe_browsing::ThreatSource::LOCAL_PVER4;
  }

  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override {
    return true;
  }

 private:
  std::map<GURL, safe_browsing::SBThreatType> url_to_threat_type_;
  bool simulate_timeout_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingDatabaseManager);
};

class MockSubresourceFilterClient
    : public subresource_filter::SubresourceFilterClient {
 public:
  MockSubresourceFilterClient() {}

  ~MockSubresourceFilterClient() override = default;

  MOCK_METHOD1(ToggleNotificationVisibility, void(bool));
  MOCK_METHOD1(IsWhitelistedByContentSettings, bool(const GURL&));
  MOCK_METHOD1(WhitelistByContentSettings, void(const GURL&));
  MOCK_METHOD0(GetRulesetDealer, VerifiedRulesetDealer::Handle*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSubresourceFilterClient);
};

// Throttle to call WillProcessResponse on the factory, which is otherwise
// called by the ThrottleManager.
class TestForwardingNavigationThrottle : public content::NavigationThrottle {
 public:
  TestForwardingNavigationThrottle(content::NavigationHandle* handle)
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
    scoped_feature_toggle_.reset(
        new testing::ScopedSubresourceFilterFeatureToggle(
            base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationLevelEnabled,
            kActivationScopeActivationList, kActivationListSubresourceFilter));
    // Note: Using NiceMock to allow uninteresting calls and suppress warnings.
    auto client =
        base::MakeUnique<::testing::NiceMock<MockSubresourceFilterClient>>();
    ContentSubresourceFilterDriverFactory::CreateForWebContents(
        RenderViewHostTestHarness::web_contents(), std::move(client));
    fake_safe_browsing_database_ = new FakeSafeBrowsingDatabaseManager();
    NavigateAndCommit(GURL("https://test.com"));
    Observe(RenderViewHostTestHarness::web_contents());
  }

  ContentSubresourceFilterDriverFactory* factory() {
    return ContentSubresourceFilterDriverFactory::FromWebContents(
        RenderViewHostTestHarness::web_contents());
  }

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    ASSERT_TRUE(navigation_handle->IsInMainFrame());
    navigation_handle_ = navigation_handle;
    navigation_handle->RegisterThrottleForTesting(
        base::MakeUnique<SubresourceFilterSafeBrowsingActivationThrottle>(
            navigation_handle, fake_safe_browsing_database_));
    navigation_handle->RegisterThrottleForTesting(
        base::MakeUnique<TestForwardingNavigationThrottle>(navigation_handle));
  }

  void SimulateStartAndExpectProceed() {
    navigation_simulator_->Start();
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              navigation_simulator_->GetLastThrottleCheckResult());
  }

  void SimulateRedirectAndExpectProceed(const GURL& new_url) {
    navigation_simulator_->Redirect(new_url);
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              navigation_simulator_->GetLastThrottleCheckResult());
  }

  void SimulateCommitAndExpectProceed() {
    navigation_simulator_->Commit();
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              navigation_simulator_->GetLastThrottleCheckResult());
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

  const base::HistogramTester& tester() const { return tester_; }

 private:
  base::FieldTrialList field_trial_list_;
  std::unique_ptr<testing::ScopedSubresourceFilterFeatureToggle>
      scoped_feature_toggle_;
  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;
  scoped_refptr<FakeSafeBrowsingDatabaseManager> fake_safe_browsing_database_;
  base::HistogramTester tester_;
  content::NavigationHandle* navigation_handle_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterSafeBrowsingActivationThrottleTest);
};

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ListNotMatched_NoActivation) {
  const GURL url(kURL);
  CreateTestNavigationForMainFrame(url);
  SimulateStartAndExpectProceed();
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(ContentSubresourceFilterDriverFactory::ActivationDecision::
                ACTIVATION_LIST_NOT_MATCHED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  tester().ExpectTotalCount(kMatchesPatternHistogramNameSubresourceFilterSuffix,
                            0);
  tester().ExpectTotalCount(kNavigationChainSizeSubresourceFilterSuffix, 0);
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
                ACTIVATION_LIST_NOT_MATCHED,
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
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(ContentSubresourceFilterDriverFactory::ActivationDecision::
                ACTIVATION_LIST_NOT_MATCHED,
            factory()->GetActivationDecisionForLastCommittedPageLoad());
  tester().ExpectTotalCount(kMatchesPatternHistogramNameSubresourceFilterSuffix,
                            0);
  tester().ExpectTotalCount(kNavigationChainSizeSubresourceFilterSuffix, 0);
}

// TODO(melandory): Once non-defering check in WillStart is implemented add one
// more test that destroys the Navigation along with corresponding throttles
// while the SB check is pending? (To be run by ASAN bots to ensure
// no use-after-free.)

}  // namespace subresource_filter
