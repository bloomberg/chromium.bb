// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager.h"
#include "chrome/browser/subresource_filter/subresource_filter_test_harness.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_level.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class SubresourceFilterTest : public SubresourceFilterTestHarness {};

TEST_F(SubresourceFilterTest, SimpleAllowedLoad) {
  GURL url("https://example.test");
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));
  EXPECT_FALSE(GetClient()->did_show_ui_for_navigation());
}

TEST_F(SubresourceFilterTest, SimpleDisallowedLoad) {
  GURL url("https://example.test");
  ConfigureAsSubresourceFilterOnlyURL(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(main_rfh()));
  EXPECT_TRUE(GetClient()->did_show_ui_for_navigation());
}

TEST_F(SubresourceFilterTest, DeactivateUrl_ClearsSiteMetadata) {
  GURL url("https://a.test");
  ConfigureAsSubresourceFilterOnlyURL(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  EXPECT_NE(nullptr, GetSettingsManager()->GetSiteMetadata(url));

  RemoveURLFromBlacklist(url);

  // Navigate to |url| again and expect the site metadata to clear.
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  EXPECT_EQ(nullptr, GetSettingsManager()->GetSiteMetadata(url));
}

// If the underlying configuration changes and a site only activates to DRYRUN,
// we should clear the metadata.
TEST_F(SubresourceFilterTest, ActivationToDryRun_ClearsSiteMetadata) {
  GURL url("https://a.test");
  ConfigureAsSubresourceFilterOnlyURL(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  EXPECT_NE(nullptr, GetSettingsManager()->GetSiteMetadata(url));

  // If the site later activates as DRYRUN due to e.g. a configuration change,
  // it should also be removed from the metadata.
  scoped_configuration().ResetConfiguration(subresource_filter::Configuration(
      subresource_filter::ActivationLevel::DRYRUN,
      subresource_filter::ActivationScope::ACTIVATION_LIST,
      subresource_filter::ActivationList::SUBRESOURCE_FILTER));

  // Navigate to |url| again and expect the site metadata to clear.
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  EXPECT_EQ(nullptr, GetSettingsManager()->GetSiteMetadata(url));
}

TEST_F(SubresourceFilterTest, ExplicitWhitelisting_ShouldNotClearMetadata) {
  GURL url("https://a.test");
  ConfigureAsSubresourceFilterOnlyURL(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  // Simulate explicit whitelisting and reload.
  GetSettingsManager()->WhitelistSite(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));

  // Should not have cleared the metadata, since the site is still on the SB
  // blacklist.
  EXPECT_NE(nullptr, GetSettingsManager()->GetSiteMetadata(url));
}

TEST_F(SubresourceFilterTest,
       NavigationToBadSchemeUrlWithNoActivation_DoesNotReportBadScheme) {
  // Don't report UNSUPPORTED_SCHEME if the navigation has no matching
  // configuration.
  scoped_configuration().ResetConfiguration(subresource_filter::Configuration(
      subresource_filter::ActivationLevel::DISABLED,
      subresource_filter::ActivationScope::NO_SITES));

  GURL url("data:text/html,hello world");
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));
  auto* driver_factory = subresource_filter::
      ContentSubresourceFilterDriverFactory::FromWebContents(web_contents());
  EXPECT_EQ(
      subresource_filter::ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
      driver_factory->GetActivationDecisionForLastCommittedPageLoad());

  // Also don't report UNSUPPORTED_SCHEME if the navigation matches a
  // configuration with DISABLED activation level.
  scoped_configuration().ResetConfiguration(subresource_filter::Configuration(
      subresource_filter::ActivationLevel::DISABLED,
      subresource_filter::ActivationScope::ALL_SITES));

  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));
  EXPECT_EQ(subresource_filter::ActivationDecision::ACTIVATION_DISABLED,
            driver_factory->GetActivationDecisionForLastCommittedPageLoad());

  // Sanity check that UNSUPPORTED_SCHEME is reported if the navigation does
  // activate.
  scoped_configuration().ResetConfiguration(subresource_filter::Configuration(
      subresource_filter::ActivationLevel::ENABLED,
      subresource_filter::ActivationScope::ALL_SITES));
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));
  EXPECT_EQ(subresource_filter::ActivationDecision::UNSUPPORTED_SCHEME,
            driver_factory->GetActivationDecisionForLastCommittedPageLoad());
}

TEST_F(SubresourceFilterTest, SimpleDisallowedLoad_WithObserver) {
  GURL url("https://example.test");
  ConfigureAsSubresourceFilterOnlyURL(url);

  subresource_filter::TestSubresourceFilterObserver observer(web_contents());
  SimulateNavigateAndCommit(url, main_rfh());

  EXPECT_EQ(subresource_filter::ActivationDecision::ACTIVATED,
            observer.GetPageActivation(url).value());

  GURL disallowed_url(SubresourceFilterTest::kDefaultDisallowedUrl);
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(main_rfh()));
  auto optional_load_policy = observer.GetSubframeLoadPolicy(disallowed_url);
  EXPECT_TRUE(optional_load_policy.has_value());
  EXPECT_EQ(subresource_filter::LoadPolicy::DISALLOW,
            observer.GetSubframeLoadPolicy(disallowed_url).value());
}

TEST_F(SubresourceFilterTest, DisableRuleset_SubframeAllowed) {
  subresource_filter::Configuration config(
      subresource_filter::ActivationLevel::ENABLED,
      subresource_filter::ActivationScope::ALL_SITES);
  config.activation_options.should_disable_ruleset_rules = true;
  scoped_configuration().ResetConfiguration(std::move(config));

  GURL url("https://a.test");
  ConfigureAsSubresourceFilterOnlyURL(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));
}
