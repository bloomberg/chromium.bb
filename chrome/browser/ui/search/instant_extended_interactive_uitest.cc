// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/instant_uitest_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class InstantExtendedTest : public InProcessBrowserTest,
                            public InstantUITestBase {
 public:
  InstantExtendedTest()
      : on_most_visited_change_calls_(0),
        most_visited_items_count_(0),
        first_most_visited_item_id_(0),
        on_focus_changed_calls_(0),
        is_focused_(false) {}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(https_test_server().Start());
    GURL base_url = https_test_server().GetURL("/instant_extended.html?");
    GURL ntp_url = https_test_server().GetURL("/instant_extended_ntp.html?");
    InstantTestBase::Init(base_url, ntp_url, false);
  }

  bool UpdateSearchState(content::WebContents* contents) WARN_UNUSED_RESULT {
    return instant_test_utils::GetIntFromJS(contents,
                                            "onMostVisitedChangedCalls",
                                            &on_most_visited_change_calls_) &&
           instant_test_utils::GetIntFromJS(contents, "mostVisitedItemsCount",
                                            &most_visited_items_count_) &&
           instant_test_utils::GetIntFromJS(contents, "firstMostVisitedItemId",
                                            &first_most_visited_item_id_) &&
           instant_test_utils::GetIntFromJS(contents, "onFocusChangedCalls",
                                            &on_focus_changed_calls_) &&
           instant_test_utils::GetBoolFromJS(contents, "isFocused",
                                             &is_focused_);
  }

  int on_most_visited_change_calls_;
  int most_visited_items_count_;
  int first_most_visited_item_id_;
  int on_focus_changed_calls_;
  bool is_focused_;
};

// TODO(treib): There's no reason for this to be in interactive_ui_tests. Move
// it to regular browser_tests instead.
class InstantThemeTest : public ExtensionBrowserTest, public InstantTestBase {
 public:
  InstantThemeTest() {}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(https_test_server().Start());
    GURL base_url = https_test_server().GetURL("/instant_extended.html");
    GURL ntp_url = https_test_server().GetURL("/instant_extended_ntp.html");
    InstantTestBase::Init(base_url, ntp_url, false);
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    content::URLDataSource::Add(profile(), new ThemeSource(profile()));
  }

  void InstallThemeAndVerify(const std::string& theme_dir,
                             const std::string& theme_name) {
    bool had_previous_theme =
        !!ThemeServiceFactory::GetThemeForProfile(profile());

    const base::FilePath theme_path = test_data_dir_.AppendASCII(theme_dir);
    // Themes install asynchronously so we must check the number of enabled
    // extensions after theme install completes.
    size_t num_before = extensions::ExtensionRegistry::Get(profile())
                            ->enabled_extensions()
                            .size();
    content::WindowedNotificationObserver theme_change_observer(
        chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
        content::Source<ThemeService>(
            ThemeServiceFactory::GetForProfile(profile())));
    ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(
        theme_path, 1, ExtensionBrowserTest::browser()));
    theme_change_observer.Wait();
    size_t num_after = extensions::ExtensionRegistry::Get(profile())
                           ->enabled_extensions()
                           .size();
    // If a theme was already installed, we're just swapping one for another, so
    // no change in extension count.
    int expected_change = had_previous_theme ? 0 : 1;
    EXPECT_EQ(num_before + expected_change, num_after);

    const extensions::Extension* new_theme =
        ThemeServiceFactory::GetThemeForProfile(profile());
    ASSERT_NE(nullptr, new_theme);
    ASSERT_EQ(new_theme->name(), theme_name);
  }

  // Loads a named image from |image_url| in the given |tab|. |loaded|
  // returns whether the image was able to load without error.
  // The method returns true if the JavaScript executed cleanly.
  bool LoadImage(content::WebContents* tab,
                 const GURL& image_url,
                 bool* loaded) {
    std::string js_chrome =
        "var img = document.createElement('img');"
        "img.onerror = function() { domAutomationController.send(false); };"
        "img.onload  = function() { domAutomationController.send(true); };"
        "img.src = '" +
        image_url.spec() + "';";
    return content::ExecuteScriptAndExtractBool(tab, js_chrome, loaded);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InstantThemeTest);
};

// Test to verify that switching tabs should not dispatch onmostvisitedchanged
// events.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NoMostVisitedChangedOnTabSwitch) {
  // Initialize Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Open new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Make sure new tab received the onmostvisitedchanged event once.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(UpdateSearchState(active_tab));
  EXPECT_EQ(1, on_most_visited_change_calls_);

  // Activate the previous tab.
  browser()->tab_strip_model()->ActivateTabAt(0, false);

  // Switch back to new tab.
  browser()->tab_strip_model()->ActivateTabAt(1, false);

  // Confirm that new tab got no onmostvisitedchanged event.
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(UpdateSearchState(active_tab));
  EXPECT_EQ(1, on_most_visited_change_calls_);
}

IN_PROC_BROWSER_TEST_F(InstantThemeTest, ThemeBackgroundAccess) {
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // The "Instant" New Tab should have access to chrome-search: scheme but not
  // chrome: scheme.
  const GURL chrome_url("chrome://theme/IDR_THEME_NTP_BACKGROUND");
  const GURL search_url("chrome-search://theme/IDR_THEME_NTP_BACKGROUND");
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  bool loaded = false;
  ASSERT_TRUE(LoadImage(tab, chrome_url, &loaded));
  EXPECT_FALSE(loaded) << chrome_url;
  ASSERT_TRUE(LoadImage(tab, search_url, &loaded));
  EXPECT_TRUE(loaded) << search_url;
}

// Flaky on all bots. http://crbug.com/335297.
IN_PROC_BROWSER_TEST_F(InstantThemeTest,
                       DISABLED_NoThemeBackgroundChangeEventOnTabSwitch) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Install a theme.
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Open new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Make sure the tab did not receive an onthemechange event for the
  // already-installed theme. (An event *is* sent, but that happens before the
  // page can register its handler.)
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  int on_theme_changed_calls = 0;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      active_tab, "onThemeChangedCalls", &on_theme_changed_calls));
  EXPECT_EQ(0, on_theme_changed_calls);

  // Activate the previous tab.
  browser()->tab_strip_model()->ActivateTabAt(0, false);
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  // Switch back to new tab.
  browser()->tab_strip_model()->ActivateTabAt(1, false);
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());

  // Confirm that new tab got no onthemechange event while switching tabs.
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  on_theme_changed_calls = 0;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      active_tab, "onThemeChangedCalls", &on_theme_changed_calls));
  EXPECT_EQ(0, on_theme_changed_calls);
}

// Flaky on all bots. http://crbug.com/335297, http://crbug.com/265971.
IN_PROC_BROWSER_TEST_F(InstantThemeTest,
                       DISABLED_SendThemeBackgroundChangedEvent) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Install a theme.
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Open new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Make sure the tab did not receive an onthemechange event for the
  // already-installed theme. (An event *is* sent, but that happens before the
  // page can register its handler.)
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  int on_theme_changed_calls = 0;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      active_tab, "onThemeChangedCalls", &on_theme_changed_calls));
  EXPECT_EQ(0, on_theme_changed_calls);

  // Install a different theme.
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme2", "snowflake theme"));

  // Confirm that the new tab got notified about the theme changed event.
  on_theme_changed_calls = 0;
  ASSERT_TRUE(instant_test_utils::GetIntFromJS(
      active_tab, "onThemeChangedCalls", &on_theme_changed_calls));
  EXPECT_EQ(1, on_theme_changed_calls);
}

// Flaky on all bots since re-enabled in r208032, crbug.com/253092
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_NavigateBackToNTP) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmnibox();

  // Open a new tab page.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  SetOmniboxText("flowers");
  PressEnterAndWaitForNavigation();

  // Navigate back to NTP.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(active_tab->GetController().CanGoBack());
  content::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &active_tab->GetController()));
  active_tab->GetController().GoBack();
  load_stop_observer.Wait();

  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(search::IsInstantNTP(active_tab));
}

// Flaky: crbug.com/267119
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       DISABLED_DispatchMVChangeEventWhileNavigatingBackToNTP) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmnibox();

  // Open new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(UpdateSearchState(active_tab));
  EXPECT_EQ(1, on_most_visited_change_calls_);

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  // Set the text and press enter to navigate from NTP.
  SetOmniboxText("Pen");
  PressEnterAndWaitForNavigation();
  observer.Wait();

  // Navigate back to NTP.
  content::WindowedNotificationObserver back_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(active_tab->GetController().CanGoBack());
  active_tab->GetController().GoBack();
  back_observer.Wait();

  // Verify that onmostvisitedchange event is dispatched when we navigate from
  // SRP to NTP.
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(UpdateSearchState(active_tab));
  EXPECT_EQ(1, on_most_visited_change_calls_);
}

// Check that clicking on a result sends the correct referrer.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, Referrer) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL result_url = embedded_test_server()->GetURL(
      "/referrer_policy/referrer-policy-log.html");
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmnibox();

  // Type a query and press enter to get results.
  SetOmniboxText("query");
  PressEnterAndWaitForFrameLoad();

  // Simulate going to a result.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::ostringstream stream;
  stream << "var link = document.createElement('a');";
  stream << "link.href = \"" << result_url.spec() << "\";";
  stream << "document.body.appendChild(link);";
  stream << "link.click();";
  EXPECT_TRUE(content::ExecuteScript(contents, stream.str()));

  content::WaitForLoadStop(contents);
  std::string expected_title =
      "Referrer is " + base_url().GetWithEmptyPath().spec();
  EXPECT_EQ(base::ASCIIToUTF16(expected_title), contents->GetTitle());
}
