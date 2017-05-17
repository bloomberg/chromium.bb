// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/browser_list_router_helper.h"

#include "base/stl_util.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/sync_sessions/synced_tab_delegate.h"

namespace sync_sessions {

class MockLocalSessionEventHandler : public LocalSessionEventHandler {
 public:
  void OnLocalTabModified(SyncedTabDelegate* modified_tab) override {
    seen_urls_.push_back(modified_tab->GetVirtualURLAtIndex(
        modified_tab->GetCurrentEntryIndex()));
  }

  void OnFaviconsChanged(const std::set<GURL>& page_urls,
                         const GURL& icon_url) override {}

  std::vector<GURL>* seen_urls() { return &seen_urls_; }

 private:
  std::vector<GURL> seen_urls_;
};

class BrowserListRouterHelperTest : public BrowserWithTestWindowTest {
 protected:
  ~BrowserListRouterHelperTest() override {}

  MockLocalSessionEventHandler handler_1;
  MockLocalSessionEventHandler handler_2;
};

TEST_F(BrowserListRouterHelperTest, ObservationScopedToSingleProfile) {
  TestingProfile* profile_1 = profile();
  std::unique_ptr<TestingProfile> profile_2(
      BrowserWithTestWindowTest::CreateProfile());

  std::unique_ptr<BrowserWindow> window_2(CreateBrowserWindow());
  std::unique_ptr<Browser> browser_2(
      CreateBrowser(profile_2.get(), browser()->type(), false, window_2.get()));

  SyncSessionsWebContentsRouter* router_1 =
      SyncSessionsWebContentsRouterFactory::GetInstance()->GetForProfile(
          profile_1);
  SyncSessionsWebContentsRouter* router_2 =
      SyncSessionsWebContentsRouterFactory::GetInstance()->GetForProfile(
          profile_2.get());

  router_1->StartRoutingTo(&handler_1);
  router_2->StartRoutingTo(&handler_2);

  GURL gurl_1("http://foo1.com");
  GURL gurl_2("http://foo2.com");
  AddTab(browser(), gurl_1);
  AddTab(browser_2.get(), gurl_2);

  std::vector<GURL>* handler_1_urls = handler_1.seen_urls();
  EXPECT_TRUE(base::ContainsValue(*handler_1_urls, gurl_1));
  EXPECT_FALSE(base::ContainsValue(*handler_1_urls, gurl_2));

  std::vector<GURL>* handler_2_urls = handler_2.seen_urls();
  EXPECT_TRUE(base::ContainsValue(*handler_2_urls, gurl_2));
  EXPECT_FALSE(base::ContainsValue(*handler_2_urls, gurl_1));

  // Add a browser for each profile.
  std::unique_ptr<BrowserWindow> window_3(CreateBrowserWindow());
  std::unique_ptr<BrowserWindow> window_4(CreateBrowserWindow());

  std::unique_ptr<Browser> new_browser_in_first_profile(
      CreateBrowser(profile_1, browser()->type(), false, window_3.get()));
  std::unique_ptr<Browser> new_browser_in_second_profile(
      CreateBrowser(profile_2.get(), browser()->type(), false, window_4.get()));

  GURL gurl_3("http://foo3.com");
  GURL gurl_4("http://foo4.com");
  AddTab(new_browser_in_first_profile.get(), gurl_3);
  AddTab(new_browser_in_second_profile.get(), gurl_4);

  handler_1_urls = handler_1.seen_urls();
  EXPECT_TRUE(base::ContainsValue(*handler_1_urls, gurl_3));
  EXPECT_FALSE(base::ContainsValue(*handler_1_urls, gurl_4));

  handler_2_urls = handler_2.seen_urls();
  EXPECT_TRUE(base::ContainsValue(*handler_2_urls, gurl_4));
  EXPECT_FALSE(base::ContainsValue(*handler_2_urls, gurl_3));

  // Cleanup needed for manually created browsers so they don't complain about
  // having open tabs when destructing.
  browser_2->tab_strip_model()->CloseAllTabs();
  new_browser_in_first_profile->tab_strip_model()->CloseAllTabs();
  new_browser_in_second_profile->tab_strip_model()->CloseAllTabs();
}

}  // namespace sync_sessions
