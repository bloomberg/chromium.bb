// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {
void SetUrlRequestMock(const FilePath& path) {
  content::URLRequestMockHTTPJob::AddUrlHandler(path);
}
}

class BrowsingDataRemoverBrowserTest : public InProcessBrowserTest {
 public:
  BrowsingDataRemoverBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    FilePath path;
    PathService::Get(content::DIR_TEST_DATA, &path);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(&SetUrlRequestMock, path));
  }

  void RunScriptAndCheckResult(const std::string& script,
                               const std::string& result) {
    std::string data;
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
        chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
        "",
        script,
        &data));
    ASSERT_EQ(data, result);
  }

  void RemoveAndWait(int remove_mask) {
    content::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_BROWSING_DATA_REMOVED,
        content::Source<Profile>(browser()->profile()));
    BrowsingDataRemover* remover = BrowsingDataRemover::CreateForPeriod(
        browser()->profile(), BrowsingDataRemover::LAST_HOUR);
    remover->Remove(remove_mask, BrowsingDataHelper::UNPROTECTED_WEB);
    signal.Wait();
  }
};

// Test BrowsingDataRemover for downloads.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, Download) {
  base::ScopedTempDir downloads_directory;
  ASSERT_TRUE(downloads_directory.CreateUniqueTempDir());
  browser()->profile()->GetPrefs()->SetFilePath(
      prefs::kDownloadDefaultDirectory, downloads_directory.path());

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
  download_manager->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());

  RemoveAndWait(BrowsingDataRemover::REMOVE_DOWNLOADS);

  downloads.clear();
  download_manager->GetAllDownloads(&downloads);
  EXPECT_TRUE(downloads.empty());
}

// Verify can modify database after deleting it.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, Database) {
  GURL url(content::URLRequestMockHTTPJob::GetMockUrl(
      FilePath().AppendASCII("simple_database.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  RunScriptAndCheckResult("createTable()", "done");
  RunScriptAndCheckResult("insertRecord('text')", "done");
  RunScriptAndCheckResult("getRecords()", "text");

  RemoveAndWait(BrowsingDataRemover::REMOVE_SITE_DATA);

  ui_test_utils::NavigateToURL(browser(), url);
  RunScriptAndCheckResult("createTable()", "done");
  RunScriptAndCheckResult("insertRecord('text2')", "done");
  RunScriptAndCheckResult("getRecords()", "text2");
}

// Profile::ClearNetworkingHistorySince should be exercised here too see whether
// the call gets delegated through ProfileIO[Impl]Data properly, which is hard
// to write unit-tests for. Currently this is done by both of the above tests.
// Add standalone test if this changes.
