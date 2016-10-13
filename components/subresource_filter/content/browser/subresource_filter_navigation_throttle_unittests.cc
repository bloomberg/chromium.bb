// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_navigation_throttle.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "components/safe_browsing_db/util.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/core/browser/subresource_filter_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::NavigationThrottle;

namespace {

const char kExampleURL[] = "http://example.com";
const char kTestURL[] = "http://test.com";
const char kRedirectURLFirst[] = "http://example1.com";
const char kRedirectURLSecond[] = "http://example2.com";
const char kRedirectURLThird[] = "http://example3.com";
const char kNonWebURL[] = "chrome://settings";

}  //  namespace

namespace subresource_filter {

class MockSubresourceFilterDriver : public ContentSubresourceFilterDriver {
 public:
  explicit MockSubresourceFilterDriver(
      content::RenderFrameHost* render_frame_host)
      : ContentSubresourceFilterDriver(render_frame_host) {}

  ~MockSubresourceFilterDriver() override {}

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

class SubresourceFilterNavigationThrottleTest
    : public content::RenderViewHostTestHarness {
 public:
  SubresourceFilterNavigationThrottleTest() {}

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    ContentSubresourceFilterDriverFactory::CreateForWebContents(
        web_contents(), base::MakeUnique<MockSubresourceFilterClient>());

    driver_ = new MockSubresourceFilterDriver(main_rfh());
    factory()->SetDriverForFrameHostForTesting(main_rfh(),
                                               base::WrapUnique(driver_));
  }

  void TearDown() override {
    handle_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  void SetUpNavigationHandleForURL(const GURL& url) {
    handle_ = content::NavigationHandle::CreateNavigationHandleForTesting(
        url, main_rfh());
    handle_->RegisterThrottleForTesting(
        SubresourceFilterNavigationThrottle::Create(handle_.get()));
  }

  content::NavigationHandle* handle() { return handle_.get(); }

  ContentSubresourceFilterDriverFactory* factory() {
    return ContentSubresourceFilterDriverFactory::FromWebContents(
        web_contents());
  }

  MockSubresourceFilterDriver* driver() { return driver_; }

  NavigationThrottle::ThrottleCheckResult SimulateWillStart() {
    return handle()->CallWillStartRequestForTesting(
        false /* is_post */, content::Referrer(), false /* has_user_gesture */,
        ui::PAGE_TRANSITION_LINK, false /* is_external_protocol */);
  }

  NavigationThrottle::ThrottleCheckResult SimulateRedirects(
      const GURL& redirect) {
    return handle()->CallWillRedirectRequestForTesting(
        redirect, false /* new_method_is_post */, GURL() /* new_referrer_url */,
        false /* new_is_external_protocol */);
  }

  void SimulateWillProcessResponse() {
    handle()->CallWillProcessResponseForTesting(main_rfh(), std::string());
  }

 private:
  // Owned by the factory.
  MockSubresourceFilterDriver* driver_;
  std::unique_ptr<content::NavigationHandle> handle_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterNavigationThrottleTest);
};

TEST_F(SubresourceFilterNavigationThrottleTest, RequestWithoutRedirects) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);

  const GURL url(kExampleURL);
  SetUpNavigationHandleForURL(url);
  SimulateWillStart();
  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      url, std::vector<GURL>(), safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);
  EXPECT_CALL(*driver(), ActivateForProvisionalLoad(ActivationState::ENABLED))
      .Times(1);
  SimulateWillProcessResponse();
  ::testing::Mock::VerifyAndClearExpectations(driver());
}

TEST_F(SubresourceFilterNavigationThrottleTest,
       RequestWithoutRedirectsNoActivation) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);

  const GURL url_with_activation(kExampleURL);
  const GURL url_without_activation(kTestURL);

  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      url_with_activation, std::vector<GURL>(),
      safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);

  SetUpNavigationHandleForURL(url_without_activation);
  SimulateWillStart();

  EXPECT_CALL(*driver(), ActivateForProvisionalLoad(::testing::_)).Times(0);
  SimulateWillProcessResponse();
  ::testing::Mock::VerifyAndClearExpectations(driver());
}

TEST_F(SubresourceFilterNavigationThrottleTest,
       RequestToNonWebURLNoActivation) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);

  const GURL non_web_url(kNonWebURL);

  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      non_web_url, std::vector<GURL>(),
      safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);

  SetUpNavigationHandleForURL(non_web_url);
  SimulateWillStart();

  EXPECT_CALL(*driver(), ActivateForProvisionalLoad(::testing::_)).Times(0);
  SimulateWillProcessResponse();
  ::testing::Mock::VerifyAndClearExpectations(driver());
}

TEST_F(SubresourceFilterNavigationThrottleTest,
       AddRedirectFromNavThrottleToServiceEmptyInitRedirects) {
  // Navigations |url| -> |redirect|. Safe Browsing classifies the |url| as
  // containing deceptive content.
  // Test checks that both |url| and |redirect| are in the activation set.
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);

  const GURL url(kExampleURL);
  const GURL redirect(kRedirectURLFirst);

  SetUpNavigationHandleForURL(url);
  SimulateWillStart();
  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      redirect, std::vector<GURL>(), safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);
  SimulateRedirects(redirect);

  EXPECT_CALL(*driver(), ActivateForProvisionalLoad(ActivationState::ENABLED))
      .Times(1);
  SimulateWillProcessResponse();
  ::testing::Mock::VerifyAndClearExpectations(driver());
}

TEST_F(SubresourceFilterNavigationThrottleTest,
       AddRedirectFromNavThrottleToServiceNonEmptyInitRedirects) {
  // Navigations |redirects| -> |url| -> |redirect_after_sb_classification|.
  // Safe Browsing classifies the |url| as containing deceptive content.
  // Test checks that |url| it doesn't lead to sending the activation signal.
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);

  const GURL url(kExampleURL);
  const GURL redirect_after_sb_classification(kTestURL);
  const GURL first_redirect(kRedirectURLFirst);
  const GURL second_redirect(kRedirectURLSecond);
  const GURL third_redirect(kRedirectURLThird);

  SetUpNavigationHandleForURL(url);
  SimulateWillStart();

  std::vector<GURL> redirects = {first_redirect, second_redirect,
                                 third_redirect};
  SimulateRedirects(redirect_after_sb_classification);
  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      url, redirects, safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);

  EXPECT_CALL(*driver(), ActivateForProvisionalLoad(::testing::_)).Times(0);
  SimulateWillProcessResponse();
  ::testing::Mock::VerifyAndClearExpectations(driver());
}

TEST_F(SubresourceFilterNavigationThrottleTest,
       RequestRedirectWithMatchRedirectTest) {
  base::FieldTrialList field_trial_list(nullptr);
  testing::ScopedSubresourceFilterFeatureToggle scoped_feature_toggle(
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, kActivationStateEnabled,
      kActivationScopeActivationList,
      kActivationListSocialEngineeringAdsInterstitial);

  const GURL init_url(kExampleURL);
  const GURL redirect_with_match(kRedirectURLFirst);
  const GURL final_url(kRedirectURLSecond);
  std::vector<GURL> redirects = {redirect_with_match};
  SetUpNavigationHandleForURL(init_url);
  SimulateWillStart();
  SimulateRedirects(redirect_with_match);
  SimulateRedirects(final_url);
  factory()->OnMainResourceMatchedSafeBrowsingBlacklist(
      final_url, redirects, safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS);
  EXPECT_CALL(*driver(), ActivateForProvisionalLoad(ActivationState::ENABLED))
      .Times(1);
  SimulateWillProcessResponse();
  ::testing::Mock::VerifyAndClearExpectations(driver());
}

}  // namespace subresource_filter
