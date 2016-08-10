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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    client_ = new MockSubresourceFilterClient();
    ContentSubresourceFilterDriverFactory::CreateForWebContents(
        web_contents(), base::WrapUnique(client()));
    driver_ = new MockSubresourceFilterDriver(web_contents()->GetMainFrame());
    factory()->SetDriverForFrameHostForTesting(main_rfh(),
                                               base::WrapUnique(driver()));
  }

  void SendActivationStateAndPromptUser() {
    factory()->SendActivationStateAndPromptUser(main_rfh());
  }

  ContentSubresourceFilterDriverFactory* factory() {
    return ContentSubresourceFilterDriverFactory::FromWebContents(
        web_contents());
  }

  MockSubresourceFilterClient* client() { return client_; }
  MockSubresourceFilterDriver* driver() { return driver_; }

 private:
  // Owned by the factory.
  MockSubresourceFilterClient* client_;
  MockSubresourceFilterDriver* driver_;

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
  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      GURL("https://example.com/soceng?q=engsoc"), std::vector<GURL>(),
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);
  EXPECT_EQ(1U, factory()->activation_set().size());

  EXPECT_TRUE(factory()->ShouldActivateForURL(GURL("https://example.com")));
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
  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      GURL("https://example.com/engsoc/q=soceng"), redirects,
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);
  EXPECT_EQ(4U, factory()->activation_set().size());
  EXPECT_TRUE(factory()->ShouldActivateForURL(GURL("https://example.com")));

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
  EXPECT_CALL(*driver(), ActivateForProvisionalLoad(::testing::_)).Times(0);
  EXPECT_CALL(*client(), ToggleNotificationVisibility(false)).Times(1);
  factory()->ActivateForFrameHostIfNeeded(main_rfh(), GURL("https://test.com"));
  ::testing::Mock::VerifyAndClearExpectations(driver());
  ::testing::Mock::VerifyAndClearExpectations(client());

  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      GURL("https://example.com/soceng?q=engsoc"), std::vector<GURL>(),
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);

  EXPECT_CALL(*driver(), ActivateForProvisionalLoad(::testing::_)).Times(0);
  EXPECT_CALL(*client(), ToggleNotificationVisibility(false)).Times(1);
  factory()->ActivateForFrameHostIfNeeded(main_rfh(),
                                          GURL("https://example.com"));
  ::testing::Mock::VerifyAndClearExpectations(driver());
  ::testing::Mock::VerifyAndClearExpectations(client());
}

TEST_F(ContentSubresourceFilterDriverFactoryTest, ActivateForFrameHostNeeded) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList);

  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      GURL("https://example.com/soceng?q=engsoc"), std::vector<GURL>(),
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);

  EXPECT_CALL(*driver(), ActivateForProvisionalLoad(::testing::_)).Times(0);
  EXPECT_CALL(*client(), ToggleNotificationVisibility(false)).Times(1);
  factory()->ActivateForFrameHostIfNeeded(main_rfh(), GURL("https://test.com"));
  ::testing::Mock::VerifyAndClearExpectations(driver());

  EXPECT_CALL(*driver(), ActivateForProvisionalLoad(::testing::_)).Times(1);
  EXPECT_CALL(*client(), ToggleNotificationVisibility(true)).Times(1);
  factory()->ActivateForFrameHostIfNeeded(main_rfh(),
                                          GURL("https://example.com"));
  ::testing::Mock::VerifyAndClearExpectations(driver());
  ::testing::Mock::VerifyAndClearExpectations(client());
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
       ActivationPromptNotShownWhenExperimentDisabled) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_DISABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList);
  EXPECT_CALL(*client(), ToggleNotificationVisibility(::testing::_)).Times(0);
  SendActivationStateAndPromptUser();
  ::testing::Mock::VerifyAndClearExpectations(client());
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       ActivationPromptNotShownWhenExperimentDryRun) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateDryRun,
      kActivationScopeActivationList);
  EXPECT_CALL(*client(), ToggleNotificationVisibility(::testing::_)).Times(0);
  SendActivationStateAndPromptUser();
  ::testing::Mock::VerifyAndClearExpectations(client());
}

TEST_F(ContentSubresourceFilterDriverFactoryTest,
       ActivationPromptShownWhenExperimentEnabled) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList);
  EXPECT_CALL(*client(), ToggleNotificationVisibility(true)).Times(1);
  SendActivationStateAndPromptUser();
  ::testing::Mock::VerifyAndClearExpectations(client());
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
