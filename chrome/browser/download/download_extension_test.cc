// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/download/download_extension_api.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_test_observer.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/net/url_request_slow_download_job.h"
#include "content/public/browser/browser_thread.h"

class DownloadExtensionTest : public InProcessBrowserTest {
 protected:
  // InProcessBrowserTest
  virtual void SetUpOnMainThread() OVERRIDE {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(CreateAndSetDownloadsDirectory(browser()));
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                                 false);
    GetDownloadManager()->RemoveAllDownloads();
  }

  DownloadManager* GetDownloadManager() {
    DownloadService* download_service =
        DownloadServiceFactory::GetForProfile(browser()->profile());
    return download_service->GetDownloadManager();
  }

  DownloadItem* CreateSlowTestDownload() {
    scoped_ptr<DownloadTestObserver> observer(
        CreateDownloadObserver(1, DownloadItem::IN_PROGRESS));
    GURL slow_download_url(URLRequestSlowDownloadJob::kUnknownSizeUrl);

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), slow_download_url, CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

    observer->WaitForFinished();
    // We don't expect a select file dialog.
    if (observer->select_file_dialog_seen())
      return NULL;

    DownloadManager::DownloadVector items;
    GetDownloadManager()->GetAllDownloads(FilePath(), &items);

    DownloadItem* new_item = NULL;
    for (DownloadManager::DownloadVector::iterator iter = items.begin();
         iter != items.end(); ++iter) {
      if ((*iter)->GetState() == DownloadItem::IN_PROGRESS) {
        // There should be only one IN_PROGRESS item.
        EXPECT_EQ(NULL, new_item);
        new_item = *iter;
      }
    }
    return new_item;
  }

  DownloadTestObserver* CreateDownloadObserver(
      size_t download_count,
      DownloadItem::DownloadState finished_state) {
    return new DownloadTestObserver(
        GetDownloadManager(), download_count, finished_state, true,
        DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  }

  bool RunFunction(UIThreadExtensionFunction* function,
                   const std::string& args) {
    scoped_refptr<ExtensionFunction> function_owner(function);
    return extension_function_test_utils::RunFunction(
        function, args, browser(), extension_function_test_utils::NONE);
  }

  std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                        const std::string& args) {
    return extension_function_test_utils::RunFunctionAndReturnError(
        function, args, browser(), extension_function_test_utils::NONE);
  }

  std::string DownloadItemIdAsArgList(const DownloadItem* download_item) {
    return base::StringPrintf("[%d]", download_item->GetId());
  }

 private:
  bool CreateAndSetDownloadsDirectory(Browser* browser) {
    if (!downloads_directory_.CreateUniqueTempDir())
      return false;
    browser->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory,
        downloads_directory_.path());
    return true;
  }

  ScopedTempDir downloads_directory_;
};

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, DownloadsApi_PauseResumeCancel) {
  DownloadItem* download_item = CreateSlowTestDownload();
  ASSERT_TRUE(download_item);

  // Call pause().  It should succeed and the download should be paused on
  // return.
  EXPECT_TRUE(RunFunction(new DownloadsPauseFunction(),
                          DownloadItemIdAsArgList(download_item)));
  EXPECT_TRUE(download_item->IsPaused());

  // Calling pause() twice shouldn't be an error.
  EXPECT_TRUE(RunFunction(new DownloadsPauseFunction(),
                          DownloadItemIdAsArgList(download_item)));
  EXPECT_TRUE(download_item->IsPaused());

  // Now try resuming this download.  It should succeed.
  EXPECT_TRUE(RunFunction(new DownloadsResumeFunction(),
                          DownloadItemIdAsArgList(download_item)));
  EXPECT_FALSE(download_item->IsPaused());

  // Resume again.  Resuming a download that wasn't paused is not an error.
  EXPECT_TRUE(RunFunction(new DownloadsResumeFunction(),
                          DownloadItemIdAsArgList(download_item)));
  EXPECT_FALSE(download_item->IsPaused());

  // Pause again.
  EXPECT_TRUE(RunFunction(new DownloadsPauseFunction(),
                          DownloadItemIdAsArgList(download_item)));
  EXPECT_TRUE(download_item->IsPaused());

  // And now cancel.
  EXPECT_TRUE(RunFunction(new DownloadsCancelFunction(),
                          DownloadItemIdAsArgList(download_item)));
  EXPECT_TRUE(download_item->IsCancelled());

  // Cancel again.  Shouldn't have any effect.
  EXPECT_TRUE(RunFunction(new DownloadsCancelFunction(),
                          DownloadItemIdAsArgList(download_item)));
  EXPECT_TRUE(download_item->IsCancelled());

  // Calling paused on a non-active download yields kInvalidOperationError.
  std::string error = RunFunctionAndReturnError(
      new DownloadsPauseFunction(), DownloadItemIdAsArgList(download_item));
  EXPECT_STREQ(download_extension_errors::kInvalidOperationError,
               error.c_str());

  // Calling resume on a non-active download yields kInvalidOperationError
  error = RunFunctionAndReturnError(
      new DownloadsResumeFunction(), DownloadItemIdAsArgList(download_item));
  EXPECT_STREQ(download_extension_errors::kInvalidOperationError,
               error.c_str());

  // Calling pause()/resume()/cancel() with invalid download Ids is
  // tested in the API test (DownloadsApiTest).
}
