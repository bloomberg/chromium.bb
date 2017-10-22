// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"
#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class TabUnderBlockerBrowserTest : public InProcessBrowserTest {
 public:
  TabUnderBlockerBrowserTest() {}
  ~TabUnderBlockerBrowserTest() override {}

  void SetUpOnMainThread() override {
    scoped_feature_list_.InitAndEnableFeature(
        TabUnderNavigationThrottle::kBlockTabUnders);
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  static std::string GetError(const GURL& blocked_url) {
    return base::StringPrintf(kBlockTabUnderFormatMessage,
                              blocked_url.spec().c_str());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(TabUnderBlockerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(TabUnderBlockerBrowserTest, SimpleTabUnder_IsBlocked) {
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();

  content::WebContents* opener =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(opener, "window.open('/title1.html')"));
  navigation_observer.Wait();

  EXPECT_TRUE(PopupOpenerTabHelper::FromWebContents(opener)
                  ->has_opened_popup_since_last_user_gesture());
  EXPECT_FALSE(opener->IsVisible());

  content::TestNavigationObserver tab_under_observer(opener, 1);
  const GURL cross_origin_url =
      embedded_test_server()->GetURL("a.com", "/title1.html");

  std::string expected_error = GetError(cross_origin_url);
  content::ConsoleObserverDelegate console_observer(opener, expected_error);
  opener->SetDelegate(&console_observer);

  EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(
      opener, base::StringPrintf("window.location = '%s';",
                                 cross_origin_url.spec().c_str())));
  tab_under_observer.Wait();
  EXPECT_FALSE(tab_under_observer.last_navigation_succeeded());

  // Round trip to the renderer to ensure the message was sent. Don't use Wait()
  // to be consistent with the NotBlocked test below, which has to use this
  // technique.
  EXPECT_TRUE(content::ExecuteScript(opener, "var a = 0;"));
  EXPECT_EQ(expected_error, console_observer.message());
}

IN_PROC_BROWSER_TEST_F(TabUnderBlockerBrowserTest,
                       RedirectAfterGesture_IsNotBlocked) {
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();

  content::WebContents* opener =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(opener, "window.open('/title1.html')"));
  navigation_observer.Wait();

  EXPECT_TRUE(PopupOpenerTabHelper::FromWebContents(opener)
                  ->has_opened_popup_since_last_user_gesture());
  EXPECT_FALSE(opener->IsVisible());

  // Perform some user gesture on the page.
  content::SimulateMouseClick(opener, 0, blink::WebMouseEvent::Button::kLeft);

  content::TestNavigationObserver tab_under_observer(opener, 1);
  const GURL cross_origin_url =
      embedded_test_server()->GetURL("a.com", "/title1.html");
  content::ConsoleObserverDelegate console_observer(opener,
                                                    GetError(cross_origin_url));
  opener->SetDelegate(&console_observer);
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(
      opener, base::StringPrintf("window.location = '%s';",
                                 cross_origin_url.spec().c_str())));
  tab_under_observer.Wait();
  EXPECT_TRUE(tab_under_observer.last_navigation_succeeded());

  // Round trip to the renderer to ensure the message would have been sent.
  EXPECT_TRUE(content::ExecuteScript(opener, "var a = 0;"));
  EXPECT_TRUE(console_observer.message().empty());
}
