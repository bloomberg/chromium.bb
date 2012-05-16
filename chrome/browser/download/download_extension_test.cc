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
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_persistent_store_info.h"
#include "content/test/net/url_request_slow_download_job.h"
#include "net/base/data_url.h"
#include "net/base/net_util.h"
#include "ui/gfx/codec/png_codec.h"

using content::BrowserThread;
using content::DownloadItem;
using content::DownloadManager;
using content::DownloadPersistentStoreInfo;

namespace {

// Comparator that orders download items by their ID. Can be used with
// std::sort.
struct DownloadIdComparator {
  bool operator() (DownloadItem* first, DownloadItem* second) {
    return first->GetId() < second->GetId();
  }
};

class DownloadExtensionTest : public InProcessBrowserTest {
 protected:
  // Used with CreateHistoryDownloads
  struct HistoryDownloadInfo {
    // Filename to use. CreateHistoryDownloads will append this filename to the
    // temporary downloads directory specified by downloads_directory().
    const FilePath::CharType*   filename;

    // State for the download. Note that IN_PROGRESS downloads will be created
    // as CANCELLED.
    DownloadItem::DownloadState state;

    // Danger type for the download.
    content::DownloadDangerType danger_type;
  };

  virtual Browser* current_browser() { return browser(); }

  // InProcessBrowserTest
  virtual void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
    InProcessBrowserTest::SetUpOnMainThread();
    CreateAndSetDownloadsDirectory();
    current_browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kPromptForDownload, false);
    GetDownloadManager()->RemoveAllDownloads();
  }

  virtual DownloadManager* GetDownloadManager() {
    DownloadService* download_service =
        DownloadServiceFactory::GetForProfile(current_browser()->profile());
    return download_service->GetDownloadManager();
  }

  // Creates a set of history downloads based on the provided |history_info|
  // array. |count| is the number of elements in |history_info|. On success,
  // |items| will contain |count| DownloadItems in the order that they were
  // specified in |history_info|. Returns true on success and false otherwise.
  bool CreateHistoryDownloads(const HistoryDownloadInfo* history_info,
                              size_t count,
                              DownloadManager::DownloadVector* items) {
    DownloadIdComparator download_id_comparator;
    base::Time current = base::Time::Now();
    std::vector<DownloadPersistentStoreInfo> entries;
    entries.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      DownloadPersistentStoreInfo entry(
          downloads_directory().Append(history_info[i].filename),
          GURL(), GURL(),    // URL, referrer
          current, current,  // start_time, end_time
          1, 1,              // received_bytes, total_bytes
          history_info[i].state,  // state
          i + 1,                  // db_handle
          false);                 // opened
      entries.push_back(entry);
    }
    GetDownloadManager()->OnPersistentStoreQueryComplete(&entries);
    GetDownloadManager()->GetAllDownloads(FilePath(), items);
    EXPECT_EQ(count, items->size());
    if (count != items->size())
      return false;

    // Order by ID so that they are in the order that we created them.
    std::sort(items->begin(), items->end(), download_id_comparator);
    // Set the danger type if necessary.
    for (size_t i = 0; i < count; ++i) {
      if (history_info[i].danger_type !=
          content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS) {
        items->at(i)->SetDangerType(history_info[i].danger_type);
      }
    }
    return true;
  }

  void CreateSlowTestDownloads(
      size_t count, DownloadManager::DownloadVector* items) {
    for (size_t i = 0; i < count; ++i) {
      scoped_ptr<DownloadTestObserver> observer(
          CreateInProgressDownloadObserver(1));
      GURL slow_download_url(URLRequestSlowDownloadJob::kUnknownSizeUrl);
      ui_test_utils::NavigateToURLWithDisposition(
          current_browser(), slow_download_url, CURRENT_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
      observer->WaitForFinished();
      EXPECT_EQ(
          1u, observer->NumDownloadsSeenInState(DownloadItem::IN_PROGRESS));
      // We don't expect a select file dialog.
      ASSERT_FALSE(observer->select_file_dialog_seen());
    }
    GetDownloadManager()->GetAllDownloads(FilePath(), items);
    ASSERT_EQ(count, items->size());
  }

  DownloadItem* CreateSlowTestDownload() {
    scoped_ptr<DownloadTestObserver> observer(
        CreateInProgressDownloadObserver(1));
    GURL slow_download_url(URLRequestSlowDownloadJob::kUnknownSizeUrl);
    DownloadManager* manager = GetDownloadManager();

    EXPECT_EQ(0, manager->InProgressCount());
    if (manager->InProgressCount() != 0)
      return NULL;

    ui_test_utils::NavigateToURLWithDisposition(
        current_browser(), slow_download_url, CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

    observer->WaitForFinished();
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::IN_PROGRESS));
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
        CreateDownloadObserver(1));
    GURL finish_url(URLRequestSlowDownloadJob::kFinishDownloadUrl);
    ui_test_utils::NavigateToURLWithDisposition(
        current_browser(), finish_url, NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    observer->WaitForFinished();
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  }

  DownloadTestObserver* CreateDownloadObserver(size_t download_count) {
    return new DownloadTestObserverTerminal(
        GetDownloadManager(), download_count, true,
        DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  }

  DownloadTestObserver* CreateInProgressDownloadObserver(
      size_t download_count) {
    return new DownloadTestObserverInProgress(GetDownloadManager(),
                                              download_count,
                                              true);
  }

  extension_function_test_utils::RunFunctionFlags GetFlags() {
    return current_browser()->profile()->IsOffTheRecord() ?
           extension_function_test_utils::INCLUDE_INCOGNITO :
           extension_function_test_utils::NONE;
  }

  // extension_function_test_utils::RunFunction*() only uses browser for its
  // profile(), so pass it the on-record browser so that it always uses the
  // on-record profile.

  bool RunFunction(UIThreadExtensionFunction* function,
                   const std::string& args) {
    // extension_function_test_utils::RunFunction() does not take
    // ownership of |function|.
    scoped_refptr<ExtensionFunction> function_owner(function);
    return extension_function_test_utils::RunFunction(
        function, args, browser(), GetFlags());
  }

  base::Value* RunFunctionAndReturnResult(UIThreadExtensionFunction* function,
                                          const std::string& args) {
    return extension_function_test_utils::RunFunctionAndReturnResult(
        function, args, browser(), GetFlags());
  }

  std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                        const std::string& args) {
    return extension_function_test_utils::RunFunctionAndReturnError(
        function, args, browser(), GetFlags());
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
  void CreateAndSetDownloadsDirectory() {
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());
    current_browser()->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory,
        downloads_directory_.path());
  }

  ScopedTempDir downloads_directory_;
};

class DownloadExtensionTestIncognito : public DownloadExtensionTest {
 public:
  virtual Browser* current_browser() OVERRIDE { return current_browser_; }

  virtual void SetUpOnMainThread() OVERRIDE {
    GoOnTheRecord();
    DownloadExtensionTest::SetUpOnMainThread();
    incognito_browser_ = CreateIncognitoBrowser();
    GoOffTheRecord();
    GetDownloadManager()->RemoveAllDownloads();
  }

  void GoOnTheRecord() { current_browser_ = browser(); }
  void GoOffTheRecord() { current_browser_ = incognito_browser_; }

 private:
  Browser* incognito_browser_;
  Browser* current_browser_;
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

// Cancels the underlying DownloadItem when the ScopedCancellingItem goes out of
// scope. Like a scoped_ptr, but for DownloadItems.
class ScopedCancellingItem {
 public:
  explicit ScopedCancellingItem(DownloadItem* item) : item_(item) {}
  ~ScopedCancellingItem() {
    item_->Cancel(true);
  }
  DownloadItem* operator*() { return item_; }
  DownloadItem* operator->() { return item_; }
  DownloadItem* get() { return item_; }
 private:
  DownloadItem* item_;
  DISALLOW_COPY_AND_ASSIGN(ScopedCancellingItem);
};

// Cancels all the underlying DownloadItems when the ScopedItemVectorCanceller
// goes out of scope. Generalization of ScopedCancellingItem to many
// DownloadItems.
class ScopedItemVectorCanceller {
 public:
  explicit ScopedItemVectorCanceller(DownloadManager::DownloadVector* items)
    : items_(items) {
  }
  ~ScopedItemVectorCanceller() {
    for (DownloadManager::DownloadVector::const_iterator item = items_->begin();
         item != items_->end(); ++item) {
      if ((*item)->IsInProgress())
        (*item)->Cancel(true);
    }
  }

 private:
  DownloadManager::DownloadVector* items_;
  DISALLOW_COPY_AND_ASSIGN(ScopedItemVectorCanceller);
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
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, DownloadsApi_FileIcon_Active) {
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
IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, DownloadsApi_FileIcon_History) {
  const HistoryDownloadInfo kHistoryInfo[] = {
    { FILE_PATH_LITERAL("real.txt"),
      DownloadItem::COMPLETE,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS },
    { FILE_PATH_LITERAL("fake.txt"),
      DownloadItem::COMPLETE,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS }
  };
  DownloadManager::DownloadVector all_downloads;
  ASSERT_TRUE(CreateHistoryDownloads(kHistoryInfo, arraysize(kHistoryInfo),
                                     &all_downloads));

  FilePath real_path = all_downloads[0]->GetFullPath();
  FilePath fake_path = all_downloads[1]->GetFullPath();

  EXPECT_EQ(0, file_util::WriteFile(real_path, "", 0));
  ASSERT_TRUE(file_util::PathExists(real_path));
  ASSERT_FALSE(file_util::PathExists(fake_path));

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

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, DownloadsApi_SearchEmptyQuery) {
  ScopedCancellingItem item(CreateSlowTestDownload());
  ASSERT_TRUE(item.get());

  scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
      new DownloadsSearchFunction(), "[{}]"));
  ASSERT_TRUE(result.get());
  base::ListValue* result_list = NULL;
  ASSERT_TRUE(result->GetAsList(&result_list));
  ASSERT_EQ(1UL, result_list->GetSize());
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
    DownloadsApi_SearchFilenameRegex) {
  const HistoryDownloadInfo kHistoryInfo[] = {
    { FILE_PATH_LITERAL("foobar"),
      DownloadItem::COMPLETE,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS },
    { FILE_PATH_LITERAL("baz"),
      DownloadItem::COMPLETE,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS }
  };
  DownloadManager::DownloadVector all_downloads;
  ASSERT_TRUE(CreateHistoryDownloads(kHistoryInfo, arraysize(kHistoryInfo),
                                     &all_downloads));

  scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
      new DownloadsSearchFunction(), "[{\"filenameRegex\": \"foobar\"}]"));
  ASSERT_TRUE(result.get());
  base::ListValue* result_list = NULL;
  ASSERT_TRUE(result->GetAsList(&result_list));
  ASSERT_EQ(1UL, result_list->GetSize());
  base::DictionaryValue* item_value = NULL;
  ASSERT_TRUE(result_list->GetDictionary(0, &item_value));
  int item_id = -1;
  ASSERT_TRUE(item_value->GetInteger("id", &item_id));
  ASSERT_EQ(0, item_id);
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, DownloadsApi_SearchId) {
  DownloadManager::DownloadVector items;
  CreateSlowTestDownloads(2, &items);
  ScopedItemVectorCanceller delete_items(&items);

  scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
      new DownloadsSearchFunction(), "[{\"id\": 0}]"));
  ASSERT_TRUE(result.get());
  base::ListValue* result_list = NULL;
  ASSERT_TRUE(result->GetAsList(&result_list));
  ASSERT_EQ(1UL, result_list->GetSize());
  base::DictionaryValue* item_value = NULL;
  ASSERT_TRUE(result_list->GetDictionary(0, &item_value));
  int item_id = -1;
  ASSERT_TRUE(item_value->GetInteger("id", &item_id));
  ASSERT_EQ(0, item_id);
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest,
    DownloadsApi_SearchIdAndFilename) {
  DownloadManager::DownloadVector items;
  CreateSlowTestDownloads(2, &items);
  ScopedItemVectorCanceller delete_items(&items);

  scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
      new DownloadsSearchFunction(), "[{\"id\": 0,\"filename\": \"foobar\"}]"));
  ASSERT_TRUE(result.get());
  base::ListValue* result_list = NULL;
  ASSERT_TRUE(result->GetAsList(&result_list));
  ASSERT_EQ(0UL, result_list->GetSize());
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, DownloadsApi_SearchOrderBy) {
  const HistoryDownloadInfo kHistoryInfo[] = {
    { FILE_PATH_LITERAL("zzz"),
      DownloadItem::COMPLETE,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS },
    { FILE_PATH_LITERAL("baz"),
      DownloadItem::COMPLETE,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS }
  };
  DownloadManager::DownloadVector items;
  ASSERT_TRUE(CreateHistoryDownloads(kHistoryInfo, arraysize(kHistoryInfo),
                                     &items));

  scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
      new DownloadsSearchFunction(), "[{\"orderBy\": \"filename\"}]"));
  ASSERT_TRUE(result.get());
  base::ListValue* result_list = NULL;
  ASSERT_TRUE(result->GetAsList(&result_list));
  ASSERT_EQ(2UL, result_list->GetSize());
  base::DictionaryValue* item0_value = NULL;
  base::DictionaryValue* item1_value = NULL;
  ASSERT_TRUE(result_list->GetDictionary(0, &item0_value));
  ASSERT_TRUE(result_list->GetDictionary(1, &item1_value));
  std::string item0_name, item1_name;
  ASSERT_TRUE(item0_value->GetString("filename", &item0_name));
  ASSERT_TRUE(item1_value->GetString("filename", &item1_name));
  ASSERT_GT(items[0]->GetFullPath().value(), items[1]->GetFullPath().value());
  ASSERT_LT(item0_name, item1_name);
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, DownloadsApi_SearchOrderByEmpty) {
  const HistoryDownloadInfo kHistoryInfo[] = {
    { FILE_PATH_LITERAL("zzz"),
      DownloadItem::COMPLETE,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS },
    { FILE_PATH_LITERAL("baz"),
      DownloadItem::COMPLETE,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS }
  };
  DownloadManager::DownloadVector items;
  ASSERT_TRUE(CreateHistoryDownloads(kHistoryInfo, arraysize(kHistoryInfo),
                                     &items));

  scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
      new DownloadsSearchFunction(), "[{\"orderBy\": \"\"}]"));
  ASSERT_TRUE(result.get());
  base::ListValue* result_list = NULL;
  ASSERT_TRUE(result->GetAsList(&result_list));
  ASSERT_EQ(2UL, result_list->GetSize());
  base::DictionaryValue* item0_value = NULL;
  base::DictionaryValue* item1_value = NULL;
  ASSERT_TRUE(result_list->GetDictionary(0, &item0_value));
  ASSERT_TRUE(result_list->GetDictionary(1, &item1_value));
  std::string item0_name, item1_name;
  ASSERT_TRUE(item0_value->GetString("filename", &item0_name));
  ASSERT_TRUE(item1_value->GetString("filename", &item1_name));
  ASSERT_GT(items[0]->GetFullPath().value(), items[1]->GetFullPath().value());
  ASSERT_GT(item0_name, item1_name);
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, DownloadsApi_SearchDanger) {
  const HistoryDownloadInfo kHistoryInfo[] = {
    { FILE_PATH_LITERAL("zzz"),
      DownloadItem::COMPLETE,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT },
    { FILE_PATH_LITERAL("baz"),
      DownloadItem::COMPLETE,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS }
  };
  DownloadManager::DownloadVector items;
  ASSERT_TRUE(CreateHistoryDownloads(kHistoryInfo, arraysize(kHistoryInfo),
                                     &items));

  scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
      new DownloadsSearchFunction(), "[{\"danger\": \"content\"}]"));
  ASSERT_TRUE(result.get());
  base::ListValue* result_list = NULL;
  ASSERT_TRUE(result->GetAsList(&result_list));
  ASSERT_EQ(1UL, result_list->GetSize());
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, DownloadsApi_SearchState) {
  DownloadManager::DownloadVector items;
  CreateSlowTestDownloads(2, &items);
  ScopedItemVectorCanceller delete_items(&items);

  items[0]->Cancel(true);

  scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
      new DownloadsSearchFunction(), "[{\"state\": \"in_progress\"}]"));
  ASSERT_TRUE(result.get());
  base::ListValue* result_list = NULL;
  ASSERT_TRUE(result->GetAsList(&result_list));
  ASSERT_EQ(1UL, result_list->GetSize());
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, DownloadsApi_SearchLimit) {
  DownloadManager::DownloadVector items;
  CreateSlowTestDownloads(2, &items);
  ScopedItemVectorCanceller delete_items(&items);

  scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
      new DownloadsSearchFunction(), "[{\"limit\": 1}]"));
  ASSERT_TRUE(result.get());
  base::ListValue* result_list = NULL;
  ASSERT_TRUE(result->GetAsList(&result_list));
  ASSERT_EQ(1UL, result_list->GetSize());
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, DownloadsApi_SearchInvalid) {
  std::string error = RunFunctionAndReturnError(
      new DownloadsSearchFunction(), "[{\"filenameRegex\": \"(\"}]");
  EXPECT_STREQ(download_extension_errors::kInvalidFilterError,
      error.c_str());
  error = RunFunctionAndReturnError(
      new DownloadsSearchFunction(), "[{\"danger\": \"goat\"}]");
  EXPECT_STREQ(download_extension_errors::kInvalidDangerTypeError,
      error.c_str());
  error = RunFunctionAndReturnError(
      new DownloadsSearchFunction(), "[{\"state\": \"goat\"}]");
  EXPECT_STREQ(download_extension_errors::kInvalidStateError,
      error.c_str());
  error = RunFunctionAndReturnError(
      new DownloadsSearchFunction(), "[{\"orderBy\": \"goat\"}]");
  EXPECT_STREQ(download_extension_errors::kInvalidOrderByError,
      error.c_str());
  error = RunFunctionAndReturnError(
      new DownloadsSearchFunction(), "[{\"limit\": -1}]");
  EXPECT_STREQ(download_extension_errors::kInvalidQueryLimit,
      error.c_str());
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTest, DownloadsApi_SearchPlural) {
  const HistoryDownloadInfo kHistoryInfo[] = {
    { FILE_PATH_LITERAL("aaa"),
      DownloadItem::CANCELLED,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS },
    { FILE_PATH_LITERAL("zzz"),
      DownloadItem::COMPLETE,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT },
    { FILE_PATH_LITERAL("baz"),
      DownloadItem::COMPLETE,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT },
  };
  DownloadManager::DownloadVector items;
  ASSERT_TRUE(CreateHistoryDownloads(kHistoryInfo, arraysize(kHistoryInfo),
                                     &items));

  scoped_ptr<base::Value> result(RunFunctionAndReturnResult(
      new DownloadsSearchFunction(), "[{"
      "\"state\": \"complete\", "
      "\"danger\": \"content\", "
      "\"orderBy\": \"filename\", "
      "\"limit\": 1}]"));
  ASSERT_TRUE(result.get());
  base::ListValue* result_list = NULL;
  ASSERT_TRUE(result->GetAsList(&result_list));
  ASSERT_EQ(1UL, result_list->GetSize());
  base::DictionaryValue* item_value = NULL;
  ASSERT_TRUE(result_list->GetDictionary(0, &item_value));
  FilePath::StringType item_name;
  ASSERT_TRUE(item_value->GetString("filename", &item_name));
  ASSERT_EQ(items[2]->GetFullPath().value(), item_name);
}

IN_PROC_BROWSER_TEST_F(DownloadExtensionTestIncognito,
                       DownloadsApi_SearchIncognito) {
  scoped_ptr<base::Value> result_value;
  base::ListValue* result_list = NULL;
  base::DictionaryValue* result_dict = NULL;
  FilePath::StringType filename;
  bool is_incognito = false;
  std::string error;
  std::string on_item_arg;
  std::string off_item_arg;
  std::string result_string;

  // Set up one on-record item and one off-record item.

  GoOnTheRecord();
  DownloadItem* on_item = CreateSlowTestDownload();
  ASSERT_TRUE(on_item);
  ASSERT_FALSE(on_item->IsOtr());
  on_item_arg = DownloadItemIdAsArgList(on_item);

  GoOffTheRecord();
  DownloadItem* off_item = CreateSlowTestDownload();
  ASSERT_TRUE(off_item);
  ASSERT_TRUE(off_item->IsOtr());
  ASSERT_TRUE(on_item->GetFullPath() != off_item->GetFullPath());
  off_item_arg = DownloadItemIdAsArgList(off_item);

  // Extensions running in the incognito window should have access to both
  // items because the Test extension is in spanning mode.
  result_value.reset(RunFunctionAndReturnResult(
      new DownloadsSearchFunction(), "[{}]"));
  ASSERT_TRUE(result_value.get());
  ASSERT_TRUE(result_value->GetAsList(&result_list));
  ASSERT_EQ(2UL, result_list->GetSize());
  ASSERT_TRUE(result_list->GetDictionary(0, &result_dict));
  ASSERT_TRUE(result_dict->GetString("filename", &filename));
  ASSERT_TRUE(result_dict->GetBoolean("incognito", &is_incognito));
  EXPECT_TRUE(on_item->GetFullPath() == FilePath(filename));
  EXPECT_FALSE(is_incognito);
  ASSERT_TRUE(result_list->GetDictionary(1, &result_dict));
  ASSERT_TRUE(result_dict->GetString("filename", &filename));
  ASSERT_TRUE(result_dict->GetBoolean("incognito", &is_incognito));
  EXPECT_TRUE(off_item->GetFullPath() == FilePath(filename));
  EXPECT_TRUE(is_incognito);

  // Extensions running in the on-record window should have access only to the
  // on-record item.
  GoOnTheRecord();
  result_value.reset(RunFunctionAndReturnResult(
      new DownloadsSearchFunction(), "[{}]"));
  ASSERT_TRUE(result_value.get());
  ASSERT_TRUE(result_value->GetAsList(&result_list));
  ASSERT_EQ(1UL, result_list->GetSize());
  ASSERT_TRUE(result_list->GetDictionary(0, &result_dict));
  ASSERT_TRUE(result_dict->GetString("filename", &filename));
  EXPECT_TRUE(on_item->GetFullPath() == FilePath(filename));
  ASSERT_TRUE(result_dict->GetBoolean("incognito", &is_incognito));
  EXPECT_FALSE(is_incognito);

  // Pausing/Resuming the off-record item while on the record should return an
  // error. Cancelling "non-existent" downloads is not an error.
  error = RunFunctionAndReturnError(new DownloadsPauseFunction(), off_item_arg);
  EXPECT_STREQ(download_extension_errors::kInvalidOperationError,
               error.c_str());
  error = RunFunctionAndReturnError(new DownloadsResumeFunction(),
                                    off_item_arg);
  EXPECT_STREQ(download_extension_errors::kInvalidOperationError,
               error.c_str());
  error = RunFunctionAndReturnError(
      new DownloadsGetFileIconFunction(),
      base::StringPrintf("[%d, {}]", off_item->GetId()));
  EXPECT_STREQ(download_extension_errors::kInvalidOperationError,
               error.c_str());

  // TODO(benjhayden): Test incognito_split_mode() extension.
  // TODO(benjhayden): Test download(), onCreated, onChanged, onErased.

  GoOffTheRecord();

  // Do the FileIcon test for both the on- and off-items while off the record.
  // NOTE(benjhayden): This does not include the FileIcon test from history,
  // just active downloads. This shouldn't be a problem.
  EXPECT_TRUE(RunFunctionAndReturnString(
      new DownloadsGetFileIconFunction(),
      base::StringPrintf("[%d, {}]", on_item->GetId()), &result_string));
  EXPECT_TRUE(RunFunctionAndReturnString(
      new DownloadsGetFileIconFunction(),
      base::StringPrintf("[%d, {}]", off_item->GetId()), &result_string));

  // Do the pause/resume/cancel test for both the on- and off-items while off
  // the record.
  EXPECT_TRUE(RunFunction(new DownloadsPauseFunction(), on_item_arg));
  EXPECT_TRUE(on_item->IsPaused());
  EXPECT_TRUE(RunFunction(new DownloadsPauseFunction(), on_item_arg));
  EXPECT_TRUE(on_item->IsPaused());
  EXPECT_TRUE(RunFunction(new DownloadsResumeFunction(), on_item_arg));
  EXPECT_FALSE(on_item->IsPaused());
  EXPECT_TRUE(RunFunction(new DownloadsResumeFunction(), on_item_arg));
  EXPECT_FALSE(on_item->IsPaused());
  EXPECT_TRUE(RunFunction(new DownloadsPauseFunction(), on_item_arg));
  EXPECT_TRUE(on_item->IsPaused());
  EXPECT_TRUE(RunFunction(new DownloadsCancelFunction(), on_item_arg));
  EXPECT_TRUE(on_item->IsCancelled());
  EXPECT_TRUE(RunFunction(new DownloadsCancelFunction(), on_item_arg));
  EXPECT_TRUE(on_item->IsCancelled());
  error = RunFunctionAndReturnError(new DownloadsPauseFunction(), on_item_arg);
  EXPECT_STREQ(download_extension_errors::kInvalidOperationError,
               error.c_str());
  error = RunFunctionAndReturnError(new DownloadsResumeFunction(), on_item_arg);
  EXPECT_STREQ(download_extension_errors::kInvalidOperationError,
               error.c_str());
  EXPECT_TRUE(RunFunction(new DownloadsPauseFunction(), off_item_arg));
  EXPECT_TRUE(off_item->IsPaused());
  EXPECT_TRUE(RunFunction(new DownloadsPauseFunction(), off_item_arg));
  EXPECT_TRUE(off_item->IsPaused());
  EXPECT_TRUE(RunFunction(new DownloadsResumeFunction(), off_item_arg));
  EXPECT_FALSE(off_item->IsPaused());
  EXPECT_TRUE(RunFunction(new DownloadsResumeFunction(), off_item_arg));
  EXPECT_FALSE(off_item->IsPaused());
  EXPECT_TRUE(RunFunction(new DownloadsPauseFunction(), off_item_arg));
  EXPECT_TRUE(off_item->IsPaused());
  EXPECT_TRUE(RunFunction(new DownloadsCancelFunction(), off_item_arg));
  EXPECT_TRUE(off_item->IsCancelled());
  EXPECT_TRUE(RunFunction(new DownloadsCancelFunction(), off_item_arg));
  EXPECT_TRUE(off_item->IsCancelled());
  error = RunFunctionAndReturnError(new DownloadsPauseFunction(),
                                    off_item_arg);
  EXPECT_STREQ(download_extension_errors::kInvalidOperationError,
               error.c_str());
  error = RunFunctionAndReturnError(new DownloadsResumeFunction(),
                                    off_item_arg);
  EXPECT_STREQ(download_extension_errors::kInvalidOperationError,
               error.c_str());
}
