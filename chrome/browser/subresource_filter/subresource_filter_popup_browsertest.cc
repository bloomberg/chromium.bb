// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

base::LazyInstance<std::vector<std::string>>::DestructorAtExit error_messages_ =
    LAZY_INSTANCE_INITIALIZER;

// Intercepts all console messages. Only used when the ConsoleObserverDelegate
// cannot be (e.g. when we need the standard delegate).
bool LogHandler(int severity,
                const char* file,
                int line,
                size_t message_start,
                const std::string& str) {
  if (file && std::string("CONSOLE") == file)
    error_messages_.Get().push_back(str);
  return false;
}

}  // namespace

const char kSubresourceFilterActionsHistogram[] = "SubresourceFilter.Actions";

// Tests for the subresource_filter popup blocker.
class SubresourceFilterPopupBrowserTest : public SubresourceFilterBrowserTest {
  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    Configuration config = Configuration::MakePresetForLiveRunOnPhishingSites();
    config.activation_options.should_strengthen_popup_blocker = true;
    ResetConfiguration(std::move(config));
    // Only necessary so we have a valid ruleset.
    ASSERT_NO_FATAL_FAILURE(SetRulesetWithRules(std::vector<proto::UrlRule>()));
  }
};

IN_PROC_BROWSER_TEST_F(SubresourceFilterPopupBrowserTest,
                       NoConfiguration_AllowCreatingNewWindows) {
  ResetConfiguration(Configuration::MakePresetForLiveRunOnPhishingSites());
  base::HistogramTester tester;
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  // Only configure |a_url| as a phishing URL.
  ConfigureAsPhishingURL(a_url);

  // Navigate to a_url, should not trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool opened_window = false;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &opened_window));
  EXPECT_TRUE(opened_window);
  EXPECT_FALSE(TabSpecificContentSettings::FromWebContents(web_contents)
                   ->IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
  tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                           kActionPopupBlocked, 0);

  // Navigate again to trigger histogram logging. Make sure the navigation
  // happens in the original WebContents.
  browser()->tab_strip_model()->ToggleSelectionAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(web_contents));
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  tester.ExpectUniqueSample("SubresourceFilter.PageLoad.BlockedPopups", 0, 1);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterPopupBrowserTest,
                       BlockCreatingNewWindows) {
  base::HistogramTester tester;
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  GURL b_url(embedded_test_server()->GetURL("b.com", kWindowOpenPath));
  // Only configure |a_url| as a phishing URL.
  ConfigureAsPhishingURL(a_url);

  // Navigate to a_url, should trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool opened_window = false;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &opened_window));
  EXPECT_FALSE(opened_window);
  tester.ExpectBucketCount(kSubresourceFilterActionsHistogram, kActionUIShown,
                           0);
  tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                           kActionPopupBlocked, 1);
  // Make sure the popup UI was shown.
  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)
                  ->IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));

  // Block again.
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &opened_window));
  EXPECT_FALSE(opened_window);

  // Navigate to |b_url|, which should successfully open the popup.
  ui_test_utils::NavigateToURL(browser(), b_url);
  tester.ExpectUniqueSample("SubresourceFilter.PageLoad.BlockedPopups", 2, 1);
  opened_window = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &opened_window));
  EXPECT_TRUE(opened_window);
  // Popup UI should not be shown.
  EXPECT_FALSE(TabSpecificContentSettings::FromWebContents(web_contents)
                   ->IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterPopupBrowserTest,
                       BlockCreatingNewWindows_LogsToConsole) {
  content::ConsoleObserverDelegate console_observer(web_contents(),
                                                    kDisallowNewWindowMessage);
  web_contents()->SetDelegate(&console_observer);
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  ConfigureAsPhishingURL(a_url);

  // Navigate to a_url, should trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool opened_window = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(), "openWindow()", &opened_window));
  EXPECT_FALSE(opened_window);
  console_observer.Wait();
  EXPECT_EQ(kDisallowNewWindowMessage, console_observer.message());
}

// Whitelisted sites should not have console logging.
IN_PROC_BROWSER_TEST_F(SubresourceFilterPopupBrowserTest,
                       AllowCreatingNewWindows_NoLogToConsole) {
  logging::SetLogMessageHandler(&LogHandler);
  const char kWindowOpenPath[] = "/subresource_filter/window_open.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  ConfigureAsPhishingURL(a_url);

  // Allow popups on |a_url|.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetContentSettingDefaultScope(
      a_url, a_url, ContentSettingsType::CONTENT_SETTINGS_TYPE_POPUPS,
      std::string(), CONTENT_SETTING_ALLOW);

  // Navigate to a_url, should not trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool opened_window = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(), "openWindow()", &opened_window));
  EXPECT_TRUE(opened_window);
  // Round trip to the renderer to ensure the message would have gotten sent.
  EXPECT_TRUE(content::ExecuteScript(web_contents(), "var a = 1;"));
  bool has_activation_ipc = false;
  for (const auto& message : error_messages_.Get()) {
    has_activation_ipc |=
        (message.find(kActivationConsoleMessage) != std::string::npos);
    EXPECT_EQ(std::string::npos, message.find(kDisallowNewWindowMessage));
  }
  EXPECT_TRUE(has_activation_ipc);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterPopupBrowserTest, BlockOpenURLFromTab) {
  base::HistogramTester tester;
  const char kWindowOpenPath[] =
      "/subresource_filter/window_open_spoof_click.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", kWindowOpenPath));
  GURL b_url(embedded_test_server()->GetURL("b.com", kWindowOpenPath));
  // Only configure |a_url| as a phishing URL.
  ConfigureAsPhishingURL(a_url);

  // Navigate to a_url, should trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(web_contents, "openWindow()"));
  tester.ExpectBucketCount(kSubresourceFilterActionsHistogram, kActionUIShown,
                           0);
  tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                           kActionPopupBlocked, 1);

  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)
                  ->IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));

  // Navigate to |b_url|, which should successfully open the popup.

  ui_test_utils::NavigateToURL(browser(), b_url);

  content::TestNavigationObserver navigation_observer(nullptr, 1);
  navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScript(web_contents, "openWindow()"));
  navigation_observer.Wait();

  // Popup UI should not be shown.
  EXPECT_FALSE(TabSpecificContentSettings::FromWebContents(web_contents)
                   ->IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterPopupBrowserTest,
                       BlockOpenURLFromTabInIframe) {
  const char popup_path[] = "/subresource_filter/iframe_spoof_click_popup.html";
  GURL a_url(embedded_test_server()->GetURL("a.com", popup_path));
  // Only configure |a_url| as a phishing URL.
  ConfigureAsPhishingURL(a_url);

  // Navigate to a_url, should not trigger the popup blocker.
  ui_test_utils::NavigateToURL(browser(), a_url);
  bool sent_open = false;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "openWindow()",
                                                   &sent_open));
  EXPECT_TRUE(sent_open);
  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)
                  ->IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterPopupBrowserTest,
                       TraditionalWindowOpen_NotBlocked) {
  GURL url(GetTestUrl("/title2.html"));
  ConfigureAsPhishingURL(url);
  ui_test_utils::NavigateToURL(browser(), GetTestUrl("/title1.html"));

  // Should not trigger the popup blocker because internally opens the tab with
  // a user gesture.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(TabSpecificContentSettings::FromWebContents(web_contents)
                   ->IsContentBlocked(CONTENT_SETTINGS_TYPE_POPUPS));
}

}  // namespace subresource_filter
