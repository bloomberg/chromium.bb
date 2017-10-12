// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_tab_helper.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_service_test_taco.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const GURL kTestPageUrl("http://mystery.site/foo.html");
}  // namespace

namespace offline_pages {

class TestMetricsCollector : public OfflineMetricsCollector {
 public:
  TestMetricsCollector() = default;
  ~TestMetricsCollector() override = default;

  // OfflineMetricsCollector implementation
  void OnAppStartupOrResume() override { app_startup_count_++; }
  void OnSuccessfulNavigationOnline() override {
    successful_online_navigations_count_++;
  }
  void OnSuccessfulNavigationOffline() override {
    successful_offline_navigations_count_++;
  }
  void OnPrefetchEnabled() override {}
  void OnHasPrefetchedPagesDetected() override {}
  void OnSuccessfulPagePrefetch() override {}
  void OnPrefetchedPageOpened() override {}
  void ReportAccumulatedStats() override { report_stats_count_++; }

  int app_startup_count_ = 0;
  int successful_offline_navigations_count_ = 0;
  int successful_online_navigations_count_ = 0;
  int report_stats_count_ = 0;
};

// This is used by KeyedServiceFactory::SetTestingFactoryAndUse.
std::unique_ptr<KeyedService> BuildTestPrefetchService(
    content::BrowserContext*) {
  auto taco = base::MakeUnique<PrefetchServiceTestTaco>();
  taco->SetOfflineMetricsCollector(base::MakeUnique<TestMetricsCollector>());
  return taco->CreateAndReturnPrefetchService();
}

class OfflinePageTabHelperTest : public content::RenderViewHostTestHarness {
 public:
  OfflinePageTabHelperTest();
  ~OfflinePageTabHelperTest() override {}

  void SetUp() override;
  void TearDown() override;
  content::BrowserContext* CreateBrowserContext() override;

  OfflinePageTabHelper* tab_helper() const { return tab_helper_; }
  PrefetchService* prefetch_service() const { return prefetch_service_; }
  content::NavigationSimulator* navigation_simulator() {
    return navigation_simulator_.get();
  }
  TestMetricsCollector* metrics() const {
    return static_cast<TestMetricsCollector*>(
        prefetch_service_->GetOfflineMetricsCollector());
  }

 private:
  OfflinePageTabHelper* tab_helper_;   // Owned by WebContents.
  PrefetchService* prefetch_service_;  // Keyed Service.
  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;

  base::WeakPtrFactory<OfflinePageTabHelperTest> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(OfflinePageTabHelperTest);
};

OfflinePageTabHelperTest::OfflinePageTabHelperTest()
    : tab_helper_(nullptr), weak_ptr_factory_(this) {}

void OfflinePageTabHelperTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();

  PrefetchServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      browser_context(), BuildTestPrefetchService);
  prefetch_service_ =
      PrefetchServiceFactory::GetForBrowserContext(browser_context());

  // This initializes a nav stack inside the harness.
  NavigateAndCommit(kTestPageUrl);

  OfflinePageTabHelper::CreateForWebContents(web_contents());
  tab_helper_ = OfflinePageTabHelper::FromWebContents(web_contents());

  navigation_simulator_ = content::NavigationSimulator::CreateRendererInitiated(
      kTestPageUrl, main_rfh());
  navigation_simulator_->SetTransition(ui::PAGE_TRANSITION_LINK);
}

void OfflinePageTabHelperTest::TearDown() {
  content::RenderViewHostTestHarness::TearDown();
}

content::BrowserContext* OfflinePageTabHelperTest::CreateBrowserContext() {
  TestingProfile::Builder builder;
  return builder.Build().release();
}

// Checks the test setup.
TEST_F(OfflinePageTabHelperTest, InitialSetup) {
  EXPECT_NE(nullptr, tab_helper());
  EXPECT_NE(nullptr, prefetch_service());
  EXPECT_NE(nullptr, prefetch_service()->GetOfflineMetricsCollector());
  EXPECT_EQ(metrics(), prefetch_service()->GetOfflineMetricsCollector());
  EXPECT_EQ(0, metrics()->app_startup_count_);
  EXPECT_EQ(0, metrics()->successful_online_navigations_count_);
  EXPECT_EQ(0, metrics()->successful_offline_navigations_count_);
  EXPECT_EQ(0, metrics()->report_stats_count_);
}

TEST_F(OfflinePageTabHelperTest, MetricsStartNavigation) {
  // This causes WCO::DidStartNavigation()
  navigation_simulator()->Start();

  EXPECT_EQ(1, metrics()->app_startup_count_);
  EXPECT_EQ(0, metrics()->successful_online_navigations_count_);
  EXPECT_EQ(0, metrics()->successful_offline_navigations_count_);
  EXPECT_EQ(0, metrics()->report_stats_count_);
}

TEST_F(OfflinePageTabHelperTest, MetricsOnlineNavigation) {
  navigation_simulator()->Start();
  navigation_simulator()->Commit();

  EXPECT_EQ(1, metrics()->app_startup_count_);
  EXPECT_EQ(1, metrics()->successful_online_navigations_count_);
  EXPECT_EQ(0, metrics()->successful_offline_navigations_count_);
  // Since this is online navigation, request to send data should be made.
  EXPECT_EQ(1, metrics()->report_stats_count_);
}

TEST_F(OfflinePageTabHelperTest, MetricsOfflineNavigation) {
  navigation_simulator()->Start();

  // Simulate offline interceptor loading an offline page instead.
  OfflinePageItem offlinePage(kTestPageUrl, 0, ClientId(), base::FilePath(), 0);
  OfflinePageHeader offlineHeader;
  tab_helper()->SetOfflinePage(offlinePage, offlineHeader, false);

  navigation_simulator()->Commit();

  EXPECT_EQ(1, metrics()->app_startup_count_);
  EXPECT_EQ(0, metrics()->successful_online_navigations_count_);
  EXPECT_EQ(1, metrics()->successful_offline_navigations_count_);
  // During offline navigation, request to send data should not be made.
  EXPECT_EQ(0, metrics()->report_stats_count_);
}

}  // namespace offline_pages
