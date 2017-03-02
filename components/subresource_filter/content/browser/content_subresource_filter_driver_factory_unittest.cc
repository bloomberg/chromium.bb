// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/test/histogram_tester.h"
#include "components/safe_browsing_db/util.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/core/browser/subresource_filter_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

using ActivationDecision =
    ContentSubresourceFilterDriverFactory::ActivationDecision;

namespace {

const char kExampleUrlWithParams[] = "https://example.com/soceng?q=engsoc";
const char kExampleUrl[] = "https://example.com";
const char kExampleLoginUrl[] = "https://example.com/login";
const char kMatchesPatternHistogramName[] =
    "SubresourceFilter.PageLoad.RedirectChainMatchPattern";
const char kNavigationChainSize[] =
    "SubresourceFilter.PageLoad.RedirectChainLength";
const char kUrlA[] = "https://example_a.com";
const char kUrlB[] = "https://example_b.com";
const char kUrlC[] = "https://example_c.com";
const char kUrlD[] = "https://example_d.com";
const char kSubframeName[] = "Child";

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

struct ActivationListTestData {
  ActivationDecision expected_activation_decision;
  const char* const activation_list;
  safe_browsing::SBThreatType threat_type;
  safe_browsing::ThreatPatternType threat_type_metadata;
};

const ActivationListTestData kActivationListTestData[] = {
    {ActivationDecision::ACTIVATION_LIST_NOT_MATCHED, "",
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {ActivationDecision::ACTIVATION_LIST_NOT_MATCHED,
     subresource_filter::kActivationListSocialEngineeringAdsInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::NONE},
    {ActivationDecision::ACTIVATION_LIST_NOT_MATCHED,
     subresource_filter::kActivationListSocialEngineeringAdsInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::MALWARE_LANDING},
    {ActivationDecision::ACTIVATION_LIST_NOT_MATCHED,
     subresource_filter::kActivationListSocialEngineeringAdsInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::MALWARE_DISTRIBUTION},
    {ActivationDecision::ACTIVATION_LIST_NOT_MATCHED,
     subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_API_ABUSE,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {ActivationDecision::ACTIVATION_LIST_NOT_MATCHED,
     subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_BLACKLISTED_RESOURCE,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {ActivationDecision::ACTIVATION_LIST_NOT_MATCHED,
     subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {ActivationDecision::ACTIVATION_LIST_NOT_MATCHED,
     subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_BINARY_MALWARE_URL,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {ActivationDecision::ACTIVATION_LIST_NOT_MATCHED,
     subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_UNWANTED,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {ActivationDecision::ACTIVATION_LIST_NOT_MATCHED,
     subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {ActivationDecision::ACTIVATION_LIST_NOT_MATCHED,
     subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {ActivationDecision::ACTIVATION_LIST_NOT_MATCHED,
     subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_SAFE,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {ActivationDecision::ACTIVATED,
     subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::NONE},
    {ActivationDecision::ACTIVATED,
     subresource_filter::kActivationListSocialEngineeringAdsInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
};

struct ActivationScopeTestData {
  ActivationDecision expected_activation_decision;
  bool url_matches_activation_list;
  const char* const activation_scope;
};

const ActivationScopeTestData kActivationScopeTestData[] = {
    {ActivationDecision::ACTIVATED, false /* url_matches_activation_list */,
     kActivationScopeAllSites},
    {ActivationDecision::ACTIVATED, true /* url_matches_activation_list */,
     kActivationScopeAllSites},
    {ActivationDecision::ACTIVATION_DISABLED,
     true /* url_matches_activation_list */, kActivationScopeNoSites},
    {ActivationDecision::ACTIVATED, true /* url_matches_activation_list */,
     kActivationScopeActivationList},
    {ActivationDecision::ACTIVATION_LIST_NOT_MATCHED,
     false /* url_matches_activation_list */, kActivationScopeActivationList},
};

struct ActivationLevelTestData {
  ActivationDecision expected_activation_decision;
  const char* const activation_level;
};

const ActivationLevelTestData kActivationLevelTestData[] = {
    {ActivationDecision::ACTIVATED, kActivationLevelDryRun},
    {ActivationDecision::ACTIVATED, kActivationLevelEnabled},
    {ActivationDecision::ACTIVATION_DISABLED, kActivationLevelDisabled},
};

class MockSubresourceFilterClient : public SubresourceFilterClient {
 public:
  MockSubresourceFilterClient() {}

  ~MockSubresourceFilterClient() override = default;

  MOCK_METHOD1(ToggleNotificationVisibility, void(bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSubresourceFilterClient);
};

}  // namespace

class ContentSubresourceFilterDriverFactoryTest
    : public content::RenderViewHostTestHarness {
 public:
  ContentSubresourceFilterDriverFactoryTest() {}
  ~ContentSubresourceFilterDriverFactoryTest() override {}

  // content::RenderViewHostImplTestHarness:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    client_ = new MockSubresourceFilterClient();
    ContentSubresourceFilterDriverFactory::CreateForWebContents(
        web_contents(), base::WrapUnique(client()));

    // Add a subframe.
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());
    rfh_tester->InitializeRenderFrameIfNeeded();
    rfh_tester->AppendChild(kSubframeName);
  }

  ContentSubresourceFilterDriverFactory* factory() {
    return ContentSubresourceFilterDriverFactory::FromWebContents(
        web_contents());
  }

  MockSubresourceFilterClient* client() { return client_; }

  content::RenderFrameHost* GetSubframeRFH() {
    for (content::RenderFrameHost* rfh : web_contents()->GetAllFrames()) {
      if (rfh->GetFrameName() == kSubframeName)
        return rfh;
    }
    return nullptr;
  }

  void ExpectActivationSignalForFrame(content::RenderFrameHost* rfh,
                                      bool expect_activation) {
    content::MockRenderProcessHost* render_process_host =
        static_cast<content::MockRenderProcessHost*>(rfh->GetProcess());
    const IPC::Message* message =
        render_process_host->sink().GetFirstMessageMatching(
            SubresourceFilterMsg_ActivateForNextCommittedLoad::ID);
    ASSERT_EQ(expect_activation, !!message);
    if (expect_activation) {
      std::tuple<ActivationLevel, bool> args;
      SubresourceFilterMsg_ActivateForNextCommittedLoad::Read(message, &args);
      EXPECT_NE(ActivationLevel::DISABLED, std::get<0>(args));
    }
    render_process_host->sink().ClearMessages();
  }

  void BlacklistURLWithRedirectsNavigateAndCommit(
      const std::vector<bool>& blacklisted_urls,
      const std::vector<GURL>& navigation_chain,
      safe_browsing::SBThreatType threat_type,
      safe_browsing::ThreatPatternType threat_type_metadata,
      const content::Referrer& referrer,
      ui::PageTransition transition,
      RedirectChainMatchPattern expected_pattern,
      ActivationDecision expected_activation_decision) {
    const bool expected_activation =
        expected_activation_decision == ActivationDecision::ACTIVATED;
    base::HistogramTester tester;
    EXPECT_CALL(*client(), ToggleNotificationVisibility(false)).Times(1);

    std::unique_ptr<content::NavigationSimulator> navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(
            navigation_chain.front(), main_rfh());
    navigation_simulator->SetReferrer(referrer);
    navigation_simulator->SetTransition(transition);
    navigation_simulator->Start();

    if (blacklisted_urls.front()) {
      factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
          navigation_chain.front(), navigation_chain, threat_type,
          threat_type_metadata);
    }
    ::testing::Mock::VerifyAndClearExpectations(client());

    for (size_t i = 1; i < navigation_chain.size(); ++i) {
      const GURL url = navigation_chain[i];
      if (i < blacklisted_urls.size() && blacklisted_urls[i]) {
        factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
            url, navigation_chain, threat_type, threat_type_metadata);
      }
      navigation_simulator->Redirect(url);
    }

    navigation_simulator->Commit();
    ExpectActivationSignalForFrame(main_rfh(), expected_activation);
    EXPECT_EQ(expected_activation_decision,
              factory()->GetActivationDecisionForLastCommittedPageLoad());

    // Re-create a subframe now that the frame has navigated.
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());
    rfh_tester->AppendChild(kSubframeName);

    if (expected_pattern != EMPTY) {
      EXPECT_THAT(tester.GetAllSamples(kMatchesPatternHistogramName),
                  ::testing::ElementsAre(base::Bucket(expected_pattern, 1)));
      EXPECT_THAT(
          tester.GetAllSamples(kNavigationChainSize),
          ::testing::ElementsAre(base::Bucket(navigation_chain.size(), 1)));

    } else {
      EXPECT_THAT(tester.GetAllSamples(kMatchesPatternHistogramName),
                  ::testing::IsEmpty());
      EXPECT_THAT(tester.GetAllSamples(kNavigationChainSize),
                  ::testing::IsEmpty());
    }
  }

  void NavigateAndCommitSubframe(const GURL& url, bool expected_activation) {
    EXPECT_CALL(*client(), ToggleNotificationVisibility(::testing::_)).Times(0);

    content::NavigationSimulator::NavigateAndCommitFromDocument(
        url, GetSubframeRFH());
    ExpectActivationSignalForFrame(GetSubframeRFH(), expected_activation);
    ::testing::Mock::VerifyAndClearExpectations(client());
  }

  void NavigateAndExpectActivation(
      const std::vector<bool>& blacklisted_urls,
      const std::vector<GURL>& navigation_chain,
      safe_browsing::SBThreatType threat_type,
      safe_browsing::ThreatPatternType threat_type_metadata,
      const content::Referrer& referrer,
      ui::PageTransition transition,
      RedirectChainMatchPattern expected_pattern,
      ActivationDecision expected_activation_decision) {
    const bool expected_activation =
        expected_activation_decision == ActivationDecision::ACTIVATED;
    BlacklistURLWithRedirectsNavigateAndCommit(
        blacklisted_urls, navigation_chain, threat_type, threat_type_metadata,
        referrer, transition, expected_pattern, expected_activation_decision);

    NavigateAndCommitSubframe(GURL(kExampleLoginUrl), expected_activation);
  }

  void NavigateAndExpectActivation(
      const std::vector<bool>& blacklisted_urls,
      const std::vector<GURL>& navigation_chain,
      RedirectChainMatchPattern expected_pattern,
      ActivationDecision expected_activation_decision) {
    NavigateAndExpectActivation(
        blacklisted_urls, navigation_chain,
        safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
        safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS,
        content::Referrer(), ui::PAGE_TRANSITION_LINK, expected_pattern,
        expected_activation_decision);
  }

  void EmulateDidDisallowFirstSubresourceMessage() {
    factory()->OnMessageReceived(
        SubresourceFilterHostMsg_DidDisallowFirstSubresource(
            main_rfh()->GetRoutingID()),
        main_rfh());
  }

  void EmulateFailedNavigationAndExpectNoActivation(const GURL& url) {
    EXPECT_CALL(*client(), ToggleNotificationVisibility(false)).Times(1);

    // With browser-side navigation enabled, ReadyToCommitNavigation is invoked
    // even for failed navigations. This is correctly simulated by
    // NavigationSimulator. Make sure no activation message is sent in this
    // case.
    content::NavigationSimulator::NavigateAndFailFromDocument(
        url, net::ERR_TIMED_OUT, main_rfh());
    ExpectActivationSignalForFrame(main_rfh(), false);
    ::testing::Mock::VerifyAndClearExpectations(client());
  }

  void EmulateInPageNavigation(
      const std::vector<bool>& blacklisted_urls,
      RedirectChainMatchPattern expected_pattern,
      ActivationDecision expected_activation_decision) {
    // This test examines the navigation with the following sequence of events:
    //   DidStartProvisional(main, "example.com")
    //   ReadyToCommitNavigation(“example.com”)
    //   DidCommitProvisional(main, "example.com")
    //   DidStartProvisional(sub, "example.com/login")
    //   DidCommitProvisional(sub, "example.com/login")
    //   DidCommitProvisional(main, "example.com#ref")

    NavigateAndExpectActivation(blacklisted_urls, {GURL(kExampleUrl)},
                                expected_pattern, expected_activation_decision);
    EXPECT_CALL(*client(), ToggleNotificationVisibility(::testing::_)).Times(0);
    std::unique_ptr<content::NavigationSimulator> navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(GURL(kExampleUrl),
                                                              main_rfh());
    navigation_simulator->CommitSamePage();
    ExpectActivationSignalForFrame(main_rfh(), false);
    ::testing::Mock::VerifyAndClearExpectations(client());
  }

 private:
  static bool expected_measure_performance() {
    const double rate = GetPerformanceMeasurementRate();
    // Note: The case when 0 < rate < 1 is not deterministic, don't test it.
    EXPECT_TRUE(rate == 0 || rate == 1);
    return rate == 1;
  }

  // Owned by the factory.
  MockSubresourceFilterClient* client_;

  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriverFactoryTest);
};

class ContentSubresourceFilterDriverFactoryThreatTypeTest
    : public ContentSubresourceFilterDriverFactoryTest,
      public ::testing::WithParamInterface<ActivationListTestData> {
 public:
  ContentSubresourceFilterDriverFactoryThreatTypeTest() {}
  ~ContentSubresourceFilterDriverFactoryThreatTypeTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriverFactoryThreatTypeTest);
};

class ContentSubresourceFilterDriverFactoryActivationScopeTest
    : public ContentSubresourceFilterDriverFactoryTest,
      public ::testing::WithParamInterface<ActivationScopeTestData> {
 public:
  ContentSubresourceFilterDriverFactoryActivationScopeTest() {}
  ~ContentSubresourceFilterDriverFactoryActivationScopeTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      ContentSubresourceFilterDriverFactoryActivationScopeTest);
};

class ContentSubresourceFilterDriverFactoryActivationLevelTest
    : public ContentSubresourceFilterDriverFactoryTest,
      public ::testing::WithParamInterface<ActivationLevelTestData> {
 public:
  ContentSubresourceFilterDriverFactoryActivationLevelTest() {}
  ~ContentSubresourceFilterDriverFactoryActivationLevelTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      ContentSubresourceFilterDriverFactoryActivationLevelTest);
};

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       ActivateForFrameHostDisabledFeature) {
  // Activation scope is set to NONE => no activation should happen even if URL
  // which is visited was a SB hit.
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_DISABLE_FEATURE, kActivationLevelEnabled,
      kActivationScopeAllSites,
      kActivationListSocialEngineeringAdsInterstitial);
  const GURL url(kExampleUrlWithParams);
  NavigateAndExpectActivation({true}, {url}, EMPTY,
                              ActivationDecision::ACTIVATION_DISABLED);
  factory()->AddHostOfURLToWhitelistSet(url);
  NavigateAndExpectActivation({true}, {url}, EMPTY,
                              ActivationDecision::ACTIVATION_DISABLED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest, NoActivationWhenNoMatch) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationLevelEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  NavigateAndExpectActivation({false}, {GURL(kExampleUrl)}, EMPTY,
                              ActivationDecision::ACTIVATION_LIST_NOT_MATCHED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       SpecialCaseNavigationAllSitesEnabled) {
  // Check that when the experiment is enabled for all site, the activation
  // signal is always sent.
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationLevelEnabled,
      kActivationScopeAllSites);
  EmulateInPageNavigation({false}, EMPTY, ActivationDecision::ACTIVATED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       SpecialCaseNavigationActivationListEnabled) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationLevelEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  EmulateInPageNavigation({true}, NO_REDIRECTS_HIT,
                          ActivationDecision::ACTIVATED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       SpecialCaseNavigationActivationListEnabledWithPerformanceMeasurement) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationLevelEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial,
      "1" /* performance_measurement_rate */);
  EmulateInPageNavigation({true}, NO_REDIRECTS_HIT,
                          ActivationDecision::ACTIVATED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest, FailedNavigation) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationLevelEnabled,
      kActivationScopeAllSites);
  const GURL url(kExampleUrl);
  NavigateAndExpectActivation({false}, {url}, EMPTY,
                              ActivationDecision::ACTIVATED);
  EmulateFailedNavigationAndExpectNoActivation(url);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest, RedirectPatternTest) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationLevelEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  struct RedirectRedirectChainMatchPatternTestData {
    std::vector<bool> blacklisted_urls;
    std::vector<GURL> navigation_chain;
    RedirectChainMatchPattern hit_expected_pattern;
    ActivationDecision expected_activation_decision;
  } kRedirectRedirectChainMatchPatternTestData[] = {
      {{false},
       {GURL(kUrlA)},
       EMPTY,
       ActivationDecision::ACTIVATION_LIST_NOT_MATCHED},
      {{true}, {GURL(kUrlA)}, NO_REDIRECTS_HIT, ActivationDecision::ACTIVATED},
      {{false, false},
       {GURL(kUrlA), GURL(kUrlB)},
       EMPTY,
       ActivationDecision::ACTIVATION_LIST_NOT_MATCHED},
      {{false, true},
       {GURL(kUrlA), GURL(kUrlB)},
       F0M0L1,
       ActivationDecision::ACTIVATED},
      {{true, false},
       {GURL(kUrlA), GURL(kUrlB)},
       F1M0L0,
       ActivationDecision::ACTIVATION_LIST_NOT_MATCHED},
      {{true, true},
       {GURL(kUrlA), GURL(kUrlB)},
       F1M0L1,
       ActivationDecision::ACTIVATED},

      {{false, false, false},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       EMPTY,
       ActivationDecision::ACTIVATION_LIST_NOT_MATCHED},
      {{false, false, true},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       F0M0L1,
       ActivationDecision::ACTIVATED},
      {{false, true, false},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       F0M1L0,
       ActivationDecision::ACTIVATION_LIST_NOT_MATCHED},
      {{false, true, true},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       F0M1L1,
       ActivationDecision::ACTIVATED},
      {{true, false, false},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       F1M0L0,
       ActivationDecision::ACTIVATION_LIST_NOT_MATCHED},
      {{true, false, true},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       F1M0L1,
       ActivationDecision::ACTIVATED},
      {{true, true, false},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       F1M1L0,
       ActivationDecision::ACTIVATION_LIST_NOT_MATCHED},
      {{true, true, true},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       F1M1L1,
       ActivationDecision::ACTIVATED},
      {{false, true, false, false},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC), GURL(kUrlD)},
       F0M1L0,
       ActivationDecision::ACTIVATION_LIST_NOT_MATCHED},
  };

  for (size_t i = 0U; i < arraysize(kRedirectRedirectChainMatchPatternTestData);
       ++i) {
    auto test_data = kRedirectRedirectChainMatchPatternTestData[i];
    NavigateAndExpectActivation(
        test_data.blacklisted_urls, test_data.navigation_chain,
        safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
        safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS,
        content::Referrer(), ui::PAGE_TRANSITION_LINK,
        test_data.hit_expected_pattern, test_data.expected_activation_decision);
    NavigateAndExpectActivation(
        {false}, {GURL("https://dummy.com")}, EMPTY,
        ActivationDecision::ACTIVATION_LIST_NOT_MATCHED);
  }
}

TEST_F(ContentSubresourceFilterDriverFactoryTest, NotificationVisibility) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationLevelEnabled,
      kActivationScopeAllSites);

  NavigateAndExpectActivation({false}, {GURL(kExampleUrl)}, EMPTY,
                              ActivationDecision::ACTIVATED);
  EXPECT_CALL(*client(), ToggleNotificationVisibility(true)).Times(1);
  EmulateDidDisallowFirstSubresourceMessage();
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       SuppressNotificationVisibility) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationLevelEnabled,
      kActivationScopeAllSites, "" /* activation_lists */,
      "" /* performance_measurement_rate */,
      "true" /* suppress_notifications */);

  NavigateAndExpectActivation({false}, {GURL(kExampleUrl)}, EMPTY,
                              ActivationDecision::ACTIVATED);
  EXPECT_CALL(*client(), ToggleNotificationVisibility(::testing::_)).Times(0);
  EmulateDidDisallowFirstSubresourceMessage();
}

TEST_F(ContentSubresourceFilterDriverFactoryTest, WhitelistSiteOnReload) {
  const struct {
    content::Referrer referrer;
    ui::PageTransition transition;
    ActivationDecision expected_activation_decision;
  } kTestCases[] = {
      {content::Referrer(), ui::PAGE_TRANSITION_LINK,
       ActivationDecision::ACTIVATED},
      {content::Referrer(GURL(kUrlA), blink::WebReferrerPolicyDefault),
       ui::PAGE_TRANSITION_LINK, ActivationDecision::ACTIVATED},
      {content::Referrer(GURL(kExampleUrl), blink::WebReferrerPolicyDefault),
       ui::PAGE_TRANSITION_LINK, ActivationDecision::URL_WHITELISTED},
      {content::Referrer(), ui::PAGE_TRANSITION_RELOAD,
       ActivationDecision::URL_WHITELISTED}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message("referrer = \"")
                 << test_case.referrer.url << "\""
                 << " transition = \"" << test_case.transition << "\"");

    base::FieldTrialList field_trial_list(nullptr);
    testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationLevelEnabled,
        kActivationScopeAllSites, "" /* activation_lists */,
        "" /* performance_measurement_rate */, "" /* suppress_notifications */,
        "true" /* whitelist_site_on_reload */);

    NavigateAndExpectActivation(
        {false}, {GURL(kExampleUrl)},
        safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
        safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS,
        test_case.referrer, test_case.transition, EMPTY,
        test_case.expected_activation_decision);
    // Verify that if the first URL failed to activate, subsequent same-origin
    // navigations also fail to activate.
    NavigateAndExpectActivation({false}, {GURL(kExampleUrlWithParams)}, EMPTY,
                                test_case.expected_activation_decision);
  }
}

TEST_P(ContentSubresourceFilterDriverFactoryActivationLevelTest,
       ActivateForFrameState) {
  const ActivationLevelTestData& test_data = GetParam();
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, test_data.activation_level,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);

  const GURL url(kExampleUrlWithParams);
  NavigateAndExpectActivation({true}, {url}, NO_REDIRECTS_HIT,
                              test_data.expected_activation_decision);
  factory()->AddHostOfURLToWhitelistSet(url);
  NavigateAndExpectActivation(
      {true}, {GURL(kExampleUrlWithParams)}, NO_REDIRECTS_HIT,
      GetMaximumActivationLevel() == ActivationLevel::DISABLED
          ? ActivationDecision::ACTIVATION_DISABLED
          : ActivationDecision::URL_WHITELISTED);
}

TEST_P(ContentSubresourceFilterDriverFactoryThreatTypeTest,
       ActivateForTheListType) {
  // Sets up the experiment in a way that the activation decision depends on the
  // list for which the Safe Browsing hit has happened.
  const ActivationListTestData& test_data = GetParam();
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationLevelEnabled,
      kActivationScopeActivationList, test_data.activation_list);

  const GURL test_url("https://example.com/nonsoceng?q=engsocnon");
  std::vector<GURL> navigation_chain;

  const bool expected_activation =
      test_data.expected_activation_decision == ActivationDecision::ACTIVATED;
  NavigateAndExpectActivation(
      {false, false, false, true},
      {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC), test_url}, test_data.threat_type,
      test_data.threat_type_metadata, content::Referrer(),
      ui::PAGE_TRANSITION_LINK, expected_activation ? F0M0L1 : EMPTY,
      test_data.expected_activation_decision);
};

TEST_P(ContentSubresourceFilterDriverFactoryActivationScopeTest,
       ActivateForScopeType) {
  const ActivationScopeTestData& test_data = GetParam();
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationLevelEnabled,
      test_data.activation_scope,
      kActivationListSocialEngineeringAdsInterstitial);

  const GURL test_url(kExampleUrlWithParams);

  RedirectChainMatchPattern expected_pattern =
      test_data.url_matches_activation_list ? NO_REDIRECTS_HIT : EMPTY;
  NavigateAndExpectActivation({test_data.url_matches_activation_list},
                              {test_url}, expected_pattern,
                              test_data.expected_activation_decision);
  if (test_data.url_matches_activation_list) {
    factory()->AddHostOfURLToWhitelistSet(test_url);
    NavigateAndExpectActivation(
        {test_data.url_matches_activation_list}, {GURL(kExampleUrlWithParams)},
        expected_pattern,
        GetCurrentActivationScope() == ActivationScope::NO_SITES
            ? ActivationDecision::ACTIVATION_DISABLED
            : ActivationDecision::URL_WHITELISTED);
  }
};

// Only main frames with http/https schemes should activate, unless the
// activation scope is for all sites.
TEST_P(ContentSubresourceFilterDriverFactoryActivationScopeTest,
       ActivateForSupportedUrlScheme) {
  const ActivationScopeTestData& test_data = GetParam();
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationLevelEnabled,
      test_data.activation_scope,
      kActivationListSocialEngineeringAdsInterstitial);

  const char* unsupported_urls[] = {
      "data:text/html,<p>Hello", "ftp://example.com/", "chrome://settings",
      "chrome-extension://some-extension", "file:///var/www/index.html"};
  const char* supported_urls[] = {"http://example.test",
                                  "https://example.test"};
  for (auto* url : unsupported_urls) {
    SCOPED_TRACE(url);
    RedirectChainMatchPattern expected_pattern = EMPTY;
    NavigateAndExpectActivation(
        {test_data.url_matches_activation_list}, {GURL(url)}, expected_pattern,
        GetCurrentActivationScope() == ActivationScope::NO_SITES
            ? ActivationDecision::ACTIVATION_DISABLED
            : ActivationDecision::UNSUPPORTED_SCHEME);
  }
  for (auto* url : supported_urls) {
    SCOPED_TRACE(url);
    RedirectChainMatchPattern expected_pattern =
        test_data.url_matches_activation_list ? NO_REDIRECTS_HIT : EMPTY;
    NavigateAndExpectActivation({test_data.url_matches_activation_list},
                                {GURL(url)}, expected_pattern,
                                test_data.expected_activation_decision);
  }
};

INSTANTIATE_TEST_CASE_P(NoSocEngHit,
                        ContentSubresourceFilterDriverFactoryThreatTypeTest,
                        ::testing::ValuesIn(kActivationListTestData));

INSTANTIATE_TEST_CASE_P(
    ActivationScopeTest,
    ContentSubresourceFilterDriverFactoryActivationScopeTest,
    ::testing::ValuesIn(kActivationScopeTestData));

INSTANTIATE_TEST_CASE_P(
    ActivationLevelTest,
    ContentSubresourceFilterDriverFactoryActivationLevelTest,
    ::testing::ValuesIn(kActivationLevelTestData));

}  // namespace subresource_filter
