// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/browser/browsing_data/browsing_data_remover_test_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "net/http/transport_security_state.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {
void SetUrlRequestMock(const base::FilePath& path) {
  net::URLRequestMockHTTPJob::AddUrlHandlers(path,
                                             BrowserThread::GetBlockingPool());
}
}

class BrowsingDataRemoverBrowserTest : public InProcessBrowserTest {
 public:
  BrowsingDataRemoverBrowserTest() {}

  void SetUpOnMainThread() override {
    base::FilePath path;
    PathService::Get(content::DIR_TEST_DATA, &path);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(&SetUrlRequestMock, path));
  }

  void RunScriptAndCheckResult(const std::string& script,
                               const std::string& result) {
    std::string data;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(), script, &data));
    ASSERT_EQ(data, result);
  }

  void VerifyDownloadCount(size_t expected) {
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    std::vector<content::DownloadItem*> downloads;
    download_manager->GetAllDownloads(&downloads);
    EXPECT_EQ(expected, downloads.size());
  }

  void DownloadAnItem() {
    base::ScopedTempDir downloads_directory;
    ASSERT_TRUE(downloads_directory.CreateUniqueTempDir());
    browser()->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory, downloads_directory.path());

    // Start a download.
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    std::unique_ptr<content::DownloadTestObserver> observer(
        new content::DownloadTestObserverTerminal(
            download_manager, 1,
            content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));

    GURL download_url = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("downloads"),
        base::FilePath().AppendASCII("a_zip_file.zip"));
    ui_test_utils::NavigateToURL(browser(), download_url);
    observer->WaitForFinished();

    VerifyDownloadCount(1u);
  }

  void RemoveAndWait(int remove_mask) {
    BrowsingDataRemover* remover =
        BrowsingDataRemoverFactory::GetForBrowserContext(browser()->profile());
    BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->Remove(BrowsingDataRemover::Period(BrowsingDataRemover::LAST_HOUR),
                    remove_mask, BrowsingDataHelper::UNPROTECTED_WEB);
    completion_observer.BlockUntilCompletion();
  }
};

class BrowsingDataRemoverTransportSecurityStateBrowserTest
    : public BrowsingDataRemoverBrowserTest {
 public:
  BrowsingDataRemoverTransportSecurityStateBrowserTest() {}

  void SetUpOnMainThread() override {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BrowsingDataRemoverTransportSecurityStateBrowserTest::
                       SetUpTransportSecurityState,
                   this, base::RetainedRef(
                             browser()->profile()->GetRequestContext())));
  }

  void CheckTransportSecurityState(
      scoped_refptr<net::URLRequestContextGetter> context_getter,
      bool should_be_cleared) {
    net::TransportSecurityState* state =
        context_getter->GetURLRequestContext()->transport_security_state();
    if (should_be_cleared)
      EXPECT_FALSE(state->ShouldUpgradeToSSL("example.test"));
    else
      EXPECT_TRUE(state->ShouldUpgradeToSSL("example.test"));
  }

 protected:
  void SetUpTransportSecurityState(
      scoped_refptr<net::URLRequestContextGetter> context_getter) {
    net::TransportSecurityState* state =
        context_getter->GetURLRequestContext()->transport_security_state();
    base::Time expiry = base::Time::Now() + base::TimeDelta::FromDays(1000);
    EXPECT_FALSE(state->ShouldUpgradeToSSL("example.test"));
    state->AddHSTS("example.test", expiry, false);
    EXPECT_TRUE(state->ShouldUpgradeToSSL("example.test"));
  }
};

// Test BrowsingDataRemover for downloads.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, Download) {
  DownloadAnItem();
  RemoveAndWait(BrowsingDataRemover::REMOVE_DOWNLOADS);
  VerifyDownloadCount(0u);
}

// The call to Remove() should crash in debug (DCHECK), but the browser-test
// process model prevents using a death test.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// Test BrowsingDataRemover for prohibited downloads. Note that this only
// really exercises the code in a Release build.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, DownloadProhibited) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  DownloadAnItem();
  RemoveAndWait(BrowsingDataRemover::REMOVE_DOWNLOADS);
  VerifyDownloadCount(1u);
}
#endif

// Verify can modify database after deleting it.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, Database) {
  GURL url(net::URLRequestMockHTTPJob::GetMockUrl("simple_database.html"));
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

// Verify that TransportSecurityState data is cleared for REMOVE_CACHE.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverTransportSecurityStateBrowserTest,
                       ClearTransportSecurityState) {
  RemoveAndWait(BrowsingDataRemover::REMOVE_CACHE);
  base::RunLoop run_loop;
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BrowsingDataRemoverTransportSecurityStateBrowserTest::
                     CheckTransportSecurityState,
                 this,
                 base::RetainedRef(browser()->profile()->GetRequestContext()),
                 true /* should be cleared */),
      run_loop.QuitClosure());
}

// Verify that TransportSecurityState data is not cleared if REMOVE_CACHE is not
// set.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverTransportSecurityStateBrowserTest,
                       PreserveTransportSecurityState) {
  RemoveAndWait(BrowsingDataRemover::REMOVE_SITE_DATA);
  base::RunLoop run_loop;
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BrowsingDataRemoverTransportSecurityStateBrowserTest::
                     CheckTransportSecurityState,
                 this,
                 base::RetainedRef(browser()->profile()->GetRequestContext()),
                 false /* should not be cleared */),
      run_loop.QuitClosure());
}
