// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/safe_browsing_triggered_popup_blocker.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebTriggeringEventInfo.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class SafeBrowsingTriggeredPopupBlockerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SafeBrowsingTriggeredPopupBlockerTest() {}
  ~SafeBrowsingTriggeredPopupBlockerTest() override {}

  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

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

    // Required for the safe browsing notifications on navigation.
    ChromeSubresourceFilterClient::CreateForWebContents(web_contents());

    scoped_feature_list_ = DefaultFeatureList();
    popup_blocker_ =
        base::MakeUnique<SafeBrowsingTriggeredPopupBlocker>(web_contents());
  }

  void TearDown() override {
    fake_safe_browsing_database_ = nullptr;
    TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();

    // Must explicitly set these to null and pump the run loop to ensure that
    // all cleanup related to these classes actually happens.
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    base::RunLoop().RunUntilIdle();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  virtual std::unique_ptr<base::test::ScopedFeatureList> DefaultFeatureList() {
    auto feature_list = base::MakeUnique<base::test::ScopedFeatureList>();
    feature_list->InitAndEnableFeature(kAbusiveExperienceEnforce);
    return feature_list;
  }

  FakeSafeBrowsingDatabaseManager* fake_safe_browsing_database() {
    return fake_safe_browsing_database_.get();
  }

  base::test::ScopedFeatureList* ResetFeatureAndGet() {
    scoped_feature_list_ = base::MakeUnique<base::test::ScopedFeatureList>();
    return scoped_feature_list_.get();
  }

  SafeBrowsingTriggeredPopupBlocker* popup_blocker() {
    return popup_blocker_.get();
  }

  void MarkUrlAsAbusiveWithLevel(const GURL& url,
                                 safe_browsing::SubresourceFilterLevel level) {
    safe_browsing::ThreatMetadata metadata;
    metadata.subresource_filter_match
        [safe_browsing::SubresourceFilterType::ABUSIVE] = level;
    fake_safe_browsing_database()->AddBlacklistedUrl(
        url, safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER, metadata);
  }

  void MarkUrlAsAbusiveEnforce(const GURL& url) {
    MarkUrlAsAbusiveWithLevel(url,
                              safe_browsing::SubresourceFilterLevel::ENFORCE);
  }

  void MarkUrlAsAbusiveWarning(const GURL& url) {
    MarkUrlAsAbusiveWithLevel(url, safe_browsing::SubresourceFilterLevel::WARN);
  }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  scoped_refptr<FakeSafeBrowsingDatabaseManager> fake_safe_browsing_database_;
  std::unique_ptr<SafeBrowsingTriggeredPopupBlocker> popup_blocker_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingTriggeredPopupBlockerTest);
};

class IgnoreSublistSafeBrowsingTriggeredPopupBlockerTest
    : public SafeBrowsingTriggeredPopupBlockerTest {
  std::unique_ptr<base::test::ScopedFeatureList> DefaultFeatureList() override {
    auto feature_list = base::MakeUnique<base::test::ScopedFeatureList>();
    feature_list->InitAndEnableFeatureWithParameters(
        kAbusiveExperienceEnforce, {{"ignore_sublists", "true"}});
    return feature_list;
  }
};

TEST_F(IgnoreSublistSafeBrowsingTriggeredPopupBlockerTest,
       MatchNoSublist_BlocksPopup) {
  const GURL url("https://example.test/");
  fake_safe_browsing_database()->AddBlacklistedUrl(
      url, safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER);
  NavigateAndCommit(url);
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
}

// ignore_sublists should still block on URLs matching a sublist.
TEST_F(IgnoreSublistSafeBrowsingTriggeredPopupBlockerTest,
       MatchSublist_BlocksPopup) {
  const GURL url("https://example.test/");
  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest, MatchingURL_BlocksPopup) {
  const GURL url("https://example.test/");
  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest,
       MatchingURL_BlocksPopupFromOpenURL) {
  const GURL url("https://example.test/");
  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);

  // If the popup is coming from OpenURL params, the strong popup blocker is
  // only going to look at the triggering event info. It will only block the
  // popup if we know the triggering event is untrusted.
  content::OpenURLParams params(
      GURL("https://example.popup/"), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      true /* is_renderer_initiated */);
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(&params));

  params.triggering_event_info =
      blink::WebTriggeringEventInfo::kFromUntrustedEvent;
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(&params));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest, NoMatch_NoBlocking) {
  const GURL url("https://example.test/");
  NavigateAndCommit(url);
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest, NoFeature_NoBlocking) {
  ResetFeatureAndGet()->InitAndDisableFeature(kAbusiveExperienceEnforce);
  const GURL url("https://example.test/");
  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest, OnlyBlockOnMatchingUrls) {
  const GURL url1("https://example.first/");
  const GURL url2("https://example.second/");
  const GURL url3("https://example.third/");
  // Only mark url2 as abusive.
  MarkUrlAsAbusiveEnforce(url2);

  NavigateAndCommit(url1);
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));

  NavigateAndCommit(url2);
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));

  NavigateAndCommit(url3);
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));

  NavigateAndCommit(url1);
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest,
       SameDocumentNavigation_MaintainsBlocking) {
  const GURL url("https://example.first/");
  const GURL hash_url("https://example.first/#hash");

  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));

  // This is merely a same document navigation, keep the popup blocker.
  NavigateAndCommit(hash_url);
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
}

TEST_F(SafeBrowsingTriggeredPopupBlockerTest,
       FailNavigation_MaintainsBlocking) {
  const GURL url("https://example.first/");
  const GURL fail_url("https://example.fail/");

  MarkUrlAsAbusiveEnforce(url);
  NavigateAndCommit(url);
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));

  // Abort the navigation before it commits.
  content::NavigationSimulator::NavigateAndFailFromDocument(
      fail_url, net::ERR_ABORTED, main_rfh());
  EXPECT_TRUE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));

  // Committing an error page should probably reset the blocker though, despite
  // the fact that it is probably a bug for an error page to spawn popups.
  content::NavigationSimulator::NavigateAndFailFromDocument(
      fail_url, net::ERR_CONNECTION_RESET, main_rfh());
  EXPECT_FALSE(popup_blocker()->ShouldApplyStrongPopupBlocker(nullptr));
}
