// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_saver/data_saver_top_host_provider.h"

#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/previews/previews_https_notification_infobar_decider.h"
#include "chrome/browser/previews/previews_lite_page_decider.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

// Class to test the TopHostProvider and the HintsFetcherTopHostBlacklist.
class DataSaverTopHostProviderTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Advance by 1-day to avoid running into null checks.
    test_clock_.Advance(base::TimeDelta::FromDays(1));

    top_host_provider_ =
        std::make_unique<DataSaverTopHostProvider>(profile(), &test_clock_);

    service_ = SiteEngagementService::Get(profile());
    pref_service_ = profile()->GetPrefs();

    drp_test_context_ =
        data_reduction_proxy::DataReductionProxyTestContext::Builder()
            .WithMockConfig()
            .Build();
    drp_test_context_->DisableWarmupURLFetch();
  }

  void TearDown() override {
    drp_test_context_->DestroySettings();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetDataSaverEnabled(bool enabled) {
    drp_test_context_->SetDataReductionProxyEnabled(enabled);
  }

  void AddEngagedHosts(size_t num_hosts) {
    for (size_t i = 1; i <= num_hosts; i++) {
      AddEngagedHost(
          GURL(base::StringPrintf("https://domain%zu.com", i)),
          static_cast<int>(
              i + SiteEngagementScore::GetFirstDailyEngagementPoints()));
    }
  }

  void AddEngagedHostsWithPoints(size_t num_hosts, int num_points) {
    for (size_t i = 1; i <= num_hosts; i++) {
      AddEngagedHost(GURL(base::StringPrintf("https://domain%zu.com", i)),
                     num_points);
    }
  }

  void AddEngagedHost(GURL url, int num_points) {
    service_->AddPointsForTesting(url, num_points);
  }

  bool IsHostBlacklisted(const std::string& host) const {
    const base::DictionaryValue* top_host_blacklist =
        pref_service_->GetDictionary(
            optimization_guide::prefs::kHintsFetcherDataSaverTopHostBlacklist);
    return top_host_blacklist->FindKey(
        optimization_guide::HashHostForDictionary(host));
  }

  double GetHintsFetcherDataSaverTopHostBlacklistMinimumEngagementScore()
      const {
    return pref_service_->GetDouble(
        optimization_guide::prefs::
            kHintsFetcherDataSaverTopHostBlacklistMinimumEngagementScore);
  }

  void PopulateTopHostBlacklist(size_t num_hosts) {
    std::unique_ptr<base::DictionaryValue> top_host_filter =
        pref_service_
            ->GetDictionary(optimization_guide::prefs::
                                kHintsFetcherDataSaverTopHostBlacklist)
            ->CreateDeepCopy();

    for (size_t i = 1; i <= num_hosts; i++) {
      top_host_filter->SetBoolKey(optimization_guide::HashHostForDictionary(
                                      base::StringPrintf("domain%zu.com", i)),
                                  true);
    }
    pref_service_->Set(
        optimization_guide::prefs::kHintsFetcherDataSaverTopHostBlacklist,
        *top_host_filter);
  }

  void AddHostToBlackList(const std::string& host) {
    std::unique_ptr<base::DictionaryValue> top_host_filter =
        pref_service_
            ->GetDictionary(optimization_guide::prefs::
                                kHintsFetcherDataSaverTopHostBlacklist)
            ->CreateDeepCopy();
    top_host_filter->SetBoolKey(optimization_guide::HashHostForDictionary(host),
                                true);
    pref_service_->Set(
        optimization_guide::prefs::kHintsFetcherDataSaverTopHostBlacklist,
        *top_host_filter);
  }

  void SimulateUniqueNavigationsToTopHosts(size_t num_hosts) {
    for (size_t i = 1; i <= num_hosts; i++) {
      SimulateNavigation(GURL(base::StringPrintf("https://domain%zu.com", i)));
    }
  }

  void SimulateNavigation(GURL url) {
    std::unique_ptr<content::MockNavigationHandle> test_handle_ =
        std::make_unique<content::MockNavigationHandle>(url, main_rfh());
    DataSaverTopHostProvider::MaybeUpdateTopHostBlacklist(test_handle_.get());
  }

  void RemoveHostsFromBlacklist(size_t num_hosts_navigated) {
    std::unique_ptr<base::DictionaryValue> top_host_filter =
        pref_service_
            ->GetDictionary(optimization_guide::prefs::
                                kHintsFetcherDataSaverTopHostBlacklist)
            ->CreateDeepCopy();

    for (size_t i = 1; i <= num_hosts_navigated; i++) {
      top_host_filter->RemoveKey(optimization_guide::HashHostForDictionary(
          base::StringPrintf("domain%zu.com", i)));
    }
    pref_service_->Set(
        optimization_guide::prefs::kHintsFetcherDataSaverTopHostBlacklist,
        *top_host_filter);
  }

  void SetTopHostBlacklistState(
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState
          blacklist_state) {
    profile()->GetPrefs()->SetInteger(
        optimization_guide::prefs::kHintsFetcherDataSaverTopHostBlacklistState,
        static_cast<int>(blacklist_state));
  }

  optimization_guide::prefs::HintsFetcherTopHostBlacklistState
  GetCurrentTopHostBlacklistState() {
    return static_cast<
        optimization_guide::prefs::HintsFetcherTopHostBlacklistState>(
        pref_service_->GetInteger(
            optimization_guide::prefs::
                kHintsFetcherDataSaverTopHostBlacklistState));
  }

  DataSaverTopHostProvider* top_host_provider() {
    return top_host_provider_.get();
  }

  base::SimpleTestClock test_clock_;

 private:
  std::unique_ptr<DataSaverTopHostProvider> top_host_provider_;
  std::unique_ptr<data_reduction_proxy::DataReductionProxyTestContext>
      drp_test_context_;
  SiteEngagementService* service_;
  PrefService* pref_service_;
};

TEST_F(DataSaverTopHostProviderTest, CreateIfAllowedNonDataSaverUser) {
  SetDataSaverEnabled(false);
  ASSERT_FALSE(DataSaverTopHostProvider::CreateIfAllowed(profile()));
}

TEST_F(DataSaverTopHostProviderTest,
       CreateIfAllowedDataSaverUserInfobarNotSeen) {
  SetDataSaverEnabled(true);

  // Make sure infobar not shown.
  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  PreviewsLitePageDecider* decider =
      previews_service->previews_lite_page_decider();
  // Initialize settings here so Lite Pages Decider checks for the Data Saver
  // bit.
  decider->OnSettingsInitialized();
  EXPECT_TRUE(decider->NeedsToNotifyUser());

  ASSERT_FALSE(DataSaverTopHostProvider::CreateIfAllowed(profile()));
}

TEST_F(DataSaverTopHostProviderTest, CreateIfAllowedDataSaverUserInfobarSeen) {
  SetDataSaverEnabled(true);

  // Navigate so infobar is shown.
  PreviewsUITabHelper::CreateForWebContents(web_contents());
  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  PreviewsHTTPSNotificationInfoBarDecider* decider =
      previews_service->previews_https_notification_infobar_decider();
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("http://whatever.com"));
  EXPECT_FALSE(decider->NeedsToNotifyUser());

  ASSERT_TRUE(DataSaverTopHostProvider::CreateIfAllowed(profile()));
}

TEST_F(DataSaverTopHostProviderTest, GetTopHostsMaxSites) {
  SetTopHostBlacklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlacklistState::kInitialized);
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 3;
  AddEngagedHosts(engaged_hosts);

  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);

  EXPECT_EQ(max_top_hosts, hosts.size());
}

TEST_F(DataSaverTopHostProviderTest,
       GetTopHostsFiltersPrivacyBlackedlistedHosts) {
  SetTopHostBlacklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlacklistState::kInitialized);
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_hosts_blacklisted = 2;
  AddEngagedHosts(engaged_hosts);

  PopulateTopHostBlacklist(num_hosts_blacklisted);

  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);

  EXPECT_EQ(hosts.size(), engaged_hosts - num_hosts_blacklisted);
}

TEST_F(DataSaverTopHostProviderTest, GetTopHostsInitializeBlacklistState) {
  EXPECT_EQ(GetCurrentTopHostBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kNotInitialized);
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  AddEngagedHosts(engaged_hosts);

  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  // On initialization, GetTopHosts should always return zero hosts.
  EXPECT_EQ(hosts.size(), 0u);
  EXPECT_EQ(GetCurrentTopHostBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kInitialized);
}

TEST_F(DataSaverTopHostProviderTest,
       GetTopHostsBlacklistStateNotInitializedToInitialized) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_hosts_blacklisted = 5;
  AddEngagedHosts(engaged_hosts);

  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);

  // Blacklist should now have items removed.
  size_t num_navigations = 2;
  RemoveHostsFromBlacklist(num_navigations);

  hosts = top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(),
            engaged_hosts - (num_hosts_blacklisted - num_navigations));
  EXPECT_EQ(GetCurrentTopHostBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kInitialized);
}

TEST_F(DataSaverTopHostProviderTest,
       GetTopHostsBlacklistStateNotInitializedToEmpty) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_hosts_blacklisted = 5;
  AddEngagedHosts(engaged_hosts);

  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);

  // Blacklist should now have items removed.
  size_t num_navigations = 5;
  RemoveHostsFromBlacklist(num_navigations);

  hosts = top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(),
            engaged_hosts - (num_hosts_blacklisted - num_navigations));
  EXPECT_EQ(
      GetCurrentTopHostBlacklistState(),
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState::kEmpty);
}

TEST_F(DataSaverTopHostProviderTest,
       MaybeUpdateTopHostBlacklistNavigationsOnBlacklist) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_top_hosts = 3;
  AddEngagedHosts(engaged_hosts);

  // The blacklist should be populated on the first request.
  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);

  // Navigate to some engaged hosts to trigger their removal from the top host
  // blacklist.
  SimulateUniqueNavigationsToTopHosts(num_top_hosts);

  hosts = top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), num_top_hosts);
}

TEST_F(DataSaverTopHostProviderTest,
       MaybeUpdateTopHostBlacklistEmptyBlacklist) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_top_hosts = 5;
  AddEngagedHosts(engaged_hosts);

  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);

  SimulateUniqueNavigationsToTopHosts(num_top_hosts);

  EXPECT_EQ(
      GetCurrentTopHostBlacklistState(),
      optimization_guide::prefs::HintsFetcherTopHostBlacklistState::kEmpty);

  hosts = top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), num_top_hosts);
}

TEST_F(DataSaverTopHostProviderTest,
       HintsFetcherTopHostBlacklistNonHTTPOrHTTPSHost) {
  size_t engaged_hosts = 5;
  size_t max_top_hosts = 5;
  size_t num_hosts_blacklisted = 5;
  GURL http_url = GURL("http://anyscheme.com");
  GURL file_url = GURL("file://anyscheme.com");
  AddEngagedHosts(engaged_hosts);
  AddEngagedHost(http_url, 5);

  PopulateTopHostBlacklist(num_hosts_blacklisted);
  AddHostToBlackList(http_url.host());

  SetTopHostBlacklistState(optimization_guide::prefs::
                               HintsFetcherTopHostBlacklistState::kInitialized);

  // A Non HTTP/HTTPS navigation should not remove a host from the blacklist.
  SimulateNavigation(file_url);
  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);
  // The host, anyscheme.com, should still be on the blacklist.
  EXPECT_TRUE(IsHostBlacklisted(file_url.host()));

  // TopHostProviderImpl prevents HTTP hosts from being returned.
  SimulateNavigation(http_url);
  hosts = top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);

  EXPECT_FALSE(IsHostBlacklisted(http_url.host()));
}

TEST_F(DataSaverTopHostProviderTest, IntializeTopHostBlacklistWithMaxTopSites) {
  size_t engaged_hosts =
      optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize() + 1;
  size_t max_top_hosts =
      optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize() + 1;
  AddEngagedHosts(engaged_hosts);

  // Blacklist should be populated on the first request.
  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(hosts.size(), 0u);

  hosts = top_host_provider()->GetTopHosts(max_top_hosts);
  EXPECT_EQ(
      hosts.size(),
      engaged_hosts -
          optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize());
  EXPECT_EQ(GetCurrentTopHostBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kInitialized);

  // The last host has the most engagement points so it will be blacklisted. The
  // first host has the lowest engagement score and will not be blacklisted
  // because it is not in the top MaxHintsFetcherTopHostBlacklistSize engaged
  // hosts by engagement score.
  EXPECT_TRUE(IsHostBlacklisted(base::StringPrintf(
      "domain%zu.com",
      optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize())));
  EXPECT_FALSE(IsHostBlacklisted(base::StringPrintf("domain%u.com", 1u)));
}

TEST_F(DataSaverTopHostProviderTest, TopHostsFilteredByEngagementThreshold) {
  size_t engaged_hosts =
      optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize() + 1;

  // Set the count of maximum hosts requested to a large number so that the
  // count itself does not prevent top_host_provider() from returning hosts that
  // it would have otherwise returned.
  static const size_t kMaxHostsRequested = INT16_MAX;

  AddEngagedHosts(engaged_hosts);
  // Add two hosts with very low engagement scores that should not be returned
  // by the top host provider.
  AddEngagedHost(GURL("https://lowengagement1.com"), 1);
  AddEngagedHost(GURL("https://lowengagement2.com"), 1);

  // Blacklist should be populated on the first request. Set the count of
  // desired
  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(kMaxHostsRequested);
  EXPECT_EQ(hosts.size(), 0u);

  hosts = top_host_provider()->GetTopHosts(kMaxHostsRequested);
  EXPECT_EQ(1u, hosts.size());
  EXPECT_EQ(GetCurrentTopHostBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kInitialized);

  // The hosts with engagement scores below the minimum threshold should not be
  // returned.
  EXPECT_EQ(std::find(hosts.begin(), hosts.end(), "lowengagement1.com"),
            hosts.end());
  EXPECT_EQ(std::find(hosts.begin(), hosts.end(), "lowengagement2.com"),
            hosts.end());

  // Advance the clock by more than DurationApplyLowEngagementScoreThreshold().
  // top_host_provider() should also return hosts with low engagement score.
  test_clock_.Advance(
      optimization_guide::features::DurationApplyLowEngagementScoreThreshold() +
      base::TimeDelta::FromDays(1));
  AddEngagedHost(GURL("https://lowengagement3.com"), 1);
  AddEngagedHost(GURL("https://lowengagement4.com"), 1);

  hosts = top_host_provider()->GetTopHosts(kMaxHostsRequested);
  // Four hosts lowengagement[1-4] should now be present in |hosts|.
  EXPECT_EQ(
      engaged_hosts + 4 -
          optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize(),
      hosts.size());
  EXPECT_EQ(GetCurrentTopHostBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kInitialized);

  // The hosts with engagement scores below the minimum threshold should not be
  // returned.
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement1.com"),
            hosts.end());
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement2.com"),
            hosts.end());
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement3.com"),
            hosts.end());
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement4.com"),
            hosts.end());
}

TEST_F(DataSaverTopHostProviderTest,
       TopHostsFilteredByEngagementThreshold_NumPoints) {
  size_t engaged_hosts =
      optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize() + 1;

  // Set the count of maximum hosts requested to a large number so that the
  // count itself does not prevent top_host_provider() from returning hosts that
  // it would have otherwise returned.
  static const size_t kMaxHostsRequested = INT16_MAX;

  AddEngagedHostsWithPoints(engaged_hosts, 15);
  // Add two hosts with engagement scores less than 15. These hosts should not
  // be returned by the top host provider because the minimum engagement score
  // threshold is set to a value larger than 5.
  AddEngagedHost(GURL("https://lowengagement1.com"), 5);
  AddEngagedHost(GURL("https://lowengagement2.com"), 5);

  // Before the blacklist is populated, the threshold should have a default
  // value.
  EXPECT_EQ(3,
            GetHintsFetcherDataSaverTopHostBlacklistMinimumEngagementScore());

  // Blacklist should be populated on the first request.
  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(kMaxHostsRequested);
  EXPECT_EQ(hosts.size(), 0u);

  hosts = top_host_provider()->GetTopHosts(kMaxHostsRequested);
  EXPECT_NEAR(GetHintsFetcherDataSaverTopHostBlacklistMinimumEngagementScore(),
              GetHintsFetcherDataSaverTopHostBlacklistMinimumEngagementScore(),
              1);
  EXPECT_EQ(3u, hosts.size());
  EXPECT_EQ(GetCurrentTopHostBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kInitialized);
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement1.com"),
            hosts.end());
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement2.com"),
            hosts.end());
}

TEST_F(DataSaverTopHostProviderTest,
       TopHostsFilteredByEngagementThreshold_LowScore) {
  size_t engaged_hosts =
      optimization_guide::features::MaxHintsFetcherTopHostBlacklistSize() - 2;

  // Set the count of maximum hosts requested to a large number so that the
  // count itself does not prevent top_host_provider() from returning hosts that
  // it would have otherwise returned.
  static const size_t kMaxHostsRequested = INT16_MAX;

  AddEngagedHostsWithPoints(engaged_hosts, 2);

  // Blacklist should be populated on the first request. Set the count of
  // desired
  std::vector<std::string> hosts =
      top_host_provider()->GetTopHosts(kMaxHostsRequested);
  EXPECT_EQ(hosts.size(), 0u);

  // Add two hosts with very low engagement scores. These hosts should be
  // returned by top_host_provider() even with low score.
  EXPECT_EQ(-1,
            GetHintsFetcherDataSaverTopHostBlacklistMinimumEngagementScore());
  AddEngagedHost(GURL("https://lowengagement1.com"), 1);
  AddEngagedHost(GURL("https://lowengagement2.com"), 1);

  hosts = top_host_provider()->GetTopHosts(kMaxHostsRequested);
  EXPECT_EQ(2u, hosts.size());
  EXPECT_EQ(GetCurrentTopHostBlacklistState(),
            optimization_guide::prefs::HintsFetcherTopHostBlacklistState::
                kInitialized);
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement1.com"),
            hosts.end());
  EXPECT_NE(std::find(hosts.begin(), hosts.end(), "lowengagement2.com"),
            hosts.end());
}
