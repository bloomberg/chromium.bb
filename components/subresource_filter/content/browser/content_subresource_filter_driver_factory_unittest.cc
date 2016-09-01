// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "components/safe_browsing_db/util.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/core/browser/subresource_filter_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kExampleUrlWithParams[] = "https://example.com/soceng?q=engsoc";
const char kTestUrl[] = "https://test.com";
const char kExampleUrl[] = "https://example.com";
const char kExampleLoginUrl[] = "https://example.com/login";

struct ActivationListTestData {
  bool should_add;
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
    {true, subresource_filter::kActivationListSocialEngineeringAdsInterstitial,
     safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
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
};

}  // namespace

namespace subresource_filter {

class MockSubresourceFilterDriver : public ContentSubresourceFilterDriver {
 public:
  explicit MockSubresourceFilterDriver(
      content::RenderFrameHost* render_frame_host)
      : ContentSubresourceFilterDriver(render_frame_host) {}

  ~MockSubresourceFilterDriver() override = default;

  MOCK_METHOD1(ActivateForProvisionalLoad, void(ActivationState));

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

  void BlacklistURLWithRedirects(const GURL& url,
                                 const std::vector<GURL>& redirects) {
    factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
        url, redirects, safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
        safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);
  }

  void ActivateAndExpectForFrameHostForUrl(MockSubresourceFilterDriver* driver,
                                           content::RenderFrameHost* rfh,
                                           const GURL& url,
                                           bool should_activate) {
    EXPECT_CALL(*driver, ActivateForProvisionalLoad(::testing::_))
        .Times(should_activate);
    factory()->ReadyToCommitMainFrameNavigation(rfh, url);
    ::testing::Mock::VerifyAndClearExpectations(driver);
  }

  void NavigateToUrlAndExpectActivationAndHidingPromptSubFrame(
      const GURL& url,
      bool should_activate) {
    EXPECT_CALL(*subframe_driver(), ActivateForProvisionalLoad(::testing::_))
        .Times(should_activate);
    EXPECT_CALL(*client(), ToggleNotificationVisibility(::testing::_)).Times(0);

    factory()->DidStartProvisionalLoadForFrame(
        subframe_rfh(), url /* validated_url */, false /* is_error_page */,
        false /* is_iframe_srcdoc */);
    factory()->DidCommitProvisionalLoadForFrame(
        subframe_rfh(), url, ui::PageTransition::PAGE_TRANSITION_AUTO_SUBFRAME);
    ::testing::Mock::VerifyAndClearExpectations(subframe_driver());
    ::testing::Mock::VerifyAndClearExpectations(client());
  }

  void NavigateToUrlAndExpectActivationAndHidingPrompt(
      const GURL& url,
      bool should_activate,
      ActivationState expected_activation_state) {
    EXPECT_CALL(*driver(), ActivateForProvisionalLoad(::testing::_))
        .Times(should_activate);
    EXPECT_CALL(*client(), ToggleNotificationVisibility(false)).Times(1);
    content::WebContentsTester::For(web_contents())->StartNavigation(url);
    ::testing::Mock::VerifyAndClearExpectations(client());
    factory()->ReadyToCommitMainFrameNavigation(main_rfh(), url);
    content::RenderFrameHostTester::For(main_rfh())
        ->SimulateNavigationCommit(url);

    ::testing::Mock::VerifyAndClearExpectations(driver());

    NavigateToUrlAndExpectActivationAndHidingPromptSubFrame(
        GURL(kExampleLoginUrl), should_activate);
  }

  void SpecialCaseNavSeq(bool should_activate,
                         ActivationState state) {
    // This test-case examinse the nevigation woth following sequence of event:
    //   DidStartProvisional(main, "example.com")
    //   ReadyToCommitMainFrameNavigation(“example.com”)
    //   DidCommitProvisional(main, "example.com")
    //   DidStartProvisional(sub, "example.com/login")
    //   DidCommitProvisional(sub, "example.com/login")
    //   DidCommitProvisional(main, "example.com#ref")

    NavigateToUrlAndExpectActivationAndHidingPrompt(GURL(kExampleUrl),
                                                    should_activate, state);
    EXPECT_CALL(*driver(), ActivateForProvisionalLoad(::testing::_)).Times(0);
    EXPECT_CALL(*client(), ToggleNotificationVisibility(::testing::_)).Times(0);
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

  void VerifyEntitiesNotInTheBlacklist(
      const GURL& test_url,
      const std::vector<GURL>& redirects,
      const ActivationListTestData& test_data) {
    factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
        test_url, std::vector<GURL>(), test_data.threat_type,
        test_data.threat_type_metadata);
    EXPECT_EQ(test_data.should_add ? 1 : 0U,
              factory()->activation_set().size());
    EXPECT_EQ(test_data.should_add,
              factory()->ShouldActivateForURL(GURL(test_url)));
    EXPECT_EQ(test_data.should_add, factory()->ShouldActivateForURL(
                                        GURL(test_url.GetWithEmptyPath())));
    EXPECT_EQ(test_data.should_add,
              factory()->ShouldActivateForURL(
                  GURL("http://" + test_url.host() + "/path?q=q")));
    factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
        test_url, redirects, test_data.threat_type,
        test_data.threat_type_metadata);
    for (const auto& redirect : redirects) {
      EXPECT_EQ(test_data.should_add,
                factory()->ShouldActivateForURL(redirect));
      EXPECT_EQ(test_data.should_add,
                factory()->ShouldActivateForURL(redirect.GetWithEmptyPath()));
      EXPECT_EQ(test_data.should_add, factory()->ShouldActivateForURL(
                                          GURL("http://" + redirect.host())));
      EXPECT_EQ(test_data.should_add,
                factory()->ShouldActivateForURL(
                    GURL("http://" + redirect.host() + "/path?q=q")));
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriverFactoryThreatTypeTest);
};

TEST_F(ContentSubresourceFilterDriverFactoryTest, SocEngHitEmptyRedirects) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeNoSites, kActivationListSocialEngineeringAdsInterstitial);

  BlacklistURLWithRedirects(GURL(kExampleUrlWithParams), std::vector<GURL>());
  EXPECT_EQ(1U, factory()->activation_set().size());

  EXPECT_TRUE(factory()->ShouldActivateForURL(GURL(kExampleUrl)));
  EXPECT_TRUE(factory()->ShouldActivateForURL(GURL("http://example.com")));
  EXPECT_TRUE(
      factory()->ShouldActivateForURL(GURL("http://example.com/42?q=42!")));
  EXPECT_TRUE(
      factory()->ShouldActivateForURL(GURL("https://example.com/42?q=42!")));
  EXPECT_TRUE(
      factory()->ShouldActivateForURL(GURL("http://example.com/awesomepath")));
  const GURL whitelisted("http://example.com/page?q=whitelisted");
  EXPECT_TRUE(factory()->ShouldActivateForURL(whitelisted));
  factory()->AddHostOfURLToWhitelistSet(whitelisted);
  EXPECT_FALSE(factory()->ShouldActivateForURL(whitelisted));
}

TEST_F(ContentSubresourceFilterDriverFactoryTest, SocEngHitWithRedirects) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeNoSites, kActivationListSocialEngineeringAdsInterstitial);

  std::vector<GURL> redirects;
  redirects.push_back(GURL("https://example1.com"));
  redirects.push_back(GURL("https://example2.com"));
  redirects.push_back(GURL("https://example3.com"));
  BlacklistURLWithRedirects(GURL(kExampleUrlWithParams), redirects);
  EXPECT_EQ(4U, factory()->activation_set().size());
  EXPECT_TRUE(factory()->ShouldActivateForURL(GURL(kExampleUrl)));

  for (const auto& redirect : redirects) {
    EXPECT_TRUE(factory()->ShouldActivateForURL(redirect));
    EXPECT_TRUE(factory()->ShouldActivateForURL(redirect.GetWithEmptyPath()));
    EXPECT_TRUE(
        factory()->ShouldActivateForURL(GURL("http://" + redirect.host())));
    EXPECT_TRUE(factory()->ShouldActivateForURL(
        GURL("http://" + redirect.host() + "/path?q=q")));
  }
  const GURL whitelisted("http://example.com/page?q=42");
  EXPECT_TRUE(factory()->ShouldActivateForURL(whitelisted));
  factory()->AddHostOfURLToWhitelistSet(whitelisted);
  EXPECT_FALSE(factory()->ShouldActivateForURL(whitelisted));
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       ActivateForFrameHostNotNeeded) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_DISABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeNoSites, kActivationListSocialEngineeringAdsInterstitial);
  ActivateAndExpectForFrameHostForUrl(driver(), main_rfh(), GURL(kTestUrl),
                                      false /* should_activate */);
  BlacklistURLWithRedirects(GURL(kExampleUrlWithParams), std::vector<GURL>());
  ActivateAndExpectForFrameHostForUrl(driver(), main_rfh(),
                                      GURL("https://not-example.com"),
                                      false /* should_activate */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest, ActivateForFrameHostNeeded) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);

  BlacklistURLWithRedirects(GURL(kExampleUrlWithParams), std::vector<GURL>());
  ActivateAndExpectForFrameHostForUrl(driver(), main_rfh(), GURL(kTestUrl),
                                      false /* should_activate */);
  ActivateAndExpectForFrameHostForUrl(driver(), main_rfh(), GURL(kExampleUrl),
                                      true /* should_activate */);
}

TEST_P(ContentSubresourceFilterDriverFactoryThreatTypeTest, NonSocEngHit) {
  const ActivationListTestData& test_data = GetParam();
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeNoSites, test_data.activation_list);

  std::vector<GURL> redirects;
  redirects.push_back(GURL("https://example1.com"));
  redirects.push_back(GURL("https://example2.com"));
  redirects.push_back(GURL("https://example3.com"));

  const GURL test_url("https://example.com/nonsoceng?q=engsocnon");
  VerifyEntitiesNotInTheBlacklist(test_url, redirects, test_data);
};

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       ActivationPromptNotShownForNonMainFrames) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  NavigateToUrlAndExpectActivationAndHidingPromptSubFrame(
      GURL(kExampleUrl), false /* should_prompt */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeAllSitesStateDryRun) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateDryRun,
      kActivationScopeAllSites,
      kActivationListSocialEngineeringAdsInterstitial);
  NavigateToUrlAndExpectActivationAndHidingPrompt(GURL(kExampleUrlWithParams),
                                                  true /* should_activate */,
                                                  ActivationState::DRYRUN);
  factory()->AddHostOfURLToWhitelistSet(GURL(kExampleUrlWithParams));
  NavigateToUrlAndExpectActivationAndHidingPrompt(GURL(kExampleUrlWithParams),
                                                  false /* should_activate */,
                                                  ActivationState::DISABLED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeAllSitesStateEnabled) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeAllSites,
      kActivationListSocialEngineeringAdsInterstitial);
  NavigateToUrlAndExpectActivationAndHidingPrompt(GURL(kExampleUrlWithParams),
                                                  true /* should_activate */,
                                                  ActivationState::ENABLED);
  factory()->AddHostOfURLToWhitelistSet(GURL(kExampleUrlWithParams));
  NavigateToUrlAndExpectActivationAndHidingPrompt(GURL(kExampleUrlWithParams),
                                                  false /* should_activate */,
                                                  ActivationState::DISABLED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeActivationListSitesStateEnabled) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  BlacklistURLWithRedirects(GURL(kExampleUrlWithParams), std::vector<GURL>());
  NavigateToUrlAndExpectActivationAndHidingPrompt(GURL(kExampleUrlWithParams),
                                                  true /* should_activate */,
                                                  ActivationState::ENABLED);
  factory()->AddHostOfURLToWhitelistSet(GURL(kExampleUrlWithParams));
  NavigateToUrlAndExpectActivationAndHidingPrompt(GURL(kExampleUrlWithParams),
                                                  false /* should_activate */,
                                                  ActivationState::DISABLED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeActivationListSitesStateDryRun) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateDryRun,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  BlacklistURLWithRedirects(GURL(kExampleUrlWithParams), std::vector<GURL>());
  NavigateToUrlAndExpectActivationAndHidingPrompt(GURL(kExampleUrlWithParams),
                                                  true /* should_activate */,
                                                  ActivationState::DRYRUN);
  factory()->AddHostOfURLToWhitelistSet(GURL(kExampleUrlWithParams));
  NavigateToUrlAndExpectActivationAndHidingPrompt(GURL(kExampleUrlWithParams),
                                                  false /* should_activate */,
                                                  ActivationState::DISABLED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeNoSites) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateDryRun,
      kActivationScopeNoSites, kActivationListSocialEngineeringAdsInterstitial);
  NavigateToUrlAndExpectActivationAndHidingPrompt(GURL(kExampleUrlWithParams),
                                                  false /* should_activate */,
                                                  ActivationState::DISABLED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeActivationList) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateDisabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  NavigateToUrlAndExpectActivationAndHidingPrompt(GURL(kExampleUrlWithParams),
                                                  false /* should_activate */,
                                                  ActivationState::DISABLED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       SpecialCaseNavigationAllSitesDryRun) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateDryRun,
      kActivationScopeAllSites);
  SpecialCaseNavSeq(true /* should_activate */, ActivationState::DRYRUN);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       SpecialCaseNavigationAllSitesEnabled) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeAllSites);
  SpecialCaseNavSeq(true /* should_activate */, ActivationState::ENABLED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       SpecialCaseNavigationActivationListEnabled) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);
  BlacklistURLWithRedirects(GURL(kExampleUrlWithParams), std::vector<GURL>());
  SpecialCaseNavSeq(true /* should_activate */, ActivationState::ENABLED);
}

INSTANTIATE_TEST_CASE_P(NoSonEngHit,
                        ContentSubresourceFilterDriverFactoryThreatTypeTest,
                        ::testing::ValuesIn(kActivationListTestData));

}  // namespace subresource_filter
