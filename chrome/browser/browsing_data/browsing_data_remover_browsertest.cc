// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "testing/gtest/include/gtest/gtest.h"

class BrowsingDataRemoverBrowserTest : public InProcessBrowserTest {
 public:
  BrowsingDataRemoverBrowserTest() {}

  void RunScriptAndCheckResult(const std::wstring& script,
                               const std::string& result) {
    std::string data;
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
        chrome::GetActiveWebContents(browser())->GetRenderViewHost(), L"",
        script, &data));
    ASSERT_EQ(data, result);
  }

  void RemoveAndWait(int remove_mask) {
    content::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_BROWSING_DATA_REMOVED,
        content::Source<Profile>(browser()->profile()));  
    BrowsingDataRemover* remover = new BrowsingDataRemover(
        browser()->profile(), BrowsingDataRemover::LAST_HOUR,
        base::Time::Now() + base::TimeDelta::FromMilliseconds(10));
    remover->Remove(remove_mask, BrowsingDataHelper::UNPROTECTED_WEB);
    signal.Wait();
  }
};

// Test BrowsingDataRemover for downloads.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, Download) {
  // Start a download.
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(browser()->profile());
  scoped_ptr<content::DownloadTestObserver> observer(
      new content::DownloadTestObserverTerminal(
          download_manager, 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));

  GURL download_url = ui_test_utils::GetTestUrl(
      FilePath().AppendASCII("downloads"),
      FilePath().AppendASCII("a_zip_file.zip"));
  ui_test_utils::NavigateToURL(browser(), download_url);
  observer->WaitForFinished();

  std::vector<content::DownloadItem*> downloads;
  download_manager->GetAllDownloads(FilePath(), &downloads);
  EXPECT_EQ(1u, downloads.size());

  RemoveAndWait(BrowsingDataRemover::REMOVE_DOWNLOADS);

  downloads.clear();
  download_manager->GetAllDownloads(FilePath(), &downloads);
  EXPECT_TRUE(downloads.empty());
}
#if !defined(OS_LINUX)
// Verify can modify database after deleting it.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, Database) {
  ASSERT_TRUE(test_server()->Start());
  GURL url = test_server()->GetURL("files/database/simple_database.html");
  ui_test_utils::NavigateToURL(browser(), url);

  RunScriptAndCheckResult(L"createTable()", "done");
  RunScriptAndCheckResult(L"insertRecord('text')", "done");
  RunScriptAndCheckResult(L"getRecords()", "text");

  RemoveAndWait(BrowsingDataRemover::REMOVE_SITE_DATA);

  ui_test_utils::NavigateToURL(browser(), url);
  RunScriptAndCheckResult(L"createTable()", "done");
  RunScriptAndCheckResult(L"insertRecord('text2')", "done");
  RunScriptAndCheckResult(L"getRecords()", "text2");
}
#endif
