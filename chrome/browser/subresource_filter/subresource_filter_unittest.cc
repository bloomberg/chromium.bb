// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/subresource_filter/test_ruleset_publisher.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "components/subresource_filter/content/browser/content_ruleset_service.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"
#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"
#include "components/subresource_filter/core/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// End to end unit test harness of (most of) the browser process portions of the
// subresource filtering code.
class SubresourceFilterTest : public ChromeRenderViewHostTestHarness {
 public:
  SubresourceFilterTest() : field_trial_list_(nullptr) {}
  ~SubresourceFilterTest() override {}

  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

    // Ensure correct features.
    scoped_feature_list_.InitFromCommandLine("SafeBrowsingV4OnlyEnabled", "");
    scoped_feature_toggle_.reset(
        new subresource_filter::testing::ScopedSubresourceFilterFeatureToggle(
            base::FeatureList::OVERRIDE_ENABLE_FEATURE,
            subresource_filter::kActivationLevelEnabled,
            subresource_filter::kActivationScopeActivationList,
            subresource_filter::kActivationListSubresourceFilter));
    NavigateAndCommit(GURL("https://example.first"));

    // Set up safe browsing service with the fake database manager.
    //
    // TODO(csharrison): This is a bit ugly. See if the instructions in
    // test_safe_browsing_service.h can be adapted to be used in unit tests.
    safe_browsing::TestSafeBrowsingServiceFactory sb_service_factory;
    fake_safe_browsing_database_ = new FakeSafeBrowsingDatabaseManager();
    sb_service_factory.SetTestDatabaseManager(
        fake_safe_browsing_database_.get());
    safe_browsing::SafeBrowsingService::RegisterFactory(&sb_service_factory);
    auto* safe_browsing_service =
        sb_service_factory.CreateSafeBrowsingService();
    safe_browsing::SafeBrowsingService::RegisterFactory(nullptr);
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        safe_browsing_service);
    g_browser_process->safe_browsing_service()->Initialize();

    // Set up the ruleset service.
    ASSERT_TRUE(ruleset_service_dir_.CreateUniqueTempDir());
    subresource_filter::IndexedRulesetVersion::RegisterPrefs(
        pref_service_.registry());
    auto content_service =
        base::MakeUnique<subresource_filter::ContentRulesetService>(
            base::ThreadTaskRunnerHandle::Get());
    auto ruleset_service = base::MakeUnique<subresource_filter::RulesetService>(
        &pref_service_, base::ThreadTaskRunnerHandle::Get(),
        content_service.get(), ruleset_service_dir_.GetPath());
    content_service->set_ruleset_service(std::move(ruleset_service));
    TestingBrowserProcess::GetGlobal()->SetRulesetService(
        std::move(content_service));

    // Publish the test ruleset.
    subresource_filter::testing::TestRulesetCreator ruleset_creator;
    subresource_filter::testing::TestRulesetPair test_ruleset_pair;
    ruleset_creator.CreateRulesetToDisallowURLsWithPathSuffix(
        "disallowed.html", &test_ruleset_pair);
    subresource_filter::testing::TestRulesetPublisher test_ruleset_publisher;
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));

    // Set up the tab helpers.
    InfoBarService::CreateForWebContents(web_contents());
    TabSpecificContentSettings::CreateForWebContents(web_contents());

    std::unique_ptr<ChromeSubresourceFilterClient> subresource_filter_client(
        new ChromeSubresourceFilterClient(web_contents()));
    client_ = subresource_filter_client.get();
    subresource_filter::ContentSubresourceFilterDriverFactory::
        CreateForWebContents(web_contents(),
                             std::move(subresource_filter_client));
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    fake_safe_browsing_database_ = nullptr;
    TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();

    // Must explicitly set these to null and pump the run loop to ensure that
    // all cleanup related to these classes actually happens.
    TestingBrowserProcess::GetGlobal()->SetRulesetService(nullptr);
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    base::RunLoop().RunUntilIdle();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Will return nullptr if the navigation fails.
  content::RenderFrameHost* SimulateNavigateAndCommit(
      const GURL& url,
      content::RenderFrameHost* rfh) {
    auto simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, rfh);
    simulator->Commit();
    return simulator->GetLastThrottleCheckResult() ==
                   content::NavigationThrottle::PROCEED
               ? simulator->GetFinalRenderFrameHost()
               : nullptr;
  }

  // Returns the frame host the navigation commit in, or nullptr if it did not
  // succeed.
  content::RenderFrameHost* CreateAndNavigateDisallowedSubframe(
      content::RenderFrameHost* parent) {
    auto* subframe =
        content::RenderFrameHostTester::For(parent)->AppendChild("subframe");
    return SimulateNavigateAndCommit(
        GURL("https://example.test/disallowed.html"), subframe);
  }

  void ConfigureAsSubresourceFilterOnlyURL(const GURL& url) {
    fake_safe_browsing_database_->AddBlacklistedUrl(
        url, safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER);
  }

  ChromeSubresourceFilterClient* client() { return client_; }

 private:
  base::ScopedTempDir ruleset_service_dir_;
  base::FieldTrialList field_trial_list_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<
      subresource_filter::testing::ScopedSubresourceFilterFeatureToggle>
      scoped_feature_toggle_;
  TestingPrefServiceSimple pref_service_;

  scoped_refptr<FakeSafeBrowsingDatabaseManager> fake_safe_browsing_database_;

  ChromeSubresourceFilterClient* client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterTest);
};

TEST_F(SubresourceFilterTest, SimpleAllowedLoad) {
  GURL url("https://example.test");
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_TRUE(CreateAndNavigateDisallowedSubframe(main_rfh()));
  EXPECT_FALSE(client()->did_show_ui_for_navigation());
}

TEST_F(SubresourceFilterTest, SimpleDisallowedLoad) {
  GURL url("https://example.test");
  ConfigureAsSubresourceFilterOnlyURL(url);
  SimulateNavigateAndCommit(url, main_rfh());
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(main_rfh()));
  EXPECT_TRUE(client()->did_show_ui_for_navigation());
}
