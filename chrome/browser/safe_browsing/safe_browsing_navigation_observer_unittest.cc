// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace safe_browsing {

class SBNavigationObserverTest : public BrowserWithTestWindowTest {
 public:
  SBNavigationObserverTest() {}
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("http://foo/0"));
    navigation_observer_manager_ = new SafeBrowsingNavigationObserverManager();
    navigation_observer_ = new SafeBrowsingNavigationObserver(
        browser()->tab_strip_model()->GetWebContentsAt(0),
        navigation_observer_manager_);
  }
  void TearDown() override {
    delete navigation_observer_;
    BrowserWithTestWindowTest::TearDown();
  }
  void VerifyNavigationEvent(const GURL& expected_source_url,
                             const GURL& expected_source_main_frame_url,
                             const GURL& expected_original_request_url,
                             const GURL& expected_destination_url,
                             int expected_source_tab,
                             int expected_target_tab,
                             bool expected_is_user_initiated,
                             bool expected_has_committed,
                             bool expected_has_server_redirect,
                             const NavigationEvent& actual_nav_event) {
    EXPECT_EQ(expected_source_url, actual_nav_event.source_url);
    EXPECT_EQ(expected_source_main_frame_url,
              actual_nav_event.source_main_frame_url);
    EXPECT_EQ(expected_original_request_url,
              actual_nav_event.original_request_url);
    EXPECT_EQ(expected_destination_url, actual_nav_event.destination_url);
    EXPECT_EQ(expected_source_tab, actual_nav_event.source_tab_id);
    EXPECT_EQ(expected_target_tab, actual_nav_event.target_tab_id);
    EXPECT_EQ(expected_is_user_initiated, actual_nav_event.is_user_initiated);
    EXPECT_EQ(expected_has_committed, actual_nav_event.has_committed);
    EXPECT_EQ(expected_has_server_redirect,
              actual_nav_event.has_server_redirect);
  }

  SafeBrowsingNavigationObserverManager::NavigationMap* navigation_map() {
    return navigation_observer_manager_->navigation_map();
  }

 protected:
  SafeBrowsingNavigationObserverManager* navigation_observer_manager_;
  SafeBrowsingNavigationObserver* navigation_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SBNavigationObserverTest);
};

TEST_F(SBNavigationObserverTest, BasicNavigationAndCommit) {
  // Navigation in current tab.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetWebContentsAt(0)->GetController();
  browser()->OpenURL(
      content::OpenURLParams(GURL("http://foo/1"), content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_AUTO_BOOKMARK, false));
  CommitPendingLoad(controller);
  int tab_id = SessionTabHelper::IdForTab(controller->GetWebContents());
  auto nav_map = navigation_map();
  ASSERT_EQ(std::size_t(1), nav_map->size());
  ASSERT_EQ(std::size_t(1), nav_map->at(GURL("http://foo/1")).size());
  VerifyNavigationEvent(GURL("http://foo/0"),  // source_url
                        GURL("http://foo/0"),  // source_main_frame_url
                        GURL("http://foo/1"),  // original_request_url
                        GURL("http://foo/1"),  // destination_url
                        tab_id,                // source_tab_id
                        tab_id,                // target_tab_id
                        true,                  // is_user_initiated
                        true,                  // has_committed
                        false,                 // has_server_redirect
                        nav_map->at(GURL("http://foo/1")).at(0));
}

TEST_F(SBNavigationObserverTest, ServerRedirect) {
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(
          browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame());
  rfh_tester->SimulateNavigationStart(GURL("http://foo/3"));
  GURL redirect("http://redirect/1");
  rfh_tester->SimulateRedirect(redirect);
  rfh_tester->SimulateNavigationCommit(redirect);
  int tab_id = SessionTabHelper::IdForTab(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  auto nav_map = navigation_map();
  ASSERT_EQ(std::size_t(1), nav_map->size());
  ASSERT_EQ(std::size_t(1), nav_map->at(redirect).size());
  VerifyNavigationEvent(GURL("http://foo/0"),       // source_url
                        GURL("http://foo/0"),       // source_main_frame_url
                        GURL("http://foo/3"),       // original_request_url
                        GURL("http://redirect/1"),  // destination_url
                        tab_id,                     // source_tab_id
                        tab_id,                     // target_tab_id
                        false,                      // is_user_initiated
                        true,                       // has_committed
                        true,                       // has_server_redirect
                        nav_map->at(GURL("http://redirect/1")).at(0));
}

}  // namespace safe_browsing
