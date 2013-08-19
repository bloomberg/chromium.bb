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
  virtual ~PopupBlockerBrowserTest() {}

  int GetBlockedContentsCount() {
    // Do a round trip to the renderer first to flush any in-flight IPCs to
    // create a to-be-blocked window.
    WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
    CHECK(content::ExecuteScript(tab, std::string()));
    PopupBlockerTabHelper* popup_blocker_helper =
        PopupBlockerTabHelper::FromWebContents(tab);
    return popup_blocker_helper->GetBlockedPopupsCount();
  }

  void NavigateAndCheckPopupShown(const GURL& url) {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
    ui_test_utils::NavigateToURL(browser(), url);
    observer.Wait();

    ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile(),
                                          browser()->host_desktop_type()));

    ASSERT_EQ(0, GetBlockedContentsCount());
  }

  // Navigates to the test indicated by |test_name| using |browser| which is
  // expected to try to open a popup. Verifies that the popup was blocked and
  // then opens the blocked popup. Once the popup stopped loading, verifies
  // that the title of the page is "PASS" if |check_title| is true.
  //
  // If |expect_new_browser| is true, the popup is expected to open a new
  // window, or a background tab if it is false.
  //
  // Returns the WebContents of the launched popup.
  WebContents* RunCheckTest(Browser* browser,
                            const base::FilePath& test_name,
                            bool expect_new_browser,
                            bool check_title) {
    GURL url(ui_test_utils::GetTestUrl(base::FilePath(kTestDir), test_name));

    CountRenderViewHosts counter;

    ui_test_utils::NavigateToURL(browser, url);

    // Since the popup blocker blocked the window.open, there should be only one
    // tab.
    EXPECT_EQ(1u, chrome::GetBrowserCount(browser->profile(),
                                          browser->host_desktop_type()));
    EXPECT_EQ(1, browser->tab_strip_model()->count());
    WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
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
    if (!popup_blocker_helper->GetBlockedPopupsCount()) {
      content::WindowedNotificationObserver observer(
          chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
          content::NotificationService::AllSources());
      observer.Wait();
    }
    EXPECT_EQ(1u, popup_blocker_helper->GetBlockedPopupsCount());
    std::map<int32, GURL> blocked_requests =
        popup_blocker_helper->GetBlockedPopupRequests();
    std::map<int32, GURL>::const_iterator iter = blocked_requests.begin();
    popup_blocker_helper->ShowBlockedPopup(iter->first);

    observer.Wait();
    Browser* new_browser;
    if (expect_new_browser) {
      new_browser = browser_observer.WaitForSingleNewBrowser();
      web_contents = new_browser->tab_strip_model()->GetActiveWebContents();
    } else {
      new_browser = browser;
      EXPECT_EQ(2, browser->tab_strip_model()->count());
      web_contents = browser->tab_strip_model()->GetWebContentsAt(1);
    }

    if (check_title) {
      // Check that the check passed.
      base::string16 expected_title(base::ASCIIToUTF16("PASS"));
      content::TitleWatcher title_watcher(web_contents, expected_title);
      EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
    }

    return web_contents;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PopupBlockerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       BlockWebContentsCreation) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  RunCheckTest(
      browser(),
      base::FilePath(FILE_PATH_LITERAL("popup-blocked-to-post-blank.html")),
      true,
      false);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       BlockWebContentsCreationIncognito) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  RunCheckTest(
      CreateIncognitoBrowser(),
      base::FilePath(FILE_PATH_LITERAL("popup-blocked-to-post-blank.html")),
      true,
      false);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       PopupBlockedFakeClickOnAnchor) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  RunCheckTest(
      browser(),
      base::FilePath(FILE_PATH_LITERAL("popup-fake-click-on-anchor.html")),
      false,
      false);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       PopupBlockedFakeClickOnAnchorNoTarget) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  RunCheckTest(
      browser(),
      base::FilePath(FILE_PATH_LITERAL("popup-fake-click-on-anchor2.html")),
      false,
      false);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, MultiplePopups) {
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("popup-many.html"))));
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_EQ(2, GetBlockedContentsCount());
}

// Verify that popups are launched on browser back button.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       AllowPopupThroughContentSetting) {
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("popup-blocked-to-post-blank.html"))));
  browser()->profile()->GetHostContentSettingsMap()
      ->SetContentSetting(ContentSettingsPattern::FromURL(url),
                          ContentSettingsPattern::Wildcard(),
                          CONTENT_SETTINGS_TYPE_POPUPS,
                          std::string(),
                          CONTENT_SETTING_ALLOW);

  NavigateAndCheckPopupShown(url);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       PopupsLaunchWhenTabIsClosed) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("popup-on-unload.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  NavigateAndCheckPopupShown(GURL(content::kAboutBlankURL));
}

// Verify that when you unblock popup, the popup shows in history and omnibox.
IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       UnblockedPopupShowsInHistoryAndOmnibox) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(kTestDir),
      base::FilePath(FILE_PATH_LITERAL("popup-blocked-to-post-blank.html"))));
  NavigateAndCheckPopupShown(url);

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

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, WindowFeatures) {
  WebContents* popup =
      RunCheckTest(browser(),
                   base::FilePath(FILE_PATH_LITERAL("popup-window-open.html")),
                   true,
                   false);

  // Check that the new popup has (roughly) the requested size.
  gfx::Size window_size = popup->GetView()->GetContainerSize();
  EXPECT_TRUE(349 <= window_size.width() && window_size.width() <= 351);
  EXPECT_TRUE(249 <= window_size.height() && window_size.height() <= 251);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, CorrectReferrer) {
  RunCheckTest(browser(),
               base::FilePath(FILE_PATH_LITERAL("popup-referrer.html")),
               true,
               true);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, WindowFeaturesBarProps) {
  RunCheckTest(browser(),
               base::FilePath(FILE_PATH_LITERAL("popup-windowfeatures.html")),
               true,
               true);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, SessionStorage) {
  RunCheckTest(browser(),
               base::FilePath(FILE_PATH_LITERAL("popup-sessionstorage.html")),
               true,
               true);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, Opener) {
  RunCheckTest(browser(),
               base::FilePath(FILE_PATH_LITERAL("popup-opener.html")),
               true,
               true);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, OpenerSuppressed) {
  RunCheckTest(browser(),
               base::FilePath(FILE_PATH_LITERAL("popup-openersuppressed.html")),
               false,
               true);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, ShiftClick) {
  RunCheckTest(
      browser(),
      base::FilePath(FILE_PATH_LITERAL("popup-fake-click-on-anchor3.html")),
      true,
      true);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, WebUI) {
  WebContents* popup =
      RunCheckTest(browser(),
                   base::FilePath(FILE_PATH_LITERAL("popup-webui.html")),
                   true,
                   false);

  // Check that the new popup displays about:blank.
  EXPECT_EQ(GURL(content::kAboutBlankURL), popup->GetURL());
}

}  // namespace
