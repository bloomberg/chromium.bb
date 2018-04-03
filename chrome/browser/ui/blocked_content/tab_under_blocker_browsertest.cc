// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"
#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#endif

class TabUnderBlockerBrowserTest : public InProcessBrowserTest {
 public:
  TabUnderBlockerBrowserTest() {
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  ~TabUnderBlockerBrowserTest() override {}

  void SetUpOnMainThread() override {
    scoped_feature_list_.InitAndEnableFeature(
        TabUnderNavigationThrottle::kBlockTabUnders);
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void UpdatePolicy(bool enable_protection) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kTabUnderProtectionEnabled,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(enable_protection),
               nullptr /* external_data_fetcher */);
    provider_.UpdateChromePolicy(policy);
  }

  static std::string GetError(const GURL& blocked_url) {
    return base::StringPrintf(kBlockTabUnderFormatMessage,
                              blocked_url.spec().c_str());
  }

  bool IsUiShownForUrl(content::WebContents* web_contents, const GURL& url) {
// TODO(csharrison): Implement android checking when crbug.com/611756 is
// resolved.
#if defined(OS_ANDROID)
    return false;
#else
    return base::ContainsValue(
        FramebustBlockTabHelper::FromWebContents(web_contents)->blocked_urls(),
        url);
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  policy::MockConfigurationPolicyProvider provider_;

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
  EXPECT_EQ(opener->GetVisibility(), content::Visibility::HIDDEN);

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
  EXPECT_TRUE(IsUiShownForUrl(opener, cross_origin_url));
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
  EXPECT_EQ(opener->GetVisibility(), content::Visibility::HIDDEN);

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
  EXPECT_FALSE(IsUiShownForUrl(opener, cross_origin_url));
}

IN_PROC_BROWSER_TEST_F(TabUnderBlockerBrowserTest,
                       SpoofCtrlClickTabUnder_IsBlocked) {
  content::WebContents* opener =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/links.html"));
  const std::string cross_origin_url =
      embedded_test_server()->GetURL("a.com", "/title1.html").spec();

  const std::string script =
      "var evt = new MouseEvent('click', {"
      "  view : window,"
#if defined(OS_MACOSX)
      "  metaKey : true"
#else
      "  ctrlKey : true"
#endif
      "});"
      "document.getElementById('title1').dispatchEvent(evt);"
      "window.location = '%s';";

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  content::TestNavigationObserver tab_under_observer(opener, 1);
  navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScript(
      opener,
      base::StringPrintf(script.c_str(), cross_origin_url.c_str()).c_str()));
  navigation_observer.Wait();
  tab_under_observer.Wait();
  EXPECT_FALSE(tab_under_observer.last_navigation_succeeded());
}

IN_PROC_BROWSER_TEST_F(TabUnderBlockerBrowserTest,
                       ControlledByEnterprisePolicy) {
  // Disable the enterprise policy, should disable tab-under blocking.
  UpdatePolicy(false /* enable_protection */);
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  content::WebContents* opener =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScript(opener, "window.open('/title1.html')"));
  navigation_observer.Wait();

  // First tab-under attempt should succeed.
  {
    content::TestNavigationObserver tab_under_observer(opener, 1);
    const GURL cross_origin_url =
        embedded_test_server()->GetURL("a.com", "/title1.html");
    EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(
        opener, base::StringPrintf("window.location = '%s';",
                                   cross_origin_url.spec().c_str())));
    tab_under_observer.Wait();

    EXPECT_TRUE(tab_under_observer.last_navigation_succeeded());
    EXPECT_FALSE(IsUiShownForUrl(opener, cross_origin_url));
  }

  // Enable the policy and try to tab-under again in the background tab. Should
  // fail to navigate.
  {
    UpdatePolicy(true /* enable_protection */);
    content::TestNavigationObserver tab_under_observer(opener, 1);
    const GURL cross_origin_url =
        embedded_test_server()->GetURL("b.com", "/title1.html");
    EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(
        opener, base::StringPrintf("window.location = '%s';",
                                   cross_origin_url.spec().c_str())));
    tab_under_observer.Wait();

    EXPECT_FALSE(tab_under_observer.last_navigation_succeeded());
    EXPECT_TRUE(IsUiShownForUrl(opener, cross_origin_url));
  }
}
