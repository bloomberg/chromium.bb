// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/test/histogram_tester.h"
#include "components/safe_browsing_db/util.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/core/browser/subresource_filter_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

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
  bool expected_activation;
  const char* const activation_list;
  safe_browsing::SBThreatType threat_type;
  safe_browsing::ThreatPatternType threat_type_metadata;
};

const ActivationListTestData kActivationListTestData[] = {
    {false, "", safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListSocialEngineeringAdsInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::NONE},
    {false, subresource_filter::kActivationListSocialEngineeringAdsInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::MALWARE_LANDING},
    {false, subresource_filter::kActivationListSocialEngineeringAdsInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::MALWARE_DISTRIBUTION},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_API_ABUSE,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_BLACKLISTED_RESOURCE,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_BINARY_MALWARE_URL,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_UNWANTED,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {false, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_SAFE,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
    {true, subresource_filter::kActivationListPhishingInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::NONE},
    {true, subresource_filter::kActivationListSocialEngineeringAdsInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
};

struct ActivationScopeTestData {
  bool expected_activation;
  bool url_matches_activation_list;
  const char* const activation_scope;
};

const ActivationScopeTestData kActivationScopeTestData[] = {
    {true /* expected_activation */, false /* url_matches_activation_list */,
     kActivationScopeAllSites},
    {true /* expected_activation */, true /* url_matches_activation_list */,
     kActivationScopeAllSites},
    {false /* expected_activation */, true /* url_matches_activation_list */,
     kActivationScopeNoSites},
    {true /* expected_activation */, true /* url_matches_activation_list */,
     kActivationScopeActivationList},
    {false /* expected_activation */, false /* url_matches_activation_list */,
     kActivationScopeActivationList},
};

struct ActivationStateTestData {
  bool expected_activation;
  const char* const activation_state;
};

const ActivationStateTestData kActivationStateTestData[] = {
    {true /* expected_activation */, kActivationStateDryRun},
    {true /* expected_activation */, kActivationStateEnabled},
    {false /* expected_activation */, kActivationStateDisabled},
};

class MockSubresourceFilterDriver : public ContentSubresourceFilterDriver {
 public:
  explicit MockSubresourceFilterDriver(
      content::RenderFrameHost* render_frame_host)
      : ContentSubresourceFilterDriver(render_frame_host) {}

  ~MockSubresourceFilterDriver() override = default;

  MOCK_METHOD2(ActivateForProvisionalLoad, void(ActivationState, const GURL&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSubresourceFilterDriver);
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
    driver_ = new MockSubresourceFilterDriver(main_rfh());
    SetDriverForFrameHostForTesting(main_rfh(), driver());
    // Add a subframe.
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());
    rfh_tester->InitializeRenderFrameIfNeeded();
    subframe_rfh_ = rfh_tester->AppendChild("Child");
    subframe_driver_ = new MockSubresourceFilterDriver(subframe_rfh());
    SetDriverForFrameHostForTesting(subframe_rfh(), subframe_driver());
  }

  void SetDriverForFrameHostForTesting(
      content::RenderFrameHost* render_frame_host,
      ContentSubresourceFilterDriver* driver) {
    factory()->SetDriverForFrameHostForTesting(render_frame_host,
                                               base::WrapUnique(driver));
  }

  ContentSubresourceFilterDriverFactory* factory() {
    return ContentSubresourceFilterDriverFactory::FromWebContents(
        web_contents());
  }

  MockSubresourceFilterClient* client() { return client_; }
  MockSubresourceFilterDriver* driver() { return driver_; }

  MockSubresourceFilterDriver* subframe_driver() { return subframe_driver_; }
  content::RenderFrameHost* subframe_rfh() { return subframe_rfh_; }

  void BlacklistURLWithRedirectsNavigateAndCommit(
      const std::vector<bool>& blacklisted_urls,
      const std::vector<GURL>& navigation_chain,
      safe_browsing::SBThreatType threat_type,
      safe_browsing::ThreatPatternType threat_type_metadata,
      RedirectChainMatchPattern extected_pattern,
      bool expected_activation) {
    base::HistogramTester tester;
    EXPECT_CALL(*client(), ToggleNotificationVisibility(false)).Times(1);
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());

    rfh_tester->SimulateNavigationStart(navigation_chain.front());
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
      rfh_tester->SimulateRedirect(url);
    }
    EXPECT_CALL(*driver(),
                ActivateForProvisionalLoad(::testing::_, ::testing::_))
        .Times(expected_activation);
    if (!content::IsBrowserSideNavigationEnabled()) {
      factory()->ReadyToCommitNavigationInternal(main_rfh(),
                                                 navigation_chain.back());
    }

    rfh_tester->SimulateNavigationCommit(navigation_chain.back());
    ::testing::Mock::VerifyAndClearExpectations(driver());

    if (extected_pattern != EMPTY) {
      EXPECT_THAT(tester.GetAllSamples(kMatchesPatternHistogramName),
                  ::testing::ElementsAre(base::Bucket(extected_pattern, 1)));
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
    EXPECT_CALL(*subframe_driver(),
                ActivateForProvisionalLoad(::testing::_, ::testing::_))
        .Times(expected_activation);
    EXPECT_CALL(*client(), ToggleNotificationVisibility(::testing::_)).Times(0);

    factory()->ReadyToCommitNavigationInternal(subframe_rfh(), url);
    ::testing::Mock::VerifyAndClearExpectations(subframe_driver());
    ::testing::Mock::VerifyAndClearExpectations(client());
  }

  void NavigateAndExpectActivation(
      const std::vector<bool>& blacklisted_urls,
      const std::vector<GURL>& navigation_chain,
      safe_browsing::SBThreatType threat_type,
      safe_browsing::ThreatPatternType threat_type_metadata,
      RedirectChainMatchPattern extected_pattern,
      bool expected_activation) {
    BlacklistURLWithRedirectsNavigateAndCommit(
        blacklisted_urls, navigation_chain, threat_type, threat_type_metadata,
        extected_pattern, expected_activation);

    NavigateAndCommitSubframe(GURL(kExampleLoginUrl), expected_activation);
  }

  void NavigateAndExpectActivation(const std::vector<bool>& blacklisted_urls,
                                   const std::vector<GURL>& navigation_chain,
                                   RedirectChainMatchPattern extected_pattern,
                                   bool expected_activation) {
    NavigateAndExpectActivation(
        blacklisted_urls, navigation_chain,
        safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
        safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS,
        extected_pattern, expected_activation);
  }

  void EmulateInPageNavigation(const std::vector<bool>& blacklisted_urls,
                               RedirectChainMatchPattern extected_pattern,
                               bool expected_activation) {
    // This test-case examinse the nevigation woth following sequence of event:
    //   DidStartProvisional(main, "example.com")
    //   ReadyToCommitNavigation(“example.com”)
    //   DidCommitProvisional(main, "example.com")
    //   DidStartProvisional(sub, "example.com/login")
    //   DidCommitProvisional(sub, "example.com/login")
    //   DidCommitProvisional(main, "example.com#ref")

    NavigateAndExpectActivation(blacklisted_urls, {GURL(kExampleUrl)},
                                extected_pattern, expected_activation);
    EXPECT_CALL(*driver(),
                ActivateForProvisionalLoad(::testing::_, ::testing::_))
        .Times(0);
    EXPECT_CALL(*client(), ToggleNotificationVisibility(::testing::_)).Times(1);
    content::RenderFrameHostTester::For(main_rfh())
        ->SimulateNavigationCommit(GURL(kExampleUrl));
    ::testing::Mock::VerifyAndClearExpectations(driver());
    ::testing::Mock::VerifyAndClearExpectations(client());
  }

 private:
  // Owned by the factory.
  MockSubresourceFilterClient* client_;
  MockSubresourceFilterDriver* driver_;

  content::RenderFrameHost* subframe_rfh_;
  MockSubresourceFilterDriver* subframe_driver_;

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

class ContentSubresourceFilterDriverFactoryActivationStateTest
    : public ContentSubresourceFilterDriverFactoryTest,
      public ::testing::WithParamInterface<ActivationStateTestData> {
 public:
  ContentSubresourceFilterDriverFactoryActivationStateTest() {}
  ~ContentSubresourceFilterDriverFactoryActivationStateTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      ContentSubresourceFilterDriverFactoryActivationStateTest);
};

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       ActivateForFrameHostDisabledFeature) {
  // Activation scope is set to NONE => no activation should happen even if URL
  // which is visited was a SB hit.
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_DISABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeAllSites,
      kActivationListSocialEngineeringAdsInterstitial);
  const GURL url(kExampleUrlWithParams);
  NavigateAndExpectActivation({true}, {url}, EMPTY,
                              false /* expected_activation */);
  factory()->AddHostOfURLToWhitelistSet(url);
  NavigateAndExpectActivation({true}, {url}, EMPTY,
                              false /* expected_activation */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest, NoActivationWhenNoMatch) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  NavigateAndExpectActivation({false}, {GURL(kExampleUrl)}, EMPTY,
                              false /* should_prompt */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       SpecialCaseNavigationAllSitesEnabled) {
  // Check that when the experiment is enabled for all site, the activation
  // signal is always sent.
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeAllSites);
  EmulateInPageNavigation({false}, EMPTY, true /* expected_activation */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       SpecialCaseNavigationActivationListEnabled) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  EmulateInPageNavigation({true}, NO_REDIRECTS_HIT,
                          true /* expected_activation */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest, RedirectPatternTest) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  struct RedirectRedirectChainMatchPatternTestData {
    std::vector<bool> blacklisted_urls;
    std::vector<GURL> navigation_chain;
    RedirectChainMatchPattern hit_extected_pattern;
    bool expected_activation;
  } kRedirectRedirectChainMatchPatternTestData[] = {
      {{false}, {GURL(kUrlA)}, EMPTY, false},
      {{true}, {GURL(kUrlA)}, NO_REDIRECTS_HIT, true},
      {{false, false}, {GURL(kUrlA), GURL(kUrlB)}, EMPTY, false},
      {{false, true}, {GURL(kUrlA), GURL(kUrlB)}, F0M0L1, true},
      {{true, false}, {GURL(kUrlA), GURL(kUrlB)}, F1M0L0, false},
      {{true, true}, {GURL(kUrlA), GURL(kUrlB)}, F1M0L1, true},

      {{false, false, false},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       EMPTY,
       false},
      {{false, false, true},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       F0M0L1,
       true},
      {{false, true, false},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       F0M1L0,
       false},
      {{false, true, true},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       F0M1L1,
       true},
      {{true, false, false},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       F1M0L0,
       false},
      {{true, false, true},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       F1M0L1,
       true},
      {{true, true, false},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       F1M1L0,
       false},
      {{true, true, true},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC)},
       F1M1L1,
       true},
      {{false, true, false, false},
       {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC), GURL(kUrlD)},
       F0M1L0,
       false},
  };

  for (size_t i = 0U; i < arraysize(kRedirectRedirectChainMatchPatternTestData);
       ++i) {
    auto test_data = kRedirectRedirectChainMatchPatternTestData[i];
    NavigateAndExpectActivation(
        test_data.blacklisted_urls, test_data.navigation_chain,
        safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
        safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS,
        test_data.hit_extected_pattern, test_data.expected_activation);
    NavigateAndExpectActivation({false}, {GURL("https://dummy.com")}, EMPTY,
                                false);
  }
}

TEST_P(ContentSubresourceFilterDriverFactoryActivationStateTest,
       ActivateForFrameState) {
  const ActivationStateTestData& test_data = GetParam();
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, test_data.activation_state,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);

  const GURL url(kExampleUrlWithParams);
  NavigateAndExpectActivation({true}, {url}, NO_REDIRECTS_HIT,
                              test_data.expected_activation);
  factory()->AddHostOfURLToWhitelistSet(url);
  NavigateAndExpectActivation({true}, {GURL(kExampleUrlWithParams)},
                              NO_REDIRECTS_HIT,
                              false /* expected_activation */);
}

TEST_P(ContentSubresourceFilterDriverFactoryThreatTypeTest,
       ActivateForTheListType) {
  // Sets up the experiment in a way that the activation decision depends on the
  // list for which the Safe Browsing hit has happened.
  const ActivationListTestData& test_data = GetParam();
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList, test_data.activation_list);

  const GURL test_url("https://example.com/nonsoceng?q=engsocnon");
  std::vector<GURL> navigation_chain;

  NavigateAndExpectActivation({false, false, false, true},
                              {GURL(kUrlA), GURL(kUrlB), GURL(kUrlC), test_url},
                              test_data.threat_type,
                              test_data.threat_type_metadata,
                              test_data.expected_activation ? F0M0L1 : EMPTY,
                              test_data.expected_activation);
};

TEST_P(ContentSubresourceFilterDriverFactoryActivationScopeTest,
       ActivateForScopeType) {
  const ActivationScopeTestData& test_data = GetParam();
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      test_data.activation_scope,
      kActivationListSocialEngineeringAdsInterstitial);

  const GURL test_url(kExampleUrlWithParams);

  RedirectChainMatchPattern extected_pattern =
      test_data.url_matches_activation_list ? NO_REDIRECTS_HIT : EMPTY;
  NavigateAndExpectActivation({test_data.url_matches_activation_list},
                              {test_url}, extected_pattern,
                              test_data.expected_activation);
  if (test_data.url_matches_activation_list) {
    factory()->AddHostOfURLToWhitelistSet(test_url);
    NavigateAndExpectActivation({test_data.url_matches_activation_list},
                                {GURL(kExampleUrlWithParams)}, extected_pattern,
                                false /* expected_activation */);
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
    ActivationStateTest,
    ContentSubresourceFilterDriverFactoryActivationStateTest,
    ::testing::ValuesIn(kActivationStateTestData));

}  // namespace subresource_filter
