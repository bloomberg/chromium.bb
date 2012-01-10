// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "base/stringprintf.h"
#include "chrome/browser/download/download_extension_api.h"
#include "chrome/browser/download/download_file_icon_extractor.h"
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
#include "content/browser/download/download_persistent_store_info.h"
#include "content/browser/net/url_request_slow_download_job.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "net/base/data_url.h"
#include "net/base/net_util.h"
#include "ui/gfx/codec/png_codec.h"

using content::BrowserThread;
using content::DownloadItem;
using content::DownloadManager;

namespace {

class DownloadExtensionTest : public InProcessBrowserTest {
 protected:
  // InProcessBrowserTest
  virtual void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
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
    DownloadManager* manager = GetDownloadManager();

    EXPECT_EQ(0, manager->InProgressCount());
    if (manager->InProgressCount() != 0)
      return NULL;

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), slow_download_url, CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

    observer->WaitForFinished();
    // We don't expect a select file dialog.
    if (observer->select_file_dialog_seen())
      return NULL;

    DownloadManager::DownloadVector items;
    manager->GetAllDownloads(FilePath(), &items);

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

  void FinishPendingSlowDownloads() {
    scoped_ptr<DownloadTestObserver> observer(
        CreateDownloadObserver(1, DownloadItem::COMPLETE));
    GURL finish_url(URLRequestSlowDownloadJob::kFinishDownloadUrl);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), finish_url, NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    observer->WaitForFinished();
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
    // extension_function_test_utils::RunFunction() does not take
    // ownership of |function|.
    scoped_refptr<ExtensionFunction> function_owner(function);
    return extension_function_test_utils::RunFunction(
        function, args, browser(), extension_function_test_utils::NONE);
  }

  std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                        const std::string& args) {
    return extension_function_test_utils::RunFunctionAndReturnError(
        function, args, browser(), extension_function_test_utils::NONE);
  }

  base::Value* RunFunctionAndReturnResult(UIThreadExtensionFunction* function,
                                          const std::string& args) {
    return extension_function_test_utils::RunFunctionAndReturnResult(
        function, args, browser());
  }

  bool RunFunctionAndReturnString(UIThreadExtensionFunction* function,
                                  const std::string& args,
                                  std::string* result_string) {
    scoped_ptr<base::Value> result(RunFunctionAndReturnResult(function, args));
    EXPECT_TRUE(result.get());
    return result.get() && result->GetAsString(result_string);
  }

  std::string DownloadItemIdAsArgList(const DownloadItem* download_item) {
    return base::StringPrintf("[%d]", download_item->GetId());
  }

  // Checks if a data URL encoded image is a PNG of a given size.
  void ExpectDataURLIsPNGWithSize(const std::string& url, int expected_size) {
    std::string mime_type;
    std::string charset;
    std::string data;
    EXPECT_FALSE(url.empty());
    EXPECT_TRUE(net::DataURL::Parse(GURL(url), &mime_type, &charset, &data));
    EXPECT_STREQ("image/png", mime_type.c_str());
    EXPECT_FALSE(data.empty());

    if (data.empty())
      return;

    int width = -1, height = -1;
    std::vector<unsigned char> decoded_data;
    EXPECT_TRUE(gfx::PNGCodec::Decode(
        reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
        gfx::PNGCodec::FORMAT_RGBA, &decoded_data,
        &width, &height));
    EXPECT_EQ(expected_size, width);
    EXPECT_EQ(expected_size, height);
  }

  const FilePath& downloads_directory() {
    return downloads_directory_.path();
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

class MockIconExtractorImpl : public DownloadFileIconExtractor {
 public:
  MockIconExtractorImpl(const FilePath& path, IconLoader::IconSize icon_size,
                        const std::string& response)
    : expected_path_(path),
      expected_icon_size_(icon_size),
      response_(response) {
  }
  virtual ~MockIconExtractorImpl() {}

  virtual bool ExtractIconURLForPath(const FilePath& path,
                                     IconLoader::IconSize icon_size,
                                     IconURLCallback callback) OVERRIDE {
    EXPECT_STREQ(expected_path_.value().c_str(), path.value().c_str());
    EXPECT_EQ(expected_icon_size_, icon_size);
    if (expected_path_ == path &&
        expected_icon_size_ == icon_size) {
      callback_ = callback;
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&MockIconExtractorImpl::RunCallback,
                     base::Unretained(this)));
      return true;
    } else {
      return false;
    }
  }

 private:
  void RunCallback() {
    callback_.Run(response_);
  }

  FilePath             expected_path_;
  IconLoader::IconSize expected_icon_size_;
  std::string          response_;
  IconURLCallback      callback_;
};

} // namespace

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

// Test downloads.getFileIcon() on in-progress, finished, cancelled and deleted
// download items.
// TODO(asanka): Fails on ChromeOS (http://crbug.com/109677)
#if defined(OS_CHROMEOS)
#define MAYBE_DownloadsApi_FileIcon_Active \
        FAILS_DownloadsApi_FileIcon_Active
#else
#define MAYBE_DownloadsApi_FileIcon_Active DownloadsApi_FileIcon_Active
#endif
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       MAYBE_DownloadsApi_FileIcon_Active) {
  DownloadItem* download_item = CreateSlowTestDownload();
  ASSERT_TRUE(download_item);

  // Get the icon for the in-progress download.  This call should succeed even
  // if the file type isn't registered.
  std::string args = base::StringPrintf("[%d, {}]", download_item->GetId());
  std::string result_string;
  EXPECT_TRUE(RunFunctionAndReturnString(new DownloadsGetFileIconFunction(),
                                         args, &result_string));

  // Note: We are checking if the result is a Data URL that has encoded
  // image/png data for an icon of a specific size. Of these, only the icon size
  // is specified in the API contract. The image type (image/png) and URL type
  // (Data) are implementation details.

  // The default size for icons returned by getFileIcon() is 32x32.
  ExpectDataURLIsPNGWithSize(result_string, 32);

  // Test whether the correct path is being pased into the icon extractor.
  FilePath expected_path(download_item->GetTargetFilePath());
  scoped_refptr<DownloadsGetFileIconFunction> function(
      new DownloadsGetFileIconFunction());
  function->SetIconExtractorForTesting(new MockIconExtractorImpl(
      expected_path, IconLoader::NORMAL, "foo"));
  EXPECT_TRUE(RunFunctionAndReturnString(function.release(), args,
                                         &result_string));

  // Now try a 16x16 icon.
  args = base::StringPrintf("[%d, {\"size\": 16}]", download_item->GetId());
  EXPECT_TRUE(RunFunctionAndReturnString(new DownloadsGetFileIconFunction(),
                                         args, &result_string));
  ExpectDataURLIsPNGWithSize(result_string, 16);

  // Explicitly asking for 32x32 should give us a 32x32 icon.
  args = base::StringPrintf("[%d, {\"size\": 32}]", download_item->GetId());
  EXPECT_TRUE(RunFunctionAndReturnString(new DownloadsGetFileIconFunction(),
                                         args, &result_string));
  ExpectDataURLIsPNGWithSize(result_string, 32);

  // Finish the download and try again.
  FinishPendingSlowDownloads();
  EXPECT_EQ(DownloadItem::COMPLETE, download_item->GetState());
  EXPECT_TRUE(RunFunctionAndReturnString(new DownloadsGetFileIconFunction(),
                                         args, &result_string));
  ExpectDataURLIsPNGWithSize(result_string, 32);

  // Check the path passed to the icon extractor post-completion.
  function = new DownloadsGetFileIconFunction();
  function->SetIconExtractorForTesting(new MockIconExtractorImpl(
      expected_path, IconLoader::NORMAL, "foo"));
  EXPECT_TRUE(RunFunctionAndReturnString(function.release(), args,
                                         &result_string));

  // Now create another download.
  download_item = CreateSlowTestDownload();
  ASSERT_TRUE(download_item);
  expected_path = download_item->GetTargetFilePath();

  // Cancel the download. As long as the download has a target path, we should
  // be able to query the file icon.
  download_item->Cancel(true);
  // Let cleanup complete on the FILE thread.
  ui_test_utils::RunAllPendingInMessageLoop(BrowserThread::FILE);
  args = base::StringPrintf("[%d, {\"size\": 32}]", download_item->GetId());
  EXPECT_TRUE(RunFunctionAndReturnString(new DownloadsGetFileIconFunction(),
                                         args, &result_string));
  ExpectDataURLIsPNGWithSize(result_string, 32);

  // Check the path passed to the icon extractor post-cancellation.
  function = new DownloadsGetFileIconFunction();
  function->SetIconExtractorForTesting(new MockIconExtractorImpl(
      expected_path, IconLoader::NORMAL, "foo"));
  EXPECT_TRUE(RunFunctionAndReturnString(function.release(), args,
                                         &result_string));

  // Simulate an error during icon load by invoking the mock with an empty
  // result string.
  function = new DownloadsGetFileIconFunction();
  function->SetIconExtractorForTesting(new MockIconExtractorImpl(
      expected_path, IconLoader::NORMAL, ""));
  std::string error = RunFunctionAndReturnError(function.release(), args);
  EXPECT_STREQ(download_extension_errors::kIconNotFoundError,
               error.c_str());

  // Once the download item is deleted, we should return kInvalidOperationError.
  download_item->Delete(DownloadItem::DELETE_DUE_TO_USER_DISCARD);
  error = RunFunctionAndReturnError(new DownloadsGetFileIconFunction(), args);
  EXPECT_STREQ(download_extension_errors::kInvalidOperationError,
               error.c_str());

  // Asking for icons of other (invalid) sizes is tested in the API test
  // (DownloadsApiTest).
}

// Test that we can acquire file icons for history downloads regardless of
// whether they exist or not.  If the file doesn't exist we should receive a
// generic icon from the OS/toolkit that may or may not be specific to the file
// type.
// TODO(asanka): Fails on ChromeOS (http://crbug.com/109677)
#if defined(OS_CHROMEOS)
#define MAYBE_DownloadsApi_FileIcon_History \
        FAILS_DownloadsApi_FileIcon_History
#else
#define MAYBE_DownloadsApi_FileIcon_History DownloadsApi_FileIcon_History
#endif
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
                       MAYBE_DownloadsApi_FileIcon_History) {
  base::Time current(base::Time::Now());
  FilePath real_path(
      downloads_directory().Append(FILE_PATH_LITERAL("real.txt")));
  FilePath fake_path(
      downloads_directory().Append(FILE_PATH_LITERAL("fake.txt")));
  DownloadPersistentStoreInfo history_entries[] = {
    DownloadPersistentStoreInfo(
        real_path,
        GURL("http://does.not.exist/foo"),
        GURL(),
        current, current,  // start_time == end_time == current.
        1, 1,              // received_bytes == total_bytes == 1.
        DownloadItem::COMPLETE,
        1, false),
    DownloadPersistentStoreInfo(
        fake_path,
        GURL("http://does.not.exist/bar"),
        GURL(),
        current, current,  // start_time == end_time == current.
        1, 1,              // received_bytes == total_bytes == 1.
        DownloadItem::COMPLETE,
        2, false)
  };
  std::vector<DownloadPersistentStoreInfo> entries(
      history_entries, history_entries + arraysize(history_entries));

  DownloadManager* manager = GetDownloadManager();
  manager->OnPersistentStoreQueryComplete(&entries);

  EXPECT_EQ(0, file_util::WriteFile(real_path, "", 0));
  ASSERT_TRUE(file_util::PathExists(real_path));
  ASSERT_FALSE(file_util::PathExists(fake_path));

  DownloadManager::DownloadVector all_downloads;
  manager->SearchDownloads(string16(), &all_downloads);
  ASSERT_EQ(2u, all_downloads.size());
  if (all_downloads[0]->GetId() > all_downloads[1]->GetId())
    std::swap(all_downloads[0], all_downloads[1]);
  EXPECT_EQ(real_path.value(), all_downloads[0]->GetFullPath().value());
  EXPECT_EQ(fake_path.value(), all_downloads[1]->GetFullPath().value());

  for (DownloadManager::DownloadVector::iterator iter = all_downloads.begin();
       iter != all_downloads.end();
       ++iter) {
    std::string args(base::StringPrintf("[%d, {\"size\": 32}]",
                                        (*iter)->GetId()));
    std::string result_string;
    EXPECT_TRUE(RunFunctionAndReturnString(
        new DownloadsGetFileIconFunction(), args, &result_string));
    // Note: We are checking if the result is a Data URL that has encoded
    // image/png data for an icon of a specific size. Of these, only the icon
    // size is specified in the API contract. The image type (image/png) and URL
    // type (Data) are implementation details.
    ExpectDataURLIsPNGWithSize(result_string, 32);

    // Use a MockIconExtractorImpl to test if the correct path is being passed
    // into the DownloadFileIconExtractor.
    scoped_refptr<DownloadsGetFileIconFunction> function(
        new DownloadsGetFileIconFunction());
    function->SetIconExtractorForTesting(new MockIconExtractorImpl(
        (*iter)->GetFullPath(), IconLoader::NORMAL, "hello"));
    EXPECT_TRUE(RunFunctionAndReturnString(function.release(), args,
                                           &result_string));
    EXPECT_STREQ("hello", result_string.c_str());
  }

  // The temporary files should be cleaned up when the ScopedTempDir is removed.
}
