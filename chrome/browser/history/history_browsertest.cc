// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;

namespace {

// Note: WaitableEvent is not used for synchronization between the main thread
// and history backend thread because the history subsystem posts tasks back
// to the main thread. Had we tried to Signal an event in such a task
// and Wait for it on the main thread, the task would not run at all because
// the main thread would be blocked on the Wait call, resulting in a deadlock.

// A task to be scheduled on the history backend thread.
// Notifies the main thread after all history backend thread tasks have run.
class WaitForHistoryTask : public HistoryDBTask {
 public:
  WaitForHistoryTask() {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) {
    return true;
  }

  virtual void DoneRunOnMainThread() {
    MessageLoop::current()->Quit();
  }

 private:
  virtual ~WaitForHistoryTask() {}

  DISALLOW_COPY_AND_ASSIGN(WaitForHistoryTask);
};

}  // namespace

class HistoryBrowserTest : public InProcessBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableFileCookies);
  }

  PrefService* GetPrefs() {
    return GetProfile()->GetPrefs();
  }

  Profile* GetProfile() {
    return browser()->profile();
  }

  std::vector<GURL> GetHistoryContents() {
    ui_test_utils::HistoryEnumerator enumerator(GetProfile());
    return enumerator.urls();
  }

  GURL GetTestUrl() {
    return ui_test_utils::GetTestUrl(
        FilePath(FilePath::kCurrentDirectory),
        FilePath(FILE_PATH_LITERAL("title2.html")));
  }

  void WaitForHistoryBackendToRun() {
    CancelableRequestConsumerTSimple<int> request_consumer;
    scoped_refptr<HistoryDBTask> task(new WaitForHistoryTask());
    HistoryService* history =
        HistoryServiceFactory::GetForProfile(GetProfile(),
                                             Profile::EXPLICIT_ACCESS);
    history->HistoryService::ScheduleDBTask(task, &request_consumer);
    content::RunMessageLoop();
  }

  void ExpectEmptyHistory() {
    std::vector<GURL> urls(GetHistoryContents());
    EXPECT_EQ(0U, urls.size());
  }

  void LoadAndWaitForURL(const GURL& url) {
    string16 expected_title(ASCIIToUTF16("OK"));
    content::TitleWatcher title_watcher(
        chrome::GetActiveWebContents(browser()), expected_title);
    title_watcher.AlsoWaitForTitle(ASCIIToUTF16("FAIL"));
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void LoadAndWaitForFile(const char* filename) {
    GURL url = ui_test_utils::GetTestUrl(
        FilePath().AppendASCII("History"),
        FilePath().AppendASCII(filename));
    LoadAndWaitForURL(url);
  }
};

// Test that the browser history is saved (default setting).
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, SavingHistoryEnabled) {
  EXPECT_FALSE(GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled));

  EXPECT_TRUE(HistoryServiceFactory::GetForProfile(
      GetProfile(), Profile::EXPLICIT_ACCESS));
  EXPECT_TRUE(HistoryServiceFactory::GetForProfile(
      GetProfile(), Profile::IMPLICIT_ACCESS));

  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      browser()->profile(), Profile::EXPLICIT_ACCESS));
  ExpectEmptyHistory();

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  WaitForHistoryBackendToRun();

  {
    std::vector<GURL> urls(GetHistoryContents());
    ASSERT_EQ(1U, urls.size());
    EXPECT_EQ(GetTestUrl().spec(), urls[0].spec());
  }
}

// Test that disabling saving browser history really works.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, SavingHistoryDisabled) {
  GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled, true);

  EXPECT_TRUE(HistoryServiceFactory::GetForProfile(
      GetProfile(), Profile::EXPLICIT_ACCESS));
  EXPECT_FALSE(HistoryServiceFactory::GetForProfile(
      GetProfile(), Profile::IMPLICIT_ACCESS));

  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      browser()->profile(), Profile::EXPLICIT_ACCESS));
  ExpectEmptyHistory();

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  WaitForHistoryBackendToRun();
  ExpectEmptyHistory();
}

// Test that changing the pref takes effect immediately
// when the browser is running.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, SavingHistoryEnabledThenDisabled) {
  EXPECT_FALSE(GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled));

  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      browser()->profile(), Profile::EXPLICIT_ACCESS));

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  WaitForHistoryBackendToRun();

  {
    std::vector<GURL> urls(GetHistoryContents());
    ASSERT_EQ(1U, urls.size());
    EXPECT_EQ(GetTestUrl().spec(), urls[0].spec());
  }

  GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled, true);

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  WaitForHistoryBackendToRun();

  {
    // No additional entries should be present in the history.
    std::vector<GURL> urls(GetHistoryContents());
    ASSERT_EQ(1U, urls.size());
    EXPECT_EQ(GetTestUrl().spec(), urls[0].spec());
  }
}

// Test that changing the pref takes effect immediately
// when the browser is running.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, SavingHistoryDisabledThenEnabled) {
  GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled, true);

  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      browser()->profile(), Profile::EXPLICIT_ACCESS));
  ExpectEmptyHistory();

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  WaitForHistoryBackendToRun();
  ExpectEmptyHistory();

  GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled, false);

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  WaitForHistoryBackendToRun();

  {
    std::vector<GURL> urls(GetHistoryContents());
    ASSERT_EQ(1U, urls.size());
    EXPECT_EQ(GetTestUrl().spec(), urls[0].spec());
  }
}

IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, VerifyHistoryLength1) {
  // Test the history length for the following page transitions.
  //   -open-> Page 1.
  LoadAndWaitForFile("history_length_test_page_1.html");
}

IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, VerifyHistoryLength2) {
  // Test the history length for the following page transitions.
  //   -open-> Page 2 -redirect-> Page 3.
  LoadAndWaitForFile("history_length_test_page_2.html");
}

IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, VerifyHistoryLength3) {
  // Test the history length for the following page transitions.
  // -open-> Page 1 -> open Page 2 -redirect Page 3. open Page 4
  // -navigate_backward-> Page 3 -navigate_backward->Page 1
  // -navigate_forward-> Page 3 -navigate_forward-> Page 4
  LoadAndWaitForFile("history_length_test_page_1.html");
  LoadAndWaitForFile("history_length_test_page_2.html");
  LoadAndWaitForFile("history_length_test_page_4.html");
}

IN_PROC_BROWSER_TEST_F(HistoryBrowserTest,
                       ConsiderRedirectAfterGestureAsUserInitiated) {
  // Test the history length for the following page transition.
  //
  // -open-> Page 11 -slow_redirect-> Page 12.
  //
  // If redirect occurs after a user gesture, e.g., mouse click, the
  // redirect is more likely to be user-initiated rather than automatic.
  // Therefore, Page 11 should be in the history in addition to Page 12.
  LoadAndWaitForFile("history_length_test_page_11.html");

  content::SimulateMouseClick(chrome::GetActiveWebContents(browser()), 0,
      WebKit::WebMouseEvent::ButtonLeft);
  LoadAndWaitForFile("history_length_test_page_11.html");
}

IN_PROC_BROWSER_TEST_F(HistoryBrowserTest,
                       ConsiderSlowRedirectAsUserInitiated) {
  // Test the history length for the following page transition.
  //
  // -open-> Page 21 -redirect-> Page 22.
  //
  // If redirect occurs more than 5 seconds later after the page is loaded,
  // the redirect is likely to be user-initiated.
  // Therefore, Page 21 should be in the history in addition to Page 22.
  LoadAndWaitForFile("history_length_test_page_21.html");
}

// If this test flakes, use bug 22111.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, HistorySearchXSS) {
  GURL url(std::string(chrome::kChromeUIHistoryURL) +
      "#q=%3Cimg%20src%3Dx%3Ax%20onerror%3D%22document.title%3D'XSS'%22%3E");
  ui_test_utils::NavigateToURL(browser(), url);
  // Mainly, this is to ensure we send a synchronous message to the renderer
  // so that we're not susceptible (less susceptible?) to a race condition.
  // Should a race condition ever trigger, it won't result in flakiness.
  int num = ui_test_utils::FindInPage(
      chrome::GetActiveWebContents(browser()), ASCIIToUTF16("<img"), true,
      true, NULL, NULL);
  EXPECT_GT(num, 0);
  EXPECT_EQ(ASCIIToUTF16("History"),
            chrome::GetActiveWebContents(browser())->GetTitle());
}

// Verify that history persists after session restart.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, PRE_HistoryPersists) {
  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ(GetTestUrl(), urls[0]);
}

IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, HistoryPersists) {
  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ(GetTestUrl(), urls[0]);
}

// Invalid URLs should not go in history.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, InvalidURLNoHistory) {
  GURL non_existant = ui_test_utils::GetTestUrl(
      FilePath().AppendASCII("History"),
      FilePath().AppendASCII("non_existant_file.html"));
  ui_test_utils::NavigateToURL(browser(), non_existant);
  ExpectEmptyHistory();
}

// New tab page should not show up in history.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, NewTabNoHistory) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ExpectEmptyHistory();
}

// Incognito browsing should not show up in history.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, IncognitoNoHistory) {
  ui_test_utils::NavigateToURL(CreateIncognitoBrowser(), GetTestUrl());
  ExpectEmptyHistory();
}

// Multiple navigations to the same url should have a single history.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, NavigateMultiTimes) {
  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ(GetTestUrl(), urls[0]);
}

// Verify history with multiple windows and tabs.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, MultiTabsWindowsHistory) {
  GURL url1 = GetTestUrl();
  GURL url2  = ui_test_utils::GetTestUrl(
      FilePath(), FilePath(FILE_PATH_LITERAL("title1.html")));
  GURL url3  = ui_test_utils::GetTestUrl(
      FilePath(), FilePath(FILE_PATH_LITERAL("title3.html")));
  GURL url4  = ui_test_utils::GetTestUrl(
      FilePath(), FilePath(FILE_PATH_LITERAL("simple.html")));

  ui_test_utils::NavigateToURL(browser(), url1);
  Browser* browser2 = CreateBrowser(browser()->profile());
  ui_test_utils::NavigateToURL(browser2, url2);
  ui_test_utils::NavigateToURLWithDisposition(
      browser2, url3, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ui_test_utils::NavigateToURLWithDisposition(
      browser2, url4, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(4u, urls.size());
  ASSERT_EQ(url4, urls[0]);
  ASSERT_EQ(url3, urls[1]);
  ASSERT_EQ(url2, urls[2]);
  ASSERT_EQ(url1, urls[3]);
}

// Downloaded URLs should not show up in history.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, DownloadNoHistory) {
  GURL download_url = ui_test_utils::GetTestUrl(
      FilePath().AppendASCII("downloads"),
      FilePath().AppendASCII("a_zip_file.zip"));
  ui_test_utils::DownloadURL(browser(), download_url);
  ExpectEmptyHistory();
}

// HTTP meta-refresh redirects should have separate history entries.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, RedirectHistory) {
  GURL redirector = ui_test_utils::GetTestUrl(
      FilePath().AppendASCII("History"),
      FilePath().AppendASCII("redirector.html"));
  GURL landing_url = ui_test_utils::GetTestUrl(
      FilePath().AppendASCII("History"),
      FilePath().AppendASCII("landing.html"));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), redirector, 2);
  ASSERT_EQ(landing_url, chrome::GetActiveWebContents(browser())->GetURL());
  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(landing_url, urls[0]);
  ASSERT_EQ(redirector, urls[1]);
}

// Verify that navigation brings current page to top of history list.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, NavigateBringPageToTop) {
  GURL url1 = GetTestUrl();
  GURL url2  = ui_test_utils::GetTestUrl(
      FilePath(), FilePath(FILE_PATH_LITERAL("title3.html")));

  ui_test_utils::NavigateToURL(browser(), url1);
  ui_test_utils::NavigateToURL(browser(), url2);

  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(url2, urls[0]);
  ASSERT_EQ(url1, urls[1]);
}

// Verify that reloading a page brings it to top of history list.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, ReloadBringPageToTop) {
  GURL url1 = GetTestUrl();
  GURL url2  = ui_test_utils::GetTestUrl(
      FilePath(), FilePath(FILE_PATH_LITERAL("title3.html")));

  ui_test_utils::NavigateToURL(browser(), url1);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(url2, urls[0]);
  ASSERT_EQ(url1, urls[1]);

  content::WebContents* tab = chrome::GetActiveWebContents(browser());
  tab->GetController().Reload(false);
  content::WaitForLoadStop(tab);

  urls = GetHistoryContents();
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(url1, urls[0]);
  ASSERT_EQ(url2, urls[1]);
}

// Verify that back/forward brings current page to top of history list.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, BackForwardBringPageToTop) {
  GURL url1 = GetTestUrl();
  GURL url2  = ui_test_utils::GetTestUrl(
      FilePath(), FilePath(FILE_PATH_LITERAL("title3.html")));

  ui_test_utils::NavigateToURL(browser(), url1);
  ui_test_utils::NavigateToURL(browser(), url2);

  content::WebContents* tab = chrome::GetActiveWebContents(browser());
  chrome::GoBack(browser(), CURRENT_TAB);
  content::WaitForLoadStop(tab);

  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(url1, urls[0]);
  ASSERT_EQ(url2, urls[1]);

  chrome::GoForward(browser(), CURRENT_TAB);
  content::WaitForLoadStop(tab);
  urls = GetHistoryContents();
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(url2, urls[0]);
  ASSERT_EQ(url1, urls[1]);
}

// Verify that submitting form adds target page to history list.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, SubmitFormAddsTargetPage) {
  GURL form = ui_test_utils::GetTestUrl(
      FilePath().AppendASCII("History"),
      FilePath().AppendASCII("form.html"));
  GURL target = ui_test_utils::GetTestUrl(
      FilePath().AppendASCII("History"),
      FilePath().AppendASCII("target.html"));
  ui_test_utils::NavigateToURL(browser(), form);

  content::WebContents* web_contents = chrome::GetActiveWebContents(browser());
  string16 expected_title(ASCIIToUTF16("Target Page"));
  content::TitleWatcher title_watcher(
      chrome::GetActiveWebContents(browser()), expected_title);
  ASSERT_TRUE(content::ExecuteScript(
      web_contents, "document.getElementById('form').submit()"));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  std::vector<GURL> urls(GetHistoryContents());
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(target, urls[0]);
  ASSERT_EQ(form, urls[1]);
}

// Verify history shortcut opens only one history tab per window.  Also, make
// sure that existing history tab is activated.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, OneHistoryTabPerWindow) {
  GURL history_url(chrome::kChromeUIHistoryURL);

  // Even after navigate completes, the currently-active tab title is
  // 'Loading...' for a brief time while the history page loads.
  content::WebContents* web_contents = chrome::GetActiveWebContents(browser());
  string16 expected_title(ASCIIToUTF16("History"));
  content::TitleWatcher title_watcher(web_contents, expected_title);
  chrome::ExecuteCommand(browser(), IDC_SHOW_HISTORY);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kAboutBlankURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  chrome::ExecuteCommand(browser(), IDC_SHOW_HISTORY);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);
  ASSERT_EQ(history_url, active_web_contents->GetURL());

  content::WebContents* second_tab =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_NE(history_url, second_tab->GetURL());
}
