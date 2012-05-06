// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/test/test_browser_thread.h"
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

// Enumerates all history contents on the backend thread.
class HistoryEnumerator : public HistoryService::URLEnumerator {
 public:
  explicit HistoryEnumerator(HistoryService* history) {
    EXPECT_TRUE(history);
    if (!history)
      return;

    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&HistoryService::IterateURLs, history, this));
    ui_test_utils::RunMessageLoop();
  }

  virtual void OnURL(const GURL& url) {
    urls_.push_back(url);
  }

  virtual void OnComplete(bool success) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        MessageLoop::QuitClosure());
  }

  std::vector<GURL>& urls() { return urls_; }

 private:
  std::vector<GURL> urls_;

  DISALLOW_COPY_AND_ASSIGN(HistoryEnumerator);
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

  HistoryService* GetHistoryService() {
    return GetProfile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  }

  std::vector<GURL> GetHistoryContents() {
    HistoryEnumerator enumerator(GetHistoryService());
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
    HistoryService* history = GetHistoryService();
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&HistoryService::ScheduleDBTask,
                                       history, task, &request_consumer));
    ui_test_utils::RunMessageLoop();
  }

  void ExpectEmptyHistory() {
    std::vector<GURL> urls(GetHistoryContents());
    EXPECT_EQ(0U, urls.size());
  }

  void LoadAndWaitForURL(const GURL& url) {
    string16 expected_title(ASCIIToUTF16("OK"));
    ui_test_utils::TitleWatcher title_watcher(
        browser()->GetSelectedWebContents(), expected_title);
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

  EXPECT_TRUE(GetProfile()->GetHistoryService(Profile::EXPLICIT_ACCESS));
  EXPECT_TRUE(GetProfile()->GetHistoryService(Profile::IMPLICIT_ACCESS));

  ui_test_utils::WaitForHistoryToLoad(browser());
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

  EXPECT_TRUE(GetProfile()->GetHistoryService(Profile::EXPLICIT_ACCESS));
  EXPECT_FALSE(GetProfile()->GetHistoryService(Profile::IMPLICIT_ACCESS));

  ui_test_utils::WaitForHistoryToLoad(browser());
  ExpectEmptyHistory();

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  WaitForHistoryBackendToRun();
  ExpectEmptyHistory();
}

// Test that changing the pref takes effect immediately
// when the browser is running.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, SavingHistoryEnabledThenDisabled) {
  EXPECT_FALSE(GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled));

  ui_test_utils::WaitForHistoryToLoad(browser());

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

  ui_test_utils::WaitForHistoryToLoad(browser());
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

  ui_test_utils::SimulateMouseClick(browser()->GetSelectedWebContents());
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
      browser()->GetSelectedTabContentsWrapper(), ASCIIToUTF16("<img"), true,
      true, NULL);
  EXPECT_GT(num, 0);
  EXPECT_EQ(ASCIIToUTF16("History"),
            browser()->GetSelectedWebContents()->GetTitle());
}
