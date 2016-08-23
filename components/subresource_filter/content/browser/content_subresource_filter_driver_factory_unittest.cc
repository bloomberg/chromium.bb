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
        url, redirects,
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

  void NavigateToUrlAndExpectActivationAndPromptSubFrame(const GURL& url,
                                                         bool should_activate) {
    EXPECT_CALL(*subframe_driver(), ActivateForProvisionalLoad(::testing::_))
        .Times(should_activate);
    EXPECT_CALL(*client(), ToggleNotificationVisibility(::testing::_)).Times(0);

    factory()->DidStartProvisionalLoadForFrame(
        subframe_rfh(), url /* validated_url */, false /* is_error_page */,
        false /* is_iframe_srcdoc */);
    factory()->DidCommitProvisionalLoadForFrame(
        subframe_rfh(), url, ui::PageTransition::PAGE_TRANSITION_AUTO_SUBFRAME);
    ::testing::Mock::VerifyAndClearExpectations(driver());
    ::testing::Mock::VerifyAndClearExpectations(client());
  }

  void NavigateToUrlAndExpectActivationAndPrompt(
      const GURL& url,
      bool should_activate,
      ActivationState expected_activation_state) {
    EXPECT_CALL(*driver(), ActivateForProvisionalLoad(::testing::_))
        .Times(should_activate);
    EXPECT_CALL(*client(),
                ToggleNotificationVisibility(expected_activation_state ==
                                             ActivationState::ENABLED));
    content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
    EXPECT_EQ(expected_activation_state, factory()->activation_state());
    ::testing::Mock::VerifyAndClearExpectations(driver());
    ::testing::Mock::VerifyAndClearExpectations(client());

    NavigateToUrlAndExpectActivationAndPromptSubFrame(GURL(kExampleUrl),
                                                      should_activate);
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
      public ::testing::WithParamInterface<safe_browsing::ThreatPatternType> {
 public:
  ContentSubresourceFilterDriverFactoryThreatTypeTest() {}
  ~ContentSubresourceFilterDriverFactoryThreatTypeTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSubresourceFilterDriverFactoryThreatTypeTest);
};

TEST_F(ContentSubresourceFilterDriverFactoryTest, SocEngHitEmptyRedirects) {
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
      kActivationScopeNoSites);
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
      kActivationScopeActivationList);

  BlacklistURLWithRedirects(GURL(kExampleUrlWithParams), std::vector<GURL>());
  ActivateAndExpectForFrameHostForUrl(driver(), main_rfh(), GURL(kTestUrl),
                                      false /* should_activate */);
  ActivateAndExpectForFrameHostForUrl(driver(), main_rfh(), GURL(kExampleUrl),
                                      true /* should_activate */);
}

TEST_P(ContentSubresourceFilterDriverFactoryThreatTypeTest, NonSocEngHit) {
  std::vector<GURL> redirects;
  redirects.push_back(GURL("https://example1.com"));
  redirects.push_back(GURL("https://example2.com"));
  redirects.push_back(GURL("https://example3.com"));

  const GURL test_url("https://example.com/nonsoceng?q=engsocnon");
  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      GURL(test_url), std::vector<GURL>(), GetParam());
  EXPECT_EQ(0U, factory()->activation_set().size());
  EXPECT_FALSE(factory()->ShouldActivateForURL(GURL(test_url)));
  EXPECT_FALSE(
      factory()->ShouldActivateForURL(GURL(test_url.GetWithEmptyPath())));
  EXPECT_FALSE(factory()->ShouldActivateForURL(
      GURL("http://" + test_url.host() + "/path?q=q")));
  for (const auto& redirect : redirects) {
    EXPECT_FALSE(factory()->ShouldActivateForURL(redirect));
    EXPECT_FALSE(factory()->ShouldActivateForURL(redirect.GetWithEmptyPath()));
    EXPECT_FALSE(
        factory()->ShouldActivateForURL(GURL("http://" + redirect.host())));
    EXPECT_FALSE(factory()->ShouldActivateForURL(
        GURL("http://" + redirect.host() + "/path?q=q")));
  }
};

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       ActivationPromptNotShownForNonMainFrames) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList);
  NavigateToUrlAndExpectActivationAndPromptSubFrame(GURL(kExampleUrl),
                                                    false /* should_prompt */);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeAllSitesStateDryRun) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateDryRun,
      kActivationScopeAllSites);
  NavigateToUrlAndExpectActivationAndPrompt(GURL(kExampleUrlWithParams),
                                            true /* should_activate */,
                                            ActivationState::DRYRUN);
  factory()->AddHostOfURLToWhitelistSet(GURL(kExampleUrlWithParams));
  NavigateToUrlAndExpectActivationAndPrompt(GURL(kExampleUrlWithParams),
                                            false /* should_activate */,
                                            ActivationState::DISABLED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeAllSitesStateEnabled) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeAllSites);
  NavigateToUrlAndExpectActivationAndPrompt(GURL(kExampleUrlWithParams),
                                            true /* should_activate */,
                                            ActivationState::ENABLED);
  factory()->AddHostOfURLToWhitelistSet(GURL(kExampleUrlWithParams));
  NavigateToUrlAndExpectActivationAndPrompt(GURL(kExampleUrlWithParams),
                                            false /* should_activate */,
                                            ActivationState::DISABLED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeNoSites) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateDryRun,
      kActivationScopeNoSites);
  NavigateToUrlAndExpectActivationAndPrompt(GURL(kExampleUrlWithParams),
                                            false /* should_activate */,
                                            ActivationState::DISABLED);
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       DidStartProvisionalLoadScopeActivationList) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateDisabled,
      kActivationScopeActivationList);
  NavigateToUrlAndExpectActivationAndPrompt(GURL(kExampleUrlWithParams),
                                            false /* should_activate */,
                                            ActivationState::DISABLED);
}

INSTANTIATE_TEST_CASE_P(
    NoSonEngHit,
    ContentSubresourceFilterDriverFactoryThreatTypeTest,
    ::testing::Values(
        safe_browsing::ThreatPatternType::NONE,
        safe_browsing::ThreatPatternType::MALWARE_LANDING,
        safe_browsing::ThreatPatternType::MALWARE_DISTRIBUTION,
        safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_LANDING));

}  // namespace subresource_filter
