// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

namespace {

static const base::FilePath::CharType* kTestDir =
    FILE_PATH_LITERAL("popup_blocker");

// Counts the number of RenderViewHosts created.
class CountRenderViewHosts : public content::NotificationObserver {
 public:
  CountRenderViewHosts()
      : count_(0) {
    registrar_.Add(this,
                   content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
                   content::NotificationService::AllSources());
  }
  virtual ~CountRenderViewHosts() {}

  int GetRenderViewHostCreatedCount() const { return count_; }

 private:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    count_++;
  }

  content::NotificationRegistrar registrar_;

  int count_;

  DISALLOW_COPY_AND_ASSIGN(CountRenderViewHosts);
};

class PopupBlockerBrowserTest : public InProcessBrowserTest {
 public:
  PopupBlockerBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableBetterPopupBlocking);
  }

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
    CHECK(content::ExecuteScript(tab, std::string()));
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

    ASSERT_EQ(2u, chrome::GetBrowserCount(browser->profile(),
                                          browser->host_desktop_type()));

    std::vector<WebContents*> blocked_contents = GetBlockedContents(browser);
    ASSERT_TRUE(blocked_contents.empty());
  }

  void BasicTest(Browser* browser, const GURL& url) {
    ui_test_utils::NavigateToURL(browser, url);

    // If the popup blocker blocked the blank post, there should be only one
    // tab in only one browser window and the URL of current tab must be equal
    // to the original URL.
    EXPECT_EQ(1u, chrome::GetBrowserCount(browser->profile(),
                                          browser->host_desktop_type()));
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
  BasicTest(browser(), GetTestURL());
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       PopupBlockedPostBlankIncognito) {
  BasicTest(CreateIncognitoBrowser(), GetTestURL());
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       PopupBlockedFakeClickOnAnchor) {
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("popup-fake-click-on-anchor.html"))));
  BasicTest(browser(), url);
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
  browser()->profile()->GetHostContentSettingsMap()
      ->SetContentSetting(ContentSettingsPattern::FromURL(url),
                          ContentSettingsPattern::Wildcard(),
                          CONTENT_SETTINGS_TYPE_POPUPS,
                          std::string(),
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

  NavigateAndCheckPopupShown(browser(), GURL(content::kAboutBlankURL));
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
  EXPECT_EQ(GURL(search_string), model->CurrentMatch(NULL).destination_url);
  EXPECT_EQ(ASCIIToUTF16(search_string), model->CurrentMatch(NULL).contents);
}

class BetterPopupBlockerBrowserTest : public PopupBlockerBrowserTest {
 public:
  BetterPopupBlockerBrowserTest() {}
  virtual ~BetterPopupBlockerBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  // Navigates to the test indicated by |test_name| which is expected to try to
  // open a popup. Verifies that the popup was blocked and then opens the
  // blocked popup. Once the popup stopped loading, verifies that the title of
  // the page is "PASS".
  //
  // If |expect_new_browser| is true, the popup is expected to open a new
  // window, or a background tab if it is false.
  void RunCheckTest(const base::FilePath& test_name, bool expect_new_browser) {
    GURL url(ui_test_utils::GetTestUrl(base::FilePath(kTestDir), test_name));

    CountRenderViewHosts counter;

    ui_test_utils::NavigateToURL(browser(), url);

    // Since the popup blocker blocked the window.open, there should be only one
    // tab.
    EXPECT_EQ(1u, chrome::GetBrowserCount(browser()->profile(),
                                          browser()->host_desktop_type()));
    EXPECT_EQ(1, browser()->tab_strip_model()->count());
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(url, web_contents->GetURL());

    // And no new RVH created.
    EXPECT_EQ(0, counter.GetRenderViewHostCreatedCount());

    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
    ui_test_utils::BrowserAddedObserver browser_observer;

    // Launch the blocked popup.
    PopupBlockerTabHelper* popup_blocker_helper =
        PopupBlockerTabHelper::FromWebContents(web_contents);
    EXPECT_EQ(1u, popup_blocker_helper->GetBlockedPopupsCount());
    std::map<int32, GURL> blocked_requests =
        popup_blocker_helper->GetBlockedPopupRequests();
    std::map<int32, GURL>::const_iterator iter = blocked_requests.begin();
    popup_blocker_helper->ShowBlockedPopup(iter->first);

    observer.Wait();
    Browser* new_browser;
    if (expect_new_browser) {
      new_browser  = browser_observer.WaitForSingleNewBrowser();
      web_contents = new_browser->tab_strip_model()->GetActiveWebContents();
    } else {
      new_browser = browser();
      EXPECT_EQ(2, browser()->tab_strip_model()->count());
      web_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
    }

    // Check that the check passed.
    base::string16 expected_title(base::ASCIIToUTF16("PASS"));
    content::TitleWatcher title_watcher(web_contents, expected_title);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BetterPopupBlockerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(BetterPopupBlockerBrowserTest,
                       BlockWebContentsCreation) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  CountRenderViewHosts counter;

  ui_test_utils::NavigateToURL(browser(), GetTestURL());

  // Wait until the request actually has hit the popup blocker. The
  // NavigateToURL call above returns as soon as the main tab stopped loading
  // which can happen before the popup request was processed.
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  PopupBlockerTabHelper* popup_blocker_helper =
      PopupBlockerTabHelper::FromWebContents(web_contents);
  if (!popup_blocker_helper->GetBlockedPopupsCount()) {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
        content::NotificationService::AllSources());
    observer.Wait();
  }

  // If the popup blocker blocked the blank post, there should be only one tab.
  EXPECT_EQ(1u, chrome::GetBrowserCount(browser()->profile(),
                                        browser()->host_desktop_type()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetTestURL(), web_contents->GetURL());

  // And no new RVH created.
  EXPECT_EQ(0, counter.GetRenderViewHostCreatedCount());

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());

  // Launch the blocked popup.
  EXPECT_EQ(1u, popup_blocker_helper->GetBlockedPopupsCount());
  std::map<int32, GURL> blocked_requests =
      popup_blocker_helper->GetBlockedPopupRequests();
  std::map<int32, GURL>::const_iterator iter = blocked_requests.begin();
  popup_blocker_helper->ShowBlockedPopup(iter->first);

  observer.Wait();
}

IN_PROC_BROWSER_TEST_F(BetterPopupBlockerBrowserTest,
                       PopupBlockedFakeClickOnAnchorNoTarget) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("popup-fake-click-on-anchor2.html"))));

  CountRenderViewHosts counter;

  ui_test_utils::NavigateToURL(browser(), url);

  // If the popup blocker blocked the blank post, there should be only one tab.
  EXPECT_EQ(1u, chrome::GetBrowserCount(browser()->profile(),
                                        browser()->host_desktop_type()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, web_contents->GetURL());

  // And no new RVH created.
  EXPECT_EQ(0, counter.GetRenderViewHostCreatedCount());

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());

  // Launch the blocked popup.
  PopupBlockerTabHelper* popup_blocker_helper =
      PopupBlockerTabHelper::FromWebContents(web_contents);
  EXPECT_EQ(1u, popup_blocker_helper->GetBlockedPopupsCount());
  std::map<int32, GURL> blocked_requests =
      popup_blocker_helper->GetBlockedPopupRequests();
  std::map<int32, GURL>::const_iterator iter = blocked_requests.begin();
  popup_blocker_helper->ShowBlockedPopup(iter->first);

  observer.Wait();
}

IN_PROC_BROWSER_TEST_F(BetterPopupBlockerBrowserTest, WindowFeatures) {
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("popup-window-open.html"))));

  CountRenderViewHosts counter;

  ui_test_utils::NavigateToURL(browser(), url);

  // If the popup blocker blocked the blank post, there should be only one tab.
  EXPECT_EQ(1u, chrome::GetBrowserCount(browser()->profile(),
                                        browser()->host_desktop_type()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, web_contents->GetURL());

  // And no new RVH created.
  EXPECT_EQ(0, counter.GetRenderViewHostCreatedCount());

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  ui_test_utils::BrowserAddedObserver browser_observer;

  // Launch the blocked popup.
  PopupBlockerTabHelper* popup_blocker_helper =
      PopupBlockerTabHelper::FromWebContents(web_contents);
  EXPECT_EQ(1u, popup_blocker_helper->GetBlockedPopupsCount());
  std::map<int32, GURL> blocked_requests =
      popup_blocker_helper->GetBlockedPopupRequests();
  std::map<int32, GURL>::const_iterator iter = blocked_requests.begin();
  popup_blocker_helper->ShowBlockedPopup(iter->first);

  observer.Wait();
  Browser* new_browser = browser_observer.WaitForSingleNewBrowser();

  // Check that the new popup has (roughly) the requested size.
  web_contents = new_browser->tab_strip_model()->GetActiveWebContents();
  gfx::Size window_size = web_contents->GetView()->GetContainerSize();
  EXPECT_TRUE(349 <= window_size.width() && window_size.width() <= 351);
  EXPECT_TRUE(249 <= window_size.height() && window_size.height() <= 251);
}

IN_PROC_BROWSER_TEST_F(BetterPopupBlockerBrowserTest, CorrectReferrer) {
  RunCheckTest(base::FilePath(FILE_PATH_LITERAL("popup-referrer.html")), true);
}

IN_PROC_BROWSER_TEST_F(BetterPopupBlockerBrowserTest, WindowFeaturesBarProps) {
  RunCheckTest(base::FilePath(FILE_PATH_LITERAL("popup-windowfeatures.html")),
               true);
}

IN_PROC_BROWSER_TEST_F(BetterPopupBlockerBrowserTest, SessionStorage) {
  RunCheckTest(base::FilePath(FILE_PATH_LITERAL("popup-sessionstorage.html")),
               true);
}

IN_PROC_BROWSER_TEST_F(BetterPopupBlockerBrowserTest, Opener) {
  RunCheckTest(base::FilePath(FILE_PATH_LITERAL("popup-opener.html")), true);
}

IN_PROC_BROWSER_TEST_F(BetterPopupBlockerBrowserTest, OpenerSuppressed) {
  RunCheckTest(
      base::FilePath(FILE_PATH_LITERAL("popup-openersuppressed.html")), false);
}

IN_PROC_BROWSER_TEST_F(BetterPopupBlockerBrowserTest, ShiftClick) {
  RunCheckTest(
      base::FilePath(FILE_PATH_LITERAL("popup-fake-click-on-anchor3.html")),
      true);
}

IN_PROC_BROWSER_TEST_F(BetterPopupBlockerBrowserTest, WebUI) {
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("popup-webui.html"))));

  CountRenderViewHosts counter;

  ui_test_utils::NavigateToURL(browser(), url);

  // If the popup blocker blocked the blank post, there should be only one tab.
  EXPECT_EQ(1u, chrome::GetBrowserCount(browser()->profile(),
                                        browser()->host_desktop_type()));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, web_contents->GetURL());

  // And no new RVH created.
  EXPECT_EQ(0, counter.GetRenderViewHostCreatedCount());

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  ui_test_utils::BrowserAddedObserver browser_observer;

  // Launch the blocked popup.
  PopupBlockerTabHelper* popup_blocker_helper =
      PopupBlockerTabHelper::FromWebContents(web_contents);
  EXPECT_EQ(1u, popup_blocker_helper->GetBlockedPopupsCount());
  std::map<int32, GURL> blocked_requests =
      popup_blocker_helper->GetBlockedPopupRequests();
  std::map<int32, GURL>::const_iterator iter = blocked_requests.begin();
  popup_blocker_helper->ShowBlockedPopup(iter->first);

  observer.Wait();
  Browser* new_browser = browser_observer.WaitForSingleNewBrowser();

  // Check that the new popup displays about:blank.
  web_contents = new_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(GURL(content::kAboutBlankURL), web_contents->GetURL());
}

}  // namespace
