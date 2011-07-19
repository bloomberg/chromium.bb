// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/message_loop.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

namespace {

// Helper to debug intermittent test hangs/timeouts.
// TODO(phajdan.jr): remove when http://crbug.com/57994 is fixed.
void Checkpoint(const char* message, const base::TimeTicks& start_time) {
  LOG(INFO) << message << " : "
            << (base::TimeTicks::Now() - start_time).InMilliseconds()
            << " ms" << std::flush;
}

// Note: WaitableEvent is not used for synchronization between the main thread
// and history backend thread because the history subsystem posts tasks back
// to the main thread. Had we tried to Signal an event in such a task
// and Wait for it on the main thread, the task would not run at all because
// the main thread would be blocked on the Wait call, resulting in a deadlock.

// A task to be scheduled on the history backend thread.
// Notifies the main thread after all history backend thread tasks have run.
class WaitForHistoryTask : public HistoryDBTask {
 public:
  WaitForHistoryTask() {
  }

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) {
    return true;
  }

  virtual void DoneRunOnMainThread() {
    MessageLoop::current()->Quit();
  }

 private:
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
        NewRunnableMethod(history, &HistoryService::IterateURLs, this));
    ui_test_utils::RunMessageLoop();
  }

  virtual void OnURL(const GURL& url) {
    urls_.push_back(url);
  }

  virtual void OnComplete(bool success) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        new MessageLoop::QuitTask());
  }

  std::vector<GURL>& urls() { return urls_; }

 private:
  std::vector<GURL> urls_;

  DISALLOW_COPY_AND_ASSIGN(HistoryEnumerator);
};

class HistoryBrowserTest : public InProcessBrowserTest {
 protected:
  PrefService* GetPrefs() {
    return GetProfile()->GetPrefs();
  }

  Profile* GetProfile() {
    return browser()->GetProfile();
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
                            NewRunnableMethod(history,
                                              &HistoryService::ScheduleDBTask,
                                              task,
                                              &request_consumer));
    ui_test_utils::RunMessageLoop();
  }

  void ExpectEmptyHistory() {
    std::vector<GURL> urls(GetHistoryContents());
    EXPECT_EQ(0U, urls.size());
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
// TODO(phajdan.jr): remove debug code when http://crbug.com/57994 is fixed.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, SavingHistoryDisabled) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled, true);

  EXPECT_TRUE(GetProfile()->GetHistoryService(Profile::EXPLICIT_ACCESS));
  EXPECT_FALSE(GetProfile()->GetHistoryService(Profile::IMPLICIT_ACCESS));

  Checkpoint("Before waiting for history to load", start_time);
  ui_test_utils::WaitForHistoryToLoad(browser());
  Checkpoint("After waiting for history to load", start_time);
  ExpectEmptyHistory();
  Checkpoint("After checking history", start_time);

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  Checkpoint("After NavigateToURL", start_time);
  WaitForHistoryBackendToRun();
  Checkpoint("After waiting for history backend to run", start_time);
  ExpectEmptyHistory();
  Checkpoint("After second check", start_time);
}

// Test that changing the pref takes effect immediately
// when the browser is running.
// TODO(phajdan.jr): remove debug code when http://crbug.com/57994 is fixed.
IN_PROC_BROWSER_TEST_F(HistoryBrowserTest, SavingHistoryEnabledThenDisabled) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  EXPECT_FALSE(GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled));

  Checkpoint("Before waiting for history to load", start_time);
  ui_test_utils::WaitForHistoryToLoad(browser());
  Checkpoint("After waiting for history to load", start_time);

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  Checkpoint("After first NavigateToURL", start_time);
  WaitForHistoryBackendToRun();
  Checkpoint("After waiting for history backend to run", start_time);

  {
    std::vector<GURL> urls(GetHistoryContents());
    Checkpoint("After first GetHistoryContents", start_time);
    ASSERT_EQ(1U, urls.size());
    EXPECT_EQ(GetTestUrl().spec(), urls[0].spec());
  }

  GetPrefs()->SetBoolean(prefs::kSavingBrowserHistoryDisabled, true);

  ui_test_utils::NavigateToURL(browser(), GetTestUrl());
  Checkpoint("After second NavigateToURL", start_time);
  WaitForHistoryBackendToRun();
  Checkpoint("After waiting for history backend to run (2nd time)", start_time);

  {
    // No additional entries should be present in the history.
    std::vector<GURL> urls(GetHistoryContents());
    Checkpoint("After second GetHistoryContents", start_time);
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

}  // namespace
