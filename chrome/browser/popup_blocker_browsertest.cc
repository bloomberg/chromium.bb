// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

namespace {

static const base::FilePath::CharType* kTestDir =
    FILE_PATH_LITERAL("popup_blocker");

class PopupBlockerBrowserTest : public InProcessBrowserTest {
 public:
  PopupBlockerBrowserTest() {}

  // Returns a url that shows one popup.
  GURL GetTestURL() {
    return ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("popup-blocked-to-post-blank.html")));
  }

  std::vector<WebContents*> GetBlockedContents(Browser* browser) {
    // Do a round trip to the renderer first to flush any in-flight IPCs to
    // create a to-be-blocked window.
    WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
    CHECK(content::ExecuteScript(tab, ""));
    BlockedContentTabHelper* blocked_content_tab_helper =
        BlockedContentTabHelper::FromWebContents(tab);
    std::vector<WebContents*> blocked_contents;
    blocked_content_tab_helper->GetBlockedContents(&blocked_contents);
    return blocked_contents;
  }

  void NavigateAndCheckPopupShown(Browser* browser, const GURL& url) {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
    ui_test_utils::NavigateToURL(browser, url);
    observer.Wait();

    ASSERT_EQ(2u, chrome::GetBrowserCount(browser->profile()));

    std::vector<WebContents*> blocked_contents = GetBlockedContents(browser);
    ASSERT_TRUE(blocked_contents.empty());
  }

  void BasicTest(Browser* browser) {
    GURL url(GetTestURL());
    ui_test_utils::NavigateToURL(browser, url);

    // If the popup blocker blocked the blank post, there should be only one
    // tab in only one browser window and the URL of current tab must be equal
    // to the original URL.
    EXPECT_EQ(1u, chrome::GetBrowserCount(browser->profile()));
    EXPECT_EQ(1, browser->tab_strip_model()->count());
    WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(url, web_contents->GetURL());

    std::vector<WebContents*> blocked_contents = GetBlockedContents(browser);
    ASSERT_EQ(1u, blocked_contents.size());

    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());

    BlockedContentTabHelper* blocked_content_tab_helper =
        BlockedContentTabHelper::FromWebContents(web_contents);
    blocked_content_tab_helper->LaunchForContents(blocked_contents[0]);

    observer.Wait();
  }
};

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, PopupBlockedPostBlank) {
  BasicTest(browser());
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       PopupBlockedPostBlankIncognito) {
  BasicTest(CreateIncognitoBrowser());
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, MultiplePopups) {
  GURL url(ui_test_utils::GetTestUrl(base::FilePath(
      kTestDir), base::FilePath(FILE_PATH_LITERAL("popup-many.html"))));
  ui_test_utils::NavigateToURL(browser(), url);
  std::vector<WebContents*> blocked_contents = GetBlockedContents(browser());
  ASSERT_EQ(2u, blocked_contents.size());
}

// Verify that popups are launched on browser back button.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       AllowPopupThroughContentSetting) {
  GURL url(GetTestURL());
  browser()->profile()->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURL(url),
      ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_POPUPS,
      "",
      CONTENT_SETTING_ALLOW);

  NavigateAndCheckPopupShown(browser(), url);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, PopupsLaunchWhenTabIsClosed) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("popup-on-unload.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  NavigateAndCheckPopupShown(browser(), GURL(chrome::kAboutBlankURL));
}

// Verify that when you unblock popup, the popup shows in history and omnibox.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       UnblockedPopupShowsInHistoryAndOmnibox) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);
  GURL url(GetTestURL());
  NavigateAndCheckPopupShown(browser(), url);

  std::string search_string =
      "data:text/html,<title>Popup Success!</title>you should not see this "
      "message if popup blocker is enabled";

  ui_test_utils::HistoryEnumerator history(browser()->profile());
  std::vector<GURL>& history_urls = history.urls();
  ASSERT_EQ(2u, history_urls.size());
  ASSERT_EQ(GURL(search_string), history_urls[0]);
  ASSERT_EQ(url, history_urls[1]);

  TemplateURLService* service = TemplateURLServiceFactory::GetForProfile(
      browser()->profile());
  ui_test_utils::WaitForTemplateURLServiceToLoad(service);
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  ui_test_utils::SendToOmniboxAndSubmit(location_bar, search_string);
  OmniboxEditModel* model = location_bar->GetLocationEntry()->model();
  EXPECT_EQ(GURL(search_string), model->CurrentMatch().destination_url);
  EXPECT_EQ(ASCIIToUTF16(search_string), model->CurrentMatch().contents);
}

}  // namespace
