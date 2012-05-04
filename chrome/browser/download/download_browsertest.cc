// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/test/test_file_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/download_test_observer.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_persistent_store_info.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/page_transition_types.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "content/test/net/url_request_slow_download_job.h"
#include "content/test/test_file_error_injector.h"
#include "content/test/test_navigation_observer.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::DownloadItem;
using content::DownloadManager;
using content::DownloadPersistentStoreInfo;
using content::DownloadUrlParameters;
using content::WebContents;

namespace {

// IDs and paths of CRX files used in tests.
const char kGoodCrxId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const FilePath kGoodCrxPath(FILE_PATH_LITERAL("extensions/good.crx"));

const char kLargeThemeCrxId[] = "pjpgmfcmabopnnfonnhmdjglfpjjfkbf";
const FilePath kLargeThemePath(FILE_PATH_LITERAL("extensions/theme2.crx"));

class PickSuggestedFileDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit PickSuggestedFileDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile) {
    SetDownloadManager(
        DownloadServiceFactory::GetForProfile(profile)->GetDownloadManager());
  }

  virtual void ChooseDownloadPath(WebContents* web_contents,
                                  const FilePath& suggested_path,
                                  int32 download_id) OVERRIDE {
    if (download_manager_)
      download_manager_->FileSelected(suggested_path, download_id);
  }

 protected:
  virtual ~PickSuggestedFileDelegate() {}
};

// Get History Information.
class DownloadsHistoryDataCollector {
 public:
  DownloadsHistoryDataCollector(int64 download_db_handle,
                                DownloadManager* manager)
      : result_valid_(false),
        download_db_handle_(download_db_handle) {
    HistoryService* hs =
        Profile::FromBrowserContext(manager->GetBrowserContext())->
            GetHistoryService(Profile::EXPLICIT_ACCESS);
    DCHECK(hs);
    hs->QueryDownloads(
        &callback_consumer_,
        base::Bind(&DownloadsHistoryDataCollector::OnQueryDownloadsComplete,
                   base::Unretained(this)));

    // TODO(rdsmith): Move message loop out of constructor.
    // Cannot complete immediately because the history backend runs on a
    // separate thread, so we can assume that the RunMessageLoop below will
    // be exited by the Quit in OnQueryDownloadsComplete.
    ui_test_utils::RunMessageLoop();
  }

  bool GetDownloadsHistoryEntry(DownloadPersistentStoreInfo* result) {
    DCHECK(result);
    *result = result_;
    return result_valid_;
  }

 private:
  void OnQueryDownloadsComplete(
      std::vector<DownloadPersistentStoreInfo>* entries) {
    result_valid_ = false;
    for (std::vector<DownloadPersistentStoreInfo>::const_iterator it =
             entries->begin();
         it != entries->end(); ++it) {
      if (it->db_handle == download_db_handle_) {
        result_ = *it;
        result_valid_ = true;
      }
    }
    MessageLoopForUI::current()->Quit();
  }

  DownloadPersistentStoreInfo result_;
  bool result_valid_;
  int64 download_db_handle_;
  CancelableRequestConsumer callback_consumer_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsHistoryDataCollector);
};

// Mock that simulates a permissions dialog where the user denies
// permission to install.  TODO(skerner): This could be shared with
// extensions tests.  Find a common place for this class.
class MockAbortExtensionInstallUI : public ExtensionInstallUI {
 public:
  MockAbortExtensionInstallUI() : ExtensionInstallUI(NULL) {}

  // Simulate a user abort on an extension installation.
  virtual void ConfirmInstall(Delegate* delegate, const Extension* extension) {
    delegate->InstallUIAbort(true);
    MessageLoopForUI::current()->Quit();
  }

  virtual void OnInstallSuccess(const Extension* extension, SkBitmap* icon) {}
  virtual void OnInstallFailure(const string16& error) {}
};

// Mock that simulates a permissions dialog where the user allows
// installation.
class MockAutoConfirmExtensionInstallUI : public ExtensionInstallUI {
 public:
  explicit MockAutoConfirmExtensionInstallUI(Profile* profile)
      : ExtensionInstallUI(profile) {}

  // Proceed without confirmation prompt.
  virtual void ConfirmInstall(Delegate* delegate, const Extension* extension) {
    delegate->InstallUIProceed();
  }

  virtual void OnInstallSuccess(const Extension* extension, SkBitmap* icon) {}
  virtual void OnInstallFailure(const string16& error) {}
};

static DownloadManager* DownloadManagerForBrowser(Browser* browser) {
  return DownloadServiceFactory::GetForProfile(browser->profile())
      ->GetDownloadManager();
}

class TestRenderViewContextMenu : public RenderViewContextMenu {
 public:
  TestRenderViewContextMenu(WebContents* web_contents,
                            const content::ContextMenuParams& params)
      : RenderViewContextMenu(web_contents, params) {
  }
  virtual ~TestRenderViewContextMenu() {}

 private:
  virtual void PlatformInit() {}
  virtual void PlatformCancel() {}
  virtual bool GetAcceleratorForCommandId(int, ui::Accelerator*) {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(TestRenderViewContextMenu);
};

}  // namespace

// While an object of this class exists, it will mock out download
// opening for all downloads created on the specified download manager.
class MockDownloadOpeningObserver : public DownloadManager::Observer {
 public:
  explicit MockDownloadOpeningObserver(DownloadManager* manager)
      : download_manager_(manager) {
    download_manager_->AddObserver(this);
  }

  ~MockDownloadOpeningObserver() {
    download_manager_->RemoveObserver(this);
  }

  // DownloadManager::Observer
  virtual void ModelChanged(DownloadManager* manager) OVERRIDE {
    DCHECK_EQ(manager, download_manager_);
    std::vector<DownloadItem*> downloads;
    download_manager_->SearchDownloads(string16(), &downloads);

    for (std::vector<DownloadItem*>::iterator it = downloads.begin();
         it != downloads.end(); ++it) {
      (*it)->MockDownloadOpenForTesting();
    }
  }

 private:
  DownloadManager* download_manager_;

  DISALLOW_COPY_AND_ASSIGN(MockDownloadOpeningObserver);
};

class DownloadTest : public InProcessBrowserTest {
 public:
  enum SelectExpectation {
    EXPECT_NO_SELECT_DIALOG = -1,
    EXPECT_NOTHING,
    EXPECT_SELECT_DIALOG
  };

  // Choice of navigation or direct fetch.  Used by |DownloadFileCheckErrors()|.
  enum DownloadMethod {
    DOWNLOAD_NAVIGATE,
    DOWNLOAD_DIRECT
  };

  // Information passed in to |DownloadFileCheckErrors()|.
  struct DownloadInfo {
    const char* url_name;  // URL for the download.
    DownloadMethod download_method;  // Navigation or Direct.
    // Download interrupt reason (NONE is OK).
    content::DownloadInterruptReason reason;
    bool show_download_item;  // True if the download item appears on the shelf.
    bool should_redirect_to_documents;  // True if we save it in "My Documents".
  };

  struct FileErrorInjectInfo {
    DownloadInfo download_info;
    content::TestFileErrorInjector::FileErrorInfo error_info;
  };

  DownloadTest() {
    EnableDOMAutomation();
  }

  void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
    ASSERT_TRUE(InitialSetup());
  }

  // Returning false indicates a failure of the setup, and should be asserted
  // in the caller.
  virtual bool InitialSetup() {
    bool have_test_dir = PathService::Get(chrome::DIR_TEST_DATA, &test_dir_);
    EXPECT_TRUE(have_test_dir);
    if (!have_test_dir)
      return false;

    // Sanity check default values for window / tab count and shelf visibility.
    int window_count = BrowserList::size();
    EXPECT_EQ(1, window_count);
    EXPECT_EQ(1, browser()->tab_count());
    EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

    // Set up the temporary download folder.
    bool created_downloads_dir = CreateAndSetDownloadsDirectory(browser());
    EXPECT_TRUE(created_downloads_dir);
    if (!created_downloads_dir)
      return false;
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kPromptForDownload, false);

    DownloadManager* manager = DownloadManagerForBrowser(browser());
    DownloadPrefs::FromDownloadManager(manager)->ResetAutoOpen();
    manager->RemoveAllDownloads();

    return true;
  }

 protected:

  enum SizeTestType {
    SIZE_TEST_TYPE_KNOWN,
    SIZE_TEST_TYPE_UNKNOWN,
  };

  // Location of the file source (the place from which it is downloaded).
  FilePath OriginFile(FilePath file) {
    return test_dir_.Append(file);
  }

  // Location of the file destination (place to which it is downloaded).
  FilePath DestinationFile(Browser* browser, FilePath file) {
    return GetDownloadDirectory(browser).Append(file);
  }

  // Must be called after browser creation.  Creates a temporary
  // directory for downloads that is auto-deleted on destruction.
  // Returning false indicates a failure of the function, and should be asserted
  // in the caller.
  bool CreateAndSetDownloadsDirectory(Browser* browser) {
    if (!browser)
      return false;

    if (!downloads_directory_.CreateUniqueTempDir())
      return false;

    browser->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory,
        downloads_directory_.path());

    return true;
  }

  DownloadPrefs* GetDownloadPrefs(Browser* browser) {
    return DownloadPrefs::FromDownloadManager(
        DownloadManagerForBrowser(browser));
  }

  FilePath GetDownloadDirectory(Browser* browser) {
    return GetDownloadPrefs(browser)->download_path();
  }

  // Create a DownloadTestObserverTerminal that will wait for the
  // specified number of downloads to finish.
  DownloadTestObserver* CreateWaiter(Browser* browser, int num_downloads) {
    DownloadManager* download_manager = DownloadManagerForBrowser(browser);
    return new DownloadTestObserverTerminal(
        download_manager, num_downloads,
        true,                   // Bail on select file
        DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  }

  // Create a DownloadTestObserverInProgress that will wait for the
  // specified number of downloads to start.
  DownloadTestObserver* CreateInProgressWaiter(Browser* browser,
                                               int num_downloads) {
    DownloadManager* download_manager = DownloadManagerForBrowser(browser);
    return new DownloadTestObserverInProgress(
        download_manager, num_downloads, true);  // Bail on select file.
  }

  // Create a DownloadTestObserverTerminal that will wait for the
  // specified number of downloads to finish, or for
  // a dangerous download warning to be shown.
  DownloadTestObserver* DangerousDownloadWaiter(
      Browser* browser,
      int num_downloads,
      DownloadTestObserver::DangerousDownloadAction dangerous_download_action) {
    DownloadManager* download_manager = DownloadManagerForBrowser(browser);
    return new DownloadTestObserverTerminal(
        download_manager, num_downloads,
        true,                         // Bail on select file
        dangerous_download_action);
  }

  void CheckDownloadStatesForBrowser(Browser* browser,
                                     size_t num,
                                     DownloadItem::DownloadState state) {
    std::vector<DownloadItem*> download_items;
    GetDownloads(browser, &download_items);

    EXPECT_EQ(num, download_items.size());

    for (size_t i = 0; i < download_items.size(); ++i) {
      EXPECT_EQ(state, download_items[i]->GetState()) << " Item " << i;
    }
  }

  void CheckDownloadStates(size_t num, DownloadItem::DownloadState state) {
    CheckDownloadStatesForBrowser(browser(), num, state);
  }

  // Download |url|, then wait for the download to finish.
  // |disposition| indicates where the navigation occurs (current tab, new
  // foreground tab, etc).
  // |expectation| indicates whether or not a Select File dialog should be
  // open when the download is finished, or if we don't care.
  // If the dialog appears, the routine exits.  The only effect |expectation|
  // has is whether or not the test succeeds.
  // |browser_test_flags| indicate what to wait for, and is an OR of 0 or more
  // values in the ui_test_utils::BrowserTestWaitFlags enum.
  void DownloadAndWaitWithDisposition(Browser* browser,
                                      const GURL& url,
                                      WindowOpenDisposition disposition,
                                      SelectExpectation expectation,
                                      int browser_test_flags) {
    // Setup notification, navigate, and block.
    scoped_ptr<DownloadTestObserver> observer(CreateWaiter(browser, 1));
    // This call will block until the condition specified by
    // |browser_test_flags|, but will not wait for the download to finish.
    ui_test_utils::NavigateToURLWithDisposition(browser,
                                                url,
                                                disposition,
                                                browser_test_flags);
    // Waits for the download to complete.
    observer->WaitForFinished();
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));

    // If specified, check the state of the select file dialog.
    if (expectation != EXPECT_NOTHING) {
      EXPECT_EQ(expectation == EXPECT_SELECT_DIALOG,
                observer->select_file_dialog_seen());
    }
  }

  // Download a file in the current tab, then wait for the download to finish.
  void DownloadAndWait(Browser* browser,
                       const GURL& url,
                       SelectExpectation expectation) {
    DownloadAndWaitWithDisposition(
        browser,
        url,
        CURRENT_TAB,
        expectation,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  }

  // Should only be called when the download is known to have finished
  // (in error or not).
  // Returning false indicates a failure of the function, and should be asserted
  // in the caller.
  bool CheckDownload(Browser* browser,
                     const FilePath& downloaded_filename,
                     const FilePath& origin_filename) {
    // Find the path to which the data will be downloaded.
    FilePath downloaded_file(DestinationFile(browser, downloaded_filename));

    // Find the origin path (from which the data comes).
    FilePath origin_file(OriginFile(origin_filename));
    return CheckDownloadFullPaths(browser, downloaded_file, origin_file);
  }

  // A version of CheckDownload that allows complete path specification.
  bool CheckDownloadFullPaths(Browser* browser,
                              const FilePath& downloaded_file,
                              const FilePath& origin_file) {
    bool origin_file_exists = file_util::PathExists(origin_file);
    EXPECT_TRUE(origin_file_exists);
    if (!origin_file_exists)
      return false;

    // Confirm the downloaded data file exists.
    bool downloaded_file_exists = file_util::PathExists(downloaded_file);
    EXPECT_TRUE(downloaded_file_exists);
    if (!downloaded_file_exists)
      return false;

    int64 origin_file_size = 0;
    EXPECT_TRUE(file_util::GetFileSize(origin_file, &origin_file_size));
    std::string original_file_contents;
    EXPECT_TRUE(
        file_util::ReadFileToString(origin_file, &original_file_contents));
    EXPECT_TRUE(
        VerifyFile(downloaded_file, original_file_contents, origin_file_size));

    // Delete the downloaded copy of the file.
    bool downloaded_file_deleted =
        file_util::DieFileDie(downloaded_file, false);
    EXPECT_TRUE(downloaded_file_deleted);
    return downloaded_file_deleted;
  }

  bool RunSizeTest(Browser* browser,
                   SizeTestType type,
                   const std::string& partial_indication,
                   const std::string& total_indication) {
    EXPECT_TRUE(type == SIZE_TEST_TYPE_UNKNOWN || type == SIZE_TEST_TYPE_KNOWN);
    if (type != SIZE_TEST_TYPE_KNOWN && type != SIZE_TEST_TYPE_UNKNOWN)
      return false;
    GURL url(type == SIZE_TEST_TYPE_KNOWN ?
             URLRequestSlowDownloadJob::kKnownSizeUrl :
             URLRequestSlowDownloadJob::kUnknownSizeUrl);

    // TODO(ahendrickson) -- |expected_title_in_progress| and
    // |expected_title_finished| need to be checked.
    FilePath filename;
    net::FileURLToFilePath(url, &filename);
    string16 expected_title_in_progress(
        ASCIIToUTF16(partial_indication) + filename.LossyDisplayName());
    string16 expected_title_finished(
        ASCIIToUTF16(total_indication) + filename.LossyDisplayName());

    // Download a partial web page in a background tab and wait.
    // The mock system will not complete until it gets a special URL.
    scoped_ptr<DownloadTestObserver> observer(CreateWaiter(browser, 1));
    ui_test_utils::NavigateToURL(browser, url);

    // TODO(ahendrickson): check download status text before downloading.
    // Need to:
    //  - Add a member function to the |DownloadShelf| interface class, that
    //    indicates how many members it has.
    //  - Add a member function to |DownloadShelf| to get the status text
    //    of a given member (for example, via the name in |DownloadItemView|'s
    //    GetAccessibleState() member function), by index.
    //  - Iterate over browser->window()->GetDownloadShelf()'s members
    //    to see if any match the status text we want.  Start with the last one.

    // Allow the request to finish.  We do this by loading a second URL in a
    // separate tab.
    GURL finish_url(URLRequestSlowDownloadJob::kFinishDownloadUrl);
    ui_test_utils::NavigateToURLWithDisposition(
        browser,
        finish_url,
        NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    observer->WaitForFinished();
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
    CheckDownloadStatesForBrowser(browser, 1, DownloadItem::COMPLETE);

    EXPECT_EQ(2, browser->tab_count());

    // TODO(ahendrickson): check download status text after downloading.

    FilePath basefilename(filename.BaseName());
    net::FileURLToFilePath(url, &filename);
    FilePath download_path = downloads_directory_.path().Append(basefilename);
    EXPECT_TRUE(browser->window()->IsDownloadShelfVisible());

    bool downloaded_path_exists = file_util::PathExists(download_path);
    EXPECT_TRUE(downloaded_path_exists);
    if (!downloaded_path_exists)
      return false;

    // Check the file contents.
    size_t file_size = URLRequestSlowDownloadJob::kFirstDownloadSize +
                       URLRequestSlowDownloadJob::kSecondDownloadSize;
    std::string expected_contents(file_size, '*');
    EXPECT_TRUE(VerifyFile(download_path, expected_contents, file_size));

    // Delete the file we just downloaded.
    EXPECT_TRUE(file_util::DieFileDie(download_path, true));
    EXPECT_FALSE(file_util::PathExists(download_path));

    return true;
  }

  void GetDownloads(Browser* browser, std::vector<DownloadItem*>* downloads) {
    DCHECK(downloads);
    DownloadManager* manager = DownloadManagerForBrowser(browser);
    manager->SearchDownloads(string16(), downloads);
  }

  static void ExpectWindowCountAfterDownload(size_t expected) {
    EXPECT_EQ(expected, BrowserList::size());
  }

  // Arrange for select file calls on the given browser from the
  // download manager to always choose the suggested file.
  void NullSelectFile(Browser* browser) {
    PickSuggestedFileDelegate* new_delegate =
        new PickSuggestedFileDelegate(browser->profile());

    // Gives ownership to DownloadService.
    DownloadServiceFactory::GetForProfile(
        browser->profile())->SetDownloadManagerDelegateForTesting(new_delegate);
  }

  // Checks that |path| is has |file_size| bytes, and matches the |value|
  // string.
  bool VerifyFile(const FilePath& path,
                  const std::string& value,
                  const int64 file_size) {
    std::string file_contents;

    bool read = file_util::ReadFileToString(path, &file_contents);
    EXPECT_TRUE(read) << "Failed reading file: " << path.value() << std::endl;
    if (!read)
      return false;  // Couldn't read the file.

    // Note: we don't handle really large files (more than size_t can hold)
    // so we will fail in that case.
    size_t expected_size = static_cast<size_t>(file_size);

    // Check the size.
    EXPECT_EQ(expected_size, file_contents.size());
    if (expected_size != file_contents.size())
      return false;

    // Check the contents.
    EXPECT_EQ(value, file_contents);
    if (memcmp(file_contents.c_str(), value.c_str(), expected_size) != 0)
      return false;

    return true;
  }

  // Attempts to download a file, based on information in |download_info|.
  // If a Select File dialog opens, will automatically choose the default.
  void DownloadFilesCheckErrorsSetup() {
    ASSERT_TRUE(test_server()->Start());
    std::vector<DownloadItem*> download_items;
    GetDownloads(browser(), &download_items);
    ASSERT_TRUE(download_items.empty());

    NullSelectFile(browser());
  }

  void DownloadFilesCheckErrorsLoopBody(const DownloadInfo& download_info,
                                        size_t i) {
    std::stringstream s;
    s << " " << __FUNCTION__ << "()"
      << " index = " << i
      << " url = '" << download_info.url_name << "'"
      << " method = "
      << ((download_info.download_method == DOWNLOAD_DIRECT) ?
          "DOWNLOAD_DIRECT" : "DOWNLOAD_NAVIGATE")
      << " show_item = " << download_info.show_download_item
      << " reason = "
      << InterruptReasonDebugString(download_info.reason);

    std::vector<DownloadItem*> download_items;
    GetDownloads(browser(), &download_items);
    size_t downloads_expected = download_items.size();

    std::string server_path = "files/downloads/";
    server_path += download_info.url_name;
    GURL url = test_server()->GetURL(server_path);
    ASSERT_TRUE(url.is_valid()) << s.str();

    DownloadManager* download_manager = DownloadManagerForBrowser(browser());
    WebContents* web_contents = browser()->GetSelectedWebContents();
    ASSERT_TRUE(web_contents) << s.str();

    scoped_ptr<DownloadTestObserver> observer(
        new DownloadTestObserverTerminal(
            download_manager,
            1,
            false,  // Don't bail on select file.
            DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

    if (download_info.download_method == DOWNLOAD_DIRECT) {
      // Go directly to download.  Don't wait for navigation.
      content::DownloadSaveInfo save_info;
      // NOTE: |prompt_for_save_location| may change during the download.
      save_info.prompt_for_save_location = false;

      scoped_refptr<DownloadTestItemCreationObserver> creation_observer(
          new DownloadTestItemCreationObserver);

      scoped_ptr<DownloadUrlParameters> params(
          DownloadUrlParameters::FromWebContents(web_contents, url, save_info));
      params->set_callback(creation_observer->callback());
      DownloadManagerForBrowser(browser())->DownloadUrl(params.Pass());

      // Wait until the item is created, or we have determined that it
      // won't be.
      creation_observer->WaitForDownloadItemCreation();

      int32 invalid_id = content::DownloadId::Invalid().local();
      EXPECT_EQ(download_info.show_download_item,
                creation_observer->succeeded());
      if (download_info.show_download_item) {
        EXPECT_EQ(net::OK, creation_observer->error());
        EXPECT_NE(invalid_id, creation_observer->download_id().local());
      } else {
        EXPECT_NE(net::OK, creation_observer->error());
        EXPECT_EQ(invalid_id, creation_observer->download_id().local());
      }
    } else {
      // Navigate to URL normally, wait until done.
      ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                                url,
                                                                1);
    }

    if (download_info.show_download_item) {
      downloads_expected++;
      observer->WaitForFinished();
      DownloadItem::DownloadState final_state =
          (download_info.reason == content::DOWNLOAD_INTERRUPT_REASON_NONE) ?
              DownloadItem::COMPLETE :
              DownloadItem::INTERRUPTED;
      EXPECT_EQ(1u, observer->NumDownloadsSeenInState(final_state));
    }

    // Wait till the |DownloadFile|s are destroyed.
    ui_test_utils::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
    ui_test_utils::RunAllPendingInMessageLoop(content::BrowserThread::UI);

    // Validate that the correct files were downloaded.
    download_items.clear();
    GetDownloads(browser(), &download_items);
    ASSERT_EQ(downloads_expected, download_items.size()) << s.str();

    if (download_info.show_download_item) {
      // Find the last download item.
      DownloadItem* item = download_items[0];
      for (size_t d = 1; d < downloads_expected; ++d) {
        if (download_items[d]->GetStartTime() > item->GetStartTime())
          item = download_items[d];
      }

      ASSERT_EQ(url, item->GetOriginalUrl()) << s.str();

      ASSERT_EQ(download_info.reason, item->GetLastReason()) << s.str();

      if (item->GetState() == content::DownloadItem::COMPLETE) {
        // Clean up the file, in case it ended up in the My Documents folder.
        FilePath destination_folder = GetDownloadDirectory(browser());
        FilePath my_downloaded_file = item->GetTargetFilePath();
        EXPECT_TRUE(file_util::PathExists(my_downloaded_file));
        EXPECT_TRUE(file_util::Delete(my_downloaded_file, false));

        EXPECT_EQ(download_info.should_redirect_to_documents ?
                      std::string::npos :
                      0u,
                  my_downloaded_file.value().find(destination_folder.value()));
        if (download_info.should_redirect_to_documents) {
          // If it's not where we asked it to be, it should be in the
          // My Documents folder.
          FilePath my_docs_folder;
          EXPECT_TRUE(PathService::Get(chrome::DIR_USER_DOCUMENTS,
                                       &my_docs_folder));
          EXPECT_EQ(0u,
                    my_downloaded_file.value().find(my_docs_folder.value()));
        }
      }
    }
  }

  // Attempts to download a set of files, based on information in the
  // |download_info| array.  |count| is the number of files.
  // If a Select File dialog appears, it will choose the default and return
  // immediately.
  void DownloadFilesCheckErrors(size_t count, DownloadInfo* download_info) {
    DownloadFilesCheckErrorsSetup();

    for (size_t i = 0; i < count; ++i) {
      DownloadFilesCheckErrorsLoopBody(download_info[i], i);
    }
  }

  void DownloadInsertFilesErrorCheckErrorsLoopBody(
      scoped_refptr<content::TestFileErrorInjector> injector,
      const FileErrorInjectInfo& info,
      size_t i) {
    std::stringstream s;
    s << " " << __FUNCTION__ << "()"
      << " index = " << i
      << " url = " << info.error_info.url
      << " operation code = " <<
          content::TestFileErrorInjector::DebugString(
              info.error_info.code)
      << " instance = " << info.error_info.operation_instance
      << " error = " << net::ErrorToString(
         info.error_info.net_error);

    injector->ClearErrors();
    injector->AddError(info.error_info);

    injector->InjectErrors();

    DownloadFilesCheckErrorsLoopBody(info.download_info, i);

    size_t expected_successes = info.download_info.show_download_item ? 1u : 0u;
    EXPECT_EQ(expected_successes, injector->TotalFileCount()) << s.str();
    EXPECT_EQ(0u, injector->CurrentFileCount()) << s.str();

    if (info.download_info.show_download_item)
      EXPECT_TRUE(injector->HadFile(GURL(info.error_info.url))) << s.str();
  }

  void DownloadInsertFilesErrorCheckErrors(size_t count,
                                           FileErrorInjectInfo* info) {
    DownloadFilesCheckErrorsSetup();

    // Set up file failures.
    scoped_refptr<content::TestFileErrorInjector> injector(
        content::TestFileErrorInjector::Create());

    for (size_t i = 0; i < count; ++i) {
      // Set up the full URL, for download file tracking.
      std::string server_path = "files/downloads/";
      server_path += info[i].download_info.url_name;
      GURL url = test_server()->GetURL(server_path);
      info[i].error_info.url = url.spec();

      DownloadInsertFilesErrorCheckErrorsLoopBody(injector, info[i], i);
    }
  }

  // Attempts to download a file to a read-only folder, based on information
  // in |download_info|.
  void DownloadFilesToReadonlyFolder(size_t count,
                                     DownloadInfo* download_info) {
    DownloadFilesCheckErrorsSetup();

    // Make the test folder unwritable.
    FilePath destination_folder = GetDownloadDirectory(browser());
    DVLOG(1) << " " << __FUNCTION__ << "()"
             << " folder = '" << destination_folder.value() << "'";
    file_util::PermissionRestorer permission_restorer(destination_folder);
    EXPECT_TRUE(file_util::MakeFileUnwritable(destination_folder));

    for (size_t i = 0; i < count; ++i) {
      DownloadFilesCheckErrorsLoopBody(download_info[i], i);
    }
  }

  bool EnsureNoPendingDownloads() {
    bool result = true;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&EnsureNoPendingDownloadJobsOnIO, &result));
    MessageLoop::current()->Run();
    return result && DownloadManager::EnsureNoPendingDownloadsForTesting();
  }

 private:
  static void EnsureNoPendingDownloadJobsOnIO(bool* result) {
    if (URLRequestSlowDownloadJob::NumberOutstandingRequests())
      *result = false;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, MessageLoop::QuitClosure());
  }

  // Location of the test data.
  FilePath test_dir_;

  // Location of the downloads directory for these tests
  ScopedTempDir downloads_directory_;
};

// NOTES:
//
// Files for these tests are found in DIR_TEST_DATA (currently
// "chrome\test\data\", see chrome_paths.cc).
// Mock responses have extension .mock-http-headers appended to the file name.

// Download a file due to the associated MIME type.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadMimeType) {
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));

  // Download the file and wait.  We do not expect the Select File dialog.
  DownloadAndWait(browser(), url, EXPECT_NO_SELECT_DIALOG);

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  CheckDownload(browser(), file, file);
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
}

#if defined(OS_WIN)
// Download a file and confirm that the zone identifier (on windows)
// is set to internet.
IN_PROC_BROWSER_TEST_F(DownloadTest, CheckInternetZone) {
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));

  // Download the file and wait.  We do not expect the Select File dialog.
  DownloadAndWait(browser(), url, EXPECT_NO_SELECT_DIALOG);

  // Check state.  Special file state must be checked before CheckDownload,
  // as CheckDownload will delete the output file.
  EXPECT_EQ(1, browser()->tab_count());
  FilePath downloaded_file(DestinationFile(browser(), file));
  if (file_util::VolumeSupportsADS(downloaded_file))
    EXPECT_TRUE(file_util::HasInternetZoneIdentifier(downloaded_file));
  CheckDownload(browser(), file, file);
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
}
#endif

// Put up a Select File dialog when the file is downloaded, due to
// downloads preferences settings.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadMimeTypeSelect) {
  // Re-enable prompting.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPromptForDownload, true);
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));

  NullSelectFile(browser());

  // Download the file and wait.  We expect the Select File dialog to appear
  // due to the MIME type, but we still wait until the download completes.
  scoped_ptr<DownloadTestObserver> observer(
      new DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()),
          1,
          false,                   // Continue on select file.
          DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);
  EXPECT_TRUE(observer->select_file_dialog_seen());

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  CheckDownload(browser(), file, file);
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
}

// Access a file with a viewable mime-type, verify that a download
// did not initiate.
IN_PROC_BROWSER_TEST_F(DownloadTest, NoDownload) {
  FilePath file(FILE_PATH_LITERAL("download-test2.html"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  FilePath file_path(DestinationFile(browser(), file));

  // Open a web page and wait.
  ui_test_utils::NavigateToURL(browser(), url);

  // Check that we did not download the web page.
  EXPECT_FALSE(file_util::PathExists(file_path));

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());
}

// Verify that when the DownloadResourceThrottle cancels a download, the
// download never makes it to the downloads system.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadResourceThrottleCancels) {
  // Navigate to a page with the same domain as the file to download.  We can't
  // navigate directly to the file we don't want to download because cross-site
  // navigations reset the TabDownloadState.
  FilePath same_site_path(FILE_PATH_LITERAL("download_script.html"));
  GURL same_site_url(URLRequestMockHTTPJob::GetMockUrl(same_site_path));
  ui_test_utils::NavigateToURL(browser(), same_site_url);

  // Make sure the initial navigation didn't trigger a download.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  // Disable downloads for the tab.
  content::NavigationController* controller =
      &browser()->GetSelectedWebContents()->GetController();
  DownloadRequestLimiter::TabDownloadState* tab_download_state =
      g_browser_process->download_request_limiter()->GetDownloadState(
          controller,
          controller,
          true);
  ASSERT_TRUE(tab_download_state);
  tab_download_state->set_download_status(
      DownloadRequestLimiter::DOWNLOADS_NOT_ALLOWED);

  // Try to start the download via Javascript and wait for the corresponding
  // load stop event.
  TestNavigationObserver observer(
      content::Source<content::NavigationController>(controller),
      NULL,
      1);
  bool download_assempted;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      browser()->GetSelectedWebContents()->GetRenderViewHost(),
      L"",
      L"window.domAutomationController.send(startDownload());",
      &download_assempted));
  ASSERT_TRUE(download_assempted);
  observer.WaitForObservation(
      base::Bind(&ui_test_utils::RunMessageLoop),
      base::Bind(&MessageLoop::Quit,
                 base::Unretained(MessageLoopForUI::current())));

  // Check that we did not download the file.
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  FilePath file_path(DestinationFile(browser(), file));
  EXPECT_FALSE(file_util::PathExists(file_path));

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  // Verify that there's no pending download.
  EXPECT_TRUE(EnsureNoPendingDownloads());
}

// Download a 0-size file with a content-disposition header, verify that the
// download tab opened and the file exists as the filename specified in the
// header.  This also ensures we properly handle empty file downloads.
// The download shelf should be visible in the current tab.
IN_PROC_BROWSER_TEST_F(DownloadTest, ContentDisposition) {
  FilePath file(FILE_PATH_LITERAL("download-test3.gif"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  FilePath download_file(FILE_PATH_LITERAL("download-test3-attachment.gif"));

  // Download a file and wait.
  DownloadAndWait(browser(), url, EXPECT_NO_SELECT_DIALOG);

  CheckDownload(browser(), download_file, file);

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
}

// Test that the download shelf is per-window by starting a download in one
// tab, opening a second tab, closing the shelf, going back to the first tab,
// and checking that the shelf is closed.
IN_PROC_BROWSER_TEST_F(DownloadTest, PerWindowShelf) {
  FilePath file(FILE_PATH_LITERAL("download-test3.gif"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  FilePath download_file(FILE_PATH_LITERAL("download-test3-attachment.gif"));

  // Download a file and wait.
  DownloadAndWait(browser(), url, EXPECT_NO_SELECT_DIALOG);

  CheckDownload(browser(), download_file, file);

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // Open a second tab and wait.
  EXPECT_NE(static_cast<TabContentsWrapper*>(NULL),
            browser()->AddSelectedTabWithURL(
                GURL(), content::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // Hide the download shelf.
  browser()->window()->GetDownloadShelf()->Close();
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  // Go to the first tab.
  browser()->ActivateTabAt(0, true);
  EXPECT_EQ(2, browser()->tab_count());

  // The download shelf should not be visible.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());
}

// Check whether the downloads shelf is closed when the downloads tab is
// invoked.
IN_PROC_BROWSER_TEST_F(DownloadTest, CloseShelfOnDownloadsTab) {
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));

  // Download the file and wait.  We do not expect the Select File dialog.
  DownloadAndWait(browser(), url, EXPECT_NO_SELECT_DIALOG);

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // Open the downloads tab.
  browser()->ShowDownloadsTab();
  // The shelf should now be closed.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());
}

// UnknownSize and KnownSize are tests which depend on
// URLRequestSlowDownloadJob to serve content in a certain way. Data will be
// sent in two chunks where the first chunk is 35K and the second chunk is 10K.
// The test will first attempt to download a file; but the server will "pause"
// in the middle until the server receives a second request for
// "download-finish".  At that time, the download will finish.
// These tests don't currently test much due to holes in |RunSizeTest()|.  See
// comments in that routine for details.
IN_PROC_BROWSER_TEST_F(DownloadTest, UnknownSize) {
  ASSERT_TRUE(RunSizeTest(browser(), SIZE_TEST_TYPE_UNKNOWN,
                          "32.0 KB - ", "100% - "));
}

IN_PROC_BROWSER_TEST_F(DownloadTest, KnownSize) {
  ASSERT_TRUE(RunSizeTest(browser(), SIZE_TEST_TYPE_KNOWN,
                          "71% - ", "100% - "));
}

// Test that when downloading an item in Incognito mode, we don't crash when
// closing the last Incognito window (http://crbug.com/13983).
// Also check that the download shelf is not visible after closing the
// Incognito window.
IN_PROC_BROWSER_TEST_F(DownloadTest, IncognitoDownload) {
  // Open an Incognito window.
  Browser* incognito = CreateIncognitoBrowser();  // Waits.
  ASSERT_TRUE(incognito);
  int window_count = BrowserList::size();
  EXPECT_EQ(2, window_count);

  // Download a file in the Incognito window and wait.
  CreateAndSetDownloadsDirectory(incognito);
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  // Since |incognito| is a separate browser, we have to set it up explicitly.
  incognito->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                               false);
  DownloadAndWait(incognito, url, EXPECT_NO_SELECT_DIALOG);

  // We should still have 2 windows.
  ExpectWindowCountAfterDownload(2);

  // Verify that the download shelf is showing for the Incognito window.
  EXPECT_TRUE(incognito->window()->IsDownloadShelfVisible());

#if !defined(OS_MACOSX)
  // On Mac OS X, the UI window close is delayed until the outermost
  // message loop runs.  So it isn't possible to get a BROWSER_CLOSED
  // notification inside of a test.
  ui_test_utils::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(incognito));
#endif

  // Close the Incognito window and don't crash.
  incognito->CloseWindow();

#if !defined(OS_MACOSX)
  signal.Wait();
  ExpectWindowCountAfterDownload(1);
#endif

  // Verify that the regular window does not have a download shelf.
  // On ChromeOS, the download panel is common to both profiles, so
  // it is still visible.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  CheckDownload(browser(), file, file);
}

// Navigate to a new background page, but don't download.  Confirm that the
// download shelf is not visible and that we have two tabs.
IN_PROC_BROWSER_TEST_F(DownloadTest, DontCloseNewTab1) {
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  FilePath file1(FILE_PATH_LITERAL("download-test2.html"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file1));

  // Open a web page and wait.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      url,
      NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // We should have two tabs now.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());
}

// Download a file in a background tab. Verify that the tab is closed
// automatically, and that the download shelf is visible in the current tab.
IN_PROC_BROWSER_TEST_F(DownloadTest, CloseNewTab1) {
  // Download a file in a new background tab and wait.  The tab is automatically
  // closed when the download begins.
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  DownloadAndWaitWithDisposition(
      browser(),
      url,
      NEW_BACKGROUND_TAB,
      EXPECT_NO_SELECT_DIALOG,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // When the download finishes, we should still have one tab.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  EXPECT_EQ(1, browser()->tab_count());

  CheckDownload(browser(), file, file);
}

// Open a web page in the current tab, then download a file in another tab via
// a Javascript call.
// Verify that we have 2 tabs, and the download shelf is visible in the current
// tab.
//
// The download_page1.html page contains an openNew() function that opens a
// tab and then downloads download-test1.lib.
IN_PROC_BROWSER_TEST_F(DownloadTest, DontCloseNewTab2) {
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  FilePath file1(FILE_PATH_LITERAL("download_page1.html"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file1));

  // Open a web page and wait.
  ui_test_utils::NavigateToURL(browser(), url);

  // Download a file in a new tab and wait (via Javascript).
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndWaitWithDisposition(browser(),
                                 GURL("javascript:openNew()"),
                                 CURRENT_TAB,
                                 EXPECT_NO_SELECT_DIALOG,
                                 ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // When the download finishes, we should have two tabs.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  EXPECT_EQ(2, browser()->tab_count());

  CheckDownload(browser(), file, file);
}

// Open a web page in the current tab, open another tab via a Javascript call,
// then download a file in the new tab.
// Verify that we have 2 tabs, and the download shelf is visible in the current
// tab.
//
// The download_page2.html page contains an openNew() function that opens a
// tab.
IN_PROC_BROWSER_TEST_F(DownloadTest, DontCloseNewTab3) {
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  FilePath file1(FILE_PATH_LITERAL("download_page2.html"));
  GURL url1(URLRequestMockHTTPJob::GetMockUrl(file1));

  // Open a web page and wait.
  ui_test_utils::NavigateToURL(browser(), url1);

  // Open a new tab and wait.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("javascript:openNew()"),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  EXPECT_EQ(2, browser()->tab_count());

  // Download a file and wait.
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  DownloadAndWaitWithDisposition(browser(),
                                 url,
                                 CURRENT_TAB,
                                 EXPECT_NO_SELECT_DIALOG,
                                 ui_test_utils::BROWSER_TEST_NONE);

  // When the download finishes, we should have two tabs.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  EXPECT_EQ(2, browser()->tab_count());

  CheckDownload(browser(), file, file);
}

// Open a web page in the current tab, then download a file via Javascript,
// which will do so in a temporary tab.
// Verify that we have 1 tab, and the download shelf is visible.
//
// The download_page3.html page contains an openNew() function that opens a
// tab with download-test1.lib in the URL.  When the URL is determined to be
// a download, the tab is closed automatically.
IN_PROC_BROWSER_TEST_F(DownloadTest, CloseNewTab2) {
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  FilePath file1(FILE_PATH_LITERAL("download_page3.html"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file1));

  // Open a web page and wait.
  ui_test_utils::NavigateToURL(browser(), url);

  // Download a file and wait.
  // The file to download is "download-test1.lib".
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndWaitWithDisposition(browser(),
                                 GURL("javascript:openNew()"),
                                 CURRENT_TAB,
                                 EXPECT_NO_SELECT_DIALOG,
                                 ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // When the download finishes, we should still have one tab.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  EXPECT_EQ(1, browser()->tab_count());

  CheckDownload(browser(), file, file);
}

// Open a web page in the current tab, then call Javascript via a button to
// download a file in a new tab, which is closed automatically when the
// download begins.
// Verify that we have 1 tab, and the download shelf is visible.
//
// The download_page4.html page contains a form with download-test1.lib as the
// action.
IN_PROC_BROWSER_TEST_F(DownloadTest, CloseNewTab3) {
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  FilePath file1(FILE_PATH_LITERAL("download_page4.html"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file1));

  // Open a web page and wait.
  ui_test_utils::NavigateToURL(browser(), url);

  // Download a file in a new tab and wait.  The tab will automatically close
  // when the download begins.
  // The file to download is "download-test1.lib".
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndWaitWithDisposition(
      browser(),
      GURL("javascript:document.getElementById('form').submit()"),
      CURRENT_TAB,
      EXPECT_NO_SELECT_DIALOG,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // When the download finishes, we should still have one tab.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  EXPECT_EQ(1, browser()->tab_count());

  CheckDownload(browser(), file, file);
}

// Download a file in a new window.
// Verify that we have 2 windows, and the download shelf is not visible in the
// first window, but is visible in the second window.
// Close the new window.
// Verify that we have 1 window, and the download shelf is not visible.
//
// Regression test for http://crbug.com/44454
IN_PROC_BROWSER_TEST_F(DownloadTest, NewWindow) {
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
#if !defined(OS_MACOSX)
  // See below.
  Browser* first_browser = browser();
#endif

  // Download a file in a new window and wait.
  DownloadAndWaitWithDisposition(browser(),
                                 url,
                                 NEW_WINDOW,
                                 EXPECT_NO_SELECT_DIALOG,
                                 ui_test_utils::BROWSER_TEST_NONE);

  // When the download finishes, the download shelf SHOULD NOT be visible in
  // the first window.
  ExpectWindowCountAfterDownload(2);
  EXPECT_EQ(1, browser()->tab_count());
  // Download shelf should close. Download panel stays open on ChromeOS.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  // The download shelf SHOULD be visible in the second window.
  std::set<Browser*> original_browsers;
  original_browsers.insert(browser());
  Browser* download_browser =
      ui_test_utils::GetBrowserNotInSet(original_browsers);
  ASSERT_TRUE(download_browser != NULL);
  EXPECT_NE(download_browser, browser());
  EXPECT_EQ(1, download_browser->tab_count());
  EXPECT_TRUE(download_browser->window()->IsDownloadShelfVisible());

#if !defined(OS_MACOSX)
  // On Mac OS X, the UI window close is delayed until the outermost
  // message loop runs.  So it isn't possible to get a BROWSER_CLOSED
  // notification inside of a test.
  ui_test_utils::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(download_browser));
#endif

  // Close the new window.
  download_browser->CloseWindow();

#if !defined(OS_MACOSX)
  signal.Wait();
  EXPECT_EQ(first_browser, browser());
  ExpectWindowCountAfterDownload(1);
#endif

  EXPECT_EQ(1, browser()->tab_count());
  // Download shelf should close. Download panel stays open on ChromeOS.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  CheckDownload(browser(), file, file);
}

// Check that downloading multiple (in this case, 2) files does not result in
// corrupted files.
IN_PROC_BROWSER_TEST_F(DownloadTest, MultiDownload) {
  EXPECT_EQ(1, browser()->tab_count());

  // Create a download, wait until it's started, and confirm
  // we're in the expected state.
  scoped_ptr<DownloadTestObserver> observer1(
      CreateInProgressWaiter(browser(), 1));
  ui_test_utils::NavigateToURL(
      browser(), GURL(URLRequestSlowDownloadJob::kUnknownSizeUrl));
  observer1->WaitForFinished();

  std::vector<DownloadItem*> downloads;
  browser()->profile()->GetDownloadManager()->SearchDownloads(
      string16(), &downloads);
  ASSERT_EQ(1u, downloads.size());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, downloads[0]->GetState());
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  DownloadItem* download1 = downloads[0];  // The only download.

  // Start the second download and wait until it's done.
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  // Download the file and wait.  We do not expect the Select File dialog.
  DownloadAndWait(browser(), url, EXPECT_NO_SELECT_DIALOG);

  // Should now have 2 items on the download shelf.
  downloads.clear();
  browser()->profile()->GetDownloadManager()->SearchDownloads(
      string16(), &downloads);
  ASSERT_EQ(2u, downloads.size());
  // We don't know the order of the downloads.
  DownloadItem* download2 = downloads[(download1 == downloads[0]) ? 1 : 0];

  ASSERT_EQ(DownloadItem::IN_PROGRESS, download1->GetState());
  ASSERT_EQ(DownloadItem::COMPLETE, download2->GetState());
  // The download shelf should be open.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // Allow the first request to finish.  We do this by loading a third URL
  // in a separate tab.
  scoped_ptr<DownloadTestObserver> observer2(CreateWaiter(browser(), 1));
  GURL finish_url(URLRequestSlowDownloadJob::kFinishDownloadUrl);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      finish_url,
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  observer2->WaitForFinished();  // Wait for the third request.
  EXPECT_EQ(1u, observer2->NumDownloadsSeenInState(DownloadItem::COMPLETE));

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // The |DownloadItem|s should now be done and have the final file names.
  // Verify that the files have the expected data and size.
  // |file1| should be full of '*'s, and |file2| should be the same as the
  // source file.
  FilePath file1(download1->GetFullPath());
  size_t file_size1 = URLRequestSlowDownloadJob::kFirstDownloadSize +
                      URLRequestSlowDownloadJob::kSecondDownloadSize;
  std::string expected_contents(file_size1, '*');
  ASSERT_TRUE(VerifyFile(file1, expected_contents, file_size1));

  FilePath file2(download2->GetFullPath());
  ASSERT_TRUE(file_util::ContentsEqual(OriginFile(file), file2));
}

IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadCancelled) {
  EXPECT_EQ(1, browser()->tab_count());

  // TODO(rdsmith): Fragile code warning!  The code below relies on the
  // DownloadTestObserverInProgress only finishing when the new download
  // has reached the state of being entered into the history and being
  // user-visible (that's what's required for the Remove to be valid and
  // for the download shelf to be visible).  By the pure semantics of
  // DownloadTestObserverInProgress, that's not guaranteed; DownloadItems
  // are created in the IN_PROGRESS state and made known to the DownloadManager
  // immediately, so any ModelChanged event on the DownloadManager after
  // navigation would allow the observer to return.  However, the only
  // ModelChanged() event the code will currently fire is in
  // OnCreateDownloadEntryComplete, at which point the download item will
  // be in the state we need.
  // The right way to fix this is to create finer grained states on the
  // DownloadItem, and wait for the state that indicates the item has been
  // entered in the history and made visible in the UI.

  // Create a download, wait until it's started, and confirm
  // we're in the expected state.
  scoped_ptr<DownloadTestObserver> observer(
      CreateInProgressWaiter(browser(), 1));
  ui_test_utils::NavigateToURL(
      browser(), GURL(URLRequestSlowDownloadJob::kUnknownSizeUrl));
  observer->WaitForFinished();

  std::vector<DownloadItem*> downloads;
  DownloadManagerForBrowser(browser())->SearchDownloads(
      string16(), &downloads);
  ASSERT_EQ(1u, downloads.size());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, downloads[0]->GetState());
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // Cancel the download and wait for download system quiesce.
  downloads[0]->Delete(DownloadItem::DELETE_DUE_TO_USER_DISCARD);
  scoped_refptr<DownloadTestFlushObserver> flush_observer(
      new DownloadTestFlushObserver(
          DownloadManagerForBrowser(browser())));
  flush_observer->WaitForFlush();

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  // Using "DownloadItem::Remove" follows the discard dangerous download path,
  // which completely removes the browser from the shelf and closes the shelf
  // if it was there. Download panel stays open on ChromeOS.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());
}

// Confirm a download makes it into the history properly.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadHistoryCheck) {
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  FilePath origin_file(OriginFile(file));
  int64 origin_size;
  file_util::GetFileSize(origin_file, &origin_size);

  // Download the file and wait.  We do not expect the Select File dialog.
  DownloadAndWait(browser(), url, EXPECT_NO_SELECT_DIALOG);

  // Get details of what downloads have just happened.
  std::vector<DownloadItem*> downloads;
  GetDownloads(browser(), &downloads);
  ASSERT_EQ(1u, downloads.size());
  int64 db_handle = downloads[0]->GetDbHandle();

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  CheckDownload(browser(), file, file);
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // Check history results.
  DownloadsHistoryDataCollector history_collector(
      db_handle,
      DownloadManagerForBrowser(browser()));
  DownloadPersistentStoreInfo info;
  EXPECT_TRUE(history_collector.GetDownloadsHistoryEntry(&info)) << db_handle;
  EXPECT_EQ(file, info.path.BaseName());
  EXPECT_EQ(url, info.url);
  // Ignore start_time.
  EXPECT_EQ(origin_size, info.received_bytes);
  EXPECT_EQ(origin_size, info.total_bytes);
  EXPECT_EQ(DownloadItem::COMPLETE, info.state);
}

// Test for crbug.com/14505. This tests that chrome:// urls are still functional
// after download of a file while viewing another chrome://.
IN_PROC_BROWSER_TEST_F(DownloadTest, ChromeURLAfterDownload) {
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL download_url(URLRequestMockHTTPJob::GetMockUrl(file));
  GURL flags_url(chrome::kChromeUIFlagsURL);
  GURL extensions_url(chrome::kChromeUIExtensionsFrameURL);

  ui_test_utils::NavigateToURL(browser(), flags_url);
  DownloadAndWait(browser(), download_url, EXPECT_NO_SELECT_DIALOG);
  ui_test_utils::NavigateToURL(browser(), extensions_url);
  WebContents* contents = browser()->GetSelectedWebContents();
  ASSERT_TRUE(contents);
  bool webui_responded = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->GetRenderViewHost(),
      L"",
      L"window.domAutomationController.send(window.webui_responded_);",
      &webui_responded));
  EXPECT_TRUE(webui_responded);
}

// Test for crbug.com/12745. This tests that if a download is initiated from
// a chrome:// page that has registered and onunload handler, the browser
// will be able to close.
IN_PROC_BROWSER_TEST_F(DownloadTest, BrowserCloseAfterDownload) {
  GURL downloads_url(chrome::kChromeUIFlagsURL);
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL download_url(URLRequestMockHTTPJob::GetMockUrl(file));

  ui_test_utils::NavigateToURL(browser(), downloads_url);
  WebContents* contents = browser()->GetSelectedWebContents();
  ASSERT_TRUE(contents);
  bool result = false;
  EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      contents->GetRenderViewHost(),
      L"",
      L"window.onunload = function() { var do_nothing = 0; }; "
      L"window.domAutomationController.send(true);",
      &result));
  EXPECT_TRUE(result);

  DownloadAndWait(browser(), download_url, EXPECT_NO_SELECT_DIALOG);

  ui_test_utils::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser()));
  browser()->CloseWindow();
  signal.Wait();
}

// Test to make sure the 'download' attribute in anchor tag is respected.
IN_PROC_BROWSER_TEST_F(DownloadTest, AnchorDownloadTag) {
  FilePath file(FILE_PATH_LITERAL("download-anchor-attrib.html"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));

  // Create a download, wait until it's complete, and confirm
  // we're in the expected state.
  scoped_ptr<DownloadTestObserver> observer(CreateWaiter(browser(), 1));
  ui_test_utils::NavigateToURL(browser(), url);
  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Confirm the downloaded data exists.
  FilePath downloaded_file = GetDownloadDirectory(browser());
  downloaded_file = downloaded_file.Append(FILE_PATH_LITERAL("a_red_dot.png"));
  EXPECT_TRUE(file_util::PathExists(downloaded_file));
}

// crbug.com/118159
#if defined(OS_LINUX)
#define MAYBE_AutoOpen DISABLED_AutoOpen
#else
#define MAYBE_AutoOpen AutoOpen
#endif

// Test to make sure auto-open works.
IN_PROC_BROWSER_TEST_F(DownloadTest, MAYBE_AutoOpen) {
  FilePath file(FILE_PATH_LITERAL("download-autoopen.txt"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));

  ASSERT_TRUE(
      GetDownloadPrefs(browser())->EnableAutoOpenBasedOnExtension(file));

  // Mock out external opening on all downloads until end of test.
  MockDownloadOpeningObserver observer(
      DownloadManagerForBrowser(browser()));

  DownloadAndWait(browser(), url, EXPECT_NO_SELECT_DIALOG);

  // Find the download and confirm it was opened.
  std::vector<DownloadItem*> downloads;
  DownloadManagerForBrowser(browser())->SearchDownloads(
      string16(), &downloads);
  ASSERT_EQ(1u, downloads.size());
  EXPECT_EQ(DownloadItem::COMPLETE, downloads[0]->GetState());
  EXPECT_TRUE(downloads[0]->GetOpened());

  // As long as we're here, confirmed everything else is good.
  EXPECT_EQ(1, browser()->tab_count());
  CheckDownload(browser(), file, file);
  // Download shelf should close. Download panel stays open on ChromeOS.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());
}

// Download an extension.  Expect a dangerous download warning.
// Deny the download.
IN_PROC_BROWSER_TEST_F(DownloadTest, CrxDenyInstall) {
  GURL extension_url(URLRequestMockHTTPJob::GetMockUrl(kGoodCrxPath));

  scoped_ptr<DownloadTestObserver> observer(
      DangerousDownloadWaiter(
          browser(), 1,
          DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_DENY));
  ui_test_utils::NavigateToURL(browser(), extension_url);

  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::CANCELLED));
  EXPECT_EQ(1u, observer->NumDangerousDownloadsSeen());

  // Download shelf should close. Download panel stays open on ChromeOS.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  // Check that the CRX is not installed.
  ExtensionService* extension_service =
      browser()->profile()->GetExtensionService();
  ASSERT_FALSE(extension_service->GetExtensionById(kGoodCrxId, false));
}

// Download an extension.  Expect a dangerous download warning.
// Allow the download, deny the install.
IN_PROC_BROWSER_TEST_F(DownloadTest, CrxInstallDenysPermissions) {
  GURL extension_url(URLRequestMockHTTPJob::GetMockUrl(kGoodCrxPath));

  // Install a mock install UI that simulates a user denying permission to
  // finish the install.
  download_crx_util::SetMockInstallUIForTesting(
      new MockAbortExtensionInstallUI());

  scoped_ptr<DownloadTestObserver> observer(
      DangerousDownloadWaiter(
          browser(), 1,
          DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));
  ui_test_utils::NavigateToURL(browser(), extension_url);

  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);
  EXPECT_EQ(1u, observer->NumDangerousDownloadsSeen());

  // Download shelf should close. Download panel stays open on ChromeOS.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  // Check that the extension was not installed.
  ExtensionService* extension_service =
      browser()->profile()->GetExtensionService();
  ASSERT_FALSE(extension_service->GetExtensionById(kGoodCrxId, false));
}

// Download an extension.  Expect a dangerous download warning.
// Allow the download, and the install.
IN_PROC_BROWSER_TEST_F(DownloadTest, CrxInstallAcceptPermissions) {
  GURL extension_url(URLRequestMockHTTPJob::GetMockUrl(kGoodCrxPath));

  // Install a mock install UI that simulates a user allowing permission to
  // finish the install.
  download_crx_util::SetMockInstallUIForTesting(
      new MockAutoConfirmExtensionInstallUI(browser()->profile()));

  scoped_ptr<DownloadTestObserver> observer(
      DangerousDownloadWaiter(
          browser(), 1,
          DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));
  ui_test_utils::NavigateToURL(browser(), extension_url);

  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);
  EXPECT_EQ(1u, observer->NumDangerousDownloadsSeen());

  // Download shelf should close. Download panel stays open on ChromeOS.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  // Check that the extension was installed.
  ExtensionService* extension_service =
      browser()->profile()->GetExtensionService();
  ASSERT_TRUE(extension_service->GetExtensionById(kGoodCrxId, false));
}

// Test installing a CRX that fails integrity checks.
IN_PROC_BROWSER_TEST_F(DownloadTest, CrxInvalid) {
  FilePath file(FILE_PATH_LITERAL("extensions/bad_signature.crx"));
  GURL extension_url(URLRequestMockHTTPJob::GetMockUrl(file));

  // Install a mock install UI that simulates a user allowing permission to
  // finish the install, and dismisses any error message.  We check that the
  // install failed below.
  download_crx_util::SetMockInstallUIForTesting(
      new MockAutoConfirmExtensionInstallUI(browser()->profile()));

  scoped_ptr<DownloadTestObserver> observer(
      DangerousDownloadWaiter(
          browser(), 1,
          DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));
  ui_test_utils::NavigateToURL(browser(), extension_url);

  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Check that the extension was not installed.
  ExtensionService* extension_service =
      browser()->profile()->GetExtensionService();
  ASSERT_FALSE(extension_service->GetExtensionById(kGoodCrxId, false));
}

// Install a large (100kb) theme.
IN_PROC_BROWSER_TEST_F(DownloadTest, CrxLargeTheme) {
  GURL extension_url(URLRequestMockHTTPJob::GetMockUrl(kLargeThemePath));

  // Install a mock install UI that simulates a user allowing permission to
  // finish the install.
  download_crx_util::SetMockInstallUIForTesting(
      new MockAutoConfirmExtensionInstallUI(browser()->profile()));

  scoped_ptr<DownloadTestObserver> observer(
      DangerousDownloadWaiter(
          browser(), 1,
          DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));
  ui_test_utils::NavigateToURL(browser(), extension_url);

  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);
  EXPECT_EQ(1u, observer->NumDangerousDownloadsSeen());

  // Download shelf should close. Download panel stays open on ChromeOS.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  // Check that the extension was installed.
  ExtensionService* extension_service =
      browser()->profile()->GetExtensionService();
  ASSERT_TRUE(extension_service->GetExtensionById(kLargeThemeCrxId, false));
}

// Sort download items by db_handle.
static bool DownloadItemSorter(DownloadItem* d1, DownloadItem* d2) {
  return d1->GetDbHandle() < d2->GetDbHandle();
}

// Confirm that searching through the history works properly
IN_PROC_BROWSER_TEST_F(DownloadTest, SearchDownloads) {
  // Downloads to populate history with.
  base::Time current(base::Time::Now());
  DownloadPersistentStoreInfo population_entries[] = {
    DownloadPersistentStoreInfo(
        FilePath(FILE_PATH_LITERAL("/path/to/file")),
        GURL("http://www.google.com/fantasy_download"),
        GURL(""),
        current - base::TimeDelta::FromMinutes(5),
        current,
        128,
        128,
        DownloadItem::COMPLETE,
        1,
        false),
    DownloadPersistentStoreInfo(
        FilePath(FILE_PATH_LITERAL("/path/to/another_file")),
        GURL("http://www.google.com/reality_download"),
        GURL(""),
        current - base::TimeDelta::FromMinutes(10),
        current,
        256,
        256,
        DownloadItem::COMPLETE,
        2,
        false),
    DownloadPersistentStoreInfo(
        FilePath(FILE_PATH_LITERAL("/different_path/to/another_file")),
        GURL("http://www.izzle.com/not_really_a_download"),
        GURL(""),
        current - base::TimeDelta::FromMinutes(15),
        current,
        512,
        512,
        DownloadItem::COMPLETE,
        3,
        true)
  };
  std::vector<DownloadPersistentStoreInfo> entries(
      population_entries, population_entries + arraysize(population_entries));

  // Populate the manager.
  DownloadManager* manager = DownloadManagerForBrowser(browser());
  manager->OnPersistentStoreQueryComplete(&entries);

  // Do some searches and check the results.
  std::vector<DownloadItem*> search_results;

  manager->SearchDownloads(string16(), &search_results);
  ASSERT_EQ(3u, search_results.size());
  std::sort(search_results.begin(), search_results.end(),
            DownloadItemSorter);
  // We do a full check only once to protect against the data
  // somehow getting scrambled on its way into the DownloadItems.
  {
    DownloadItem* d1 = search_results[0];
    DownloadItem* d2 = search_results[1];
    DownloadItem* d3 = search_results[2];
    EXPECT_EQ(FilePath(FILE_PATH_LITERAL("/path/to/file")), d1->GetFullPath());
    EXPECT_EQ(GURL("http://www.google.com/fantasy_download"),
              d1->GetOriginalUrl());
    EXPECT_EQ(current - base::TimeDelta::FromMinutes(5),
              d1->GetStartTime());
    EXPECT_EQ(current, d1->GetEndTime());
    EXPECT_EQ(128, d1->GetReceivedBytes());
    EXPECT_EQ(128, d1->GetTotalBytes());
    EXPECT_EQ(DownloadItem::COMPLETE, d1->GetState());
    EXPECT_EQ(1, d1->GetDbHandle());
    EXPECT_FALSE(d1->GetOpened());

    EXPECT_EQ(FilePath(FILE_PATH_LITERAL("/path/to/another_file")),
              d2->GetFullPath());
    EXPECT_EQ(GURL("http://www.google.com/reality_download"),
              d2->GetOriginalUrl());
    EXPECT_EQ(current - base::TimeDelta::FromMinutes(10),
              d2->GetStartTime());
    EXPECT_EQ(current, d2->GetEndTime());
    EXPECT_EQ(256, d2->GetReceivedBytes());
    EXPECT_EQ(256, d2->GetTotalBytes());
    EXPECT_EQ(DownloadItem::COMPLETE, d2->GetState());
    EXPECT_EQ(2, d2->GetDbHandle());
    EXPECT_FALSE(d2->GetOpened());

    EXPECT_EQ(FilePath(FILE_PATH_LITERAL("/different_path/to/another_file")),
              d3->GetFullPath());
    EXPECT_EQ(GURL("http://www.izzle.com/not_really_a_download"),
              d3->GetOriginalUrl());
    EXPECT_EQ(current - base::TimeDelta::FromMinutes(15),
              d3->GetStartTime());
    EXPECT_EQ(current, d3->GetEndTime());
    EXPECT_EQ(512, d3->GetReceivedBytes());
    EXPECT_EQ(512, d3->GetTotalBytes());
    EXPECT_EQ(DownloadItem::COMPLETE, d3->GetState());
    EXPECT_EQ(3, d3->GetDbHandle());
    EXPECT_TRUE(d3->GetOpened());
  }
  search_results.clear();

  string16 search_input;
  manager->SearchDownloads(UTF8ToUTF16("www.google.com"), &search_results);
  ASSERT_EQ(2u, search_results.size());
  std::sort(search_results.begin(), search_results.end(),
            DownloadItemSorter);
  EXPECT_EQ(1, search_results[0]->GetDbHandle());
  EXPECT_EQ(2, search_results[1]->GetDbHandle());
  search_results.clear();

  manager->SearchDownloads(UTF8ToUTF16("real"), &search_results);
  ASSERT_EQ(2u, search_results.size());
  std::sort(search_results.begin(), search_results.end(),
            DownloadItemSorter);
  EXPECT_EQ(2, search_results[0]->GetDbHandle());
  EXPECT_EQ(3, search_results[1]->GetDbHandle());
  search_results.clear();

  manager->SearchDownloads(UTF8ToUTF16("another_file"), &search_results);
  ASSERT_EQ(2u, search_results.size());
  std::sort(search_results.begin(), search_results.end(),
            DownloadItemSorter);
  EXPECT_EQ(2, search_results[0]->GetDbHandle());
  EXPECT_EQ(3, search_results[1]->GetDbHandle());
  search_results.clear();
}

// Tests for download initiation functions.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadUrl) {
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));

  // DownloadUrl always prompts; return acceptance of whatever it prompts.
  NullSelectFile(browser());

  WebContents* web_contents = browser()->GetSelectedWebContents();
  ASSERT_TRUE(web_contents);

  DownloadTestObserver* observer(
      new DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          false,                   // Ignore select file.
          DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  content::DownloadSaveInfo save_info;
  save_info.prompt_for_save_location = true;
  scoped_ptr<DownloadUrlParameters> params(
      DownloadUrlParameters::FromWebContents(web_contents, url, save_info));
  DownloadManagerForBrowser(browser())->DownloadUrl(params.Pass());
  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);
  EXPECT_TRUE(observer->select_file_dialog_seen());

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  ASSERT_TRUE(CheckDownload(browser(), file, file));
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
}

IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadUrlToPath) {
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));

  WebContents* web_contents = browser()->GetSelectedWebContents();
  ASSERT_TRUE(web_contents);

  ScopedTempDir other_directory;
  ASSERT_TRUE(other_directory.CreateUniqueTempDir());
  FilePath target_file_full_path
      = other_directory.path().Append(file.BaseName());
  content::DownloadSaveInfo save_info;
  save_info.file_path = target_file_full_path;

  DownloadTestObserver* observer(CreateWaiter(browser(), 1));
  scoped_ptr<DownloadUrlParameters> params(
      DownloadUrlParameters::FromWebContents(web_contents, url, save_info));
  DownloadManagerForBrowser(browser())->DownloadUrl(params.Pass());
  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  ASSERT_TRUE(CheckDownloadFullPaths(browser(),
                                     target_file_full_path,
                                     OriginFile(file)));

  // Temporary downloads won't be visible.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());
}

IN_PROC_BROWSER_TEST_F(DownloadTest, SavePageNonHTMLViaGet) {
  // Do initial setup.
  ASSERT_TRUE(test_server()->Start());
  NullSelectFile(browser());
  std::vector<DownloadItem*> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_TRUE(download_items.empty());

  // Navigate to a non-HTML resource. The resource also has
  // Cache-Control: no-cache set, which normally requires revalidation
  // each time.
  GURL url = test_server()->GetURL("files/downloads/image.jpg");
  ASSERT_TRUE(url.is_valid());
  ui_test_utils::NavigateToURL(browser(), url);

  // Stop the test server, and then try to save the page. If cache validation
  // is not bypassed then this will fail since the server is no longer
  // reachable.
  ASSERT_TRUE(test_server()->Stop());
  scoped_ptr<DownloadTestObserver> waiter(
      new DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          false, DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  browser()->SavePage();
  waiter->WaitForFinished();
  EXPECT_EQ(1u, waiter->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded.
  GetDownloads(browser(), &download_items);
  EXPECT_TRUE(waiter->select_file_dialog_seen());
  ASSERT_EQ(1u, download_items.size());
  ASSERT_EQ(url, download_items[0]->GetOriginalUrl());

  // Try to download it via a context menu.
  scoped_ptr<DownloadTestObserver> waiter_context_menu(
      new DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          false, DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  content::ContextMenuParams context_menu_params;
  context_menu_params.media_type = WebKit::WebContextMenuData::MediaTypeImage;
  context_menu_params.src_url = url;
  context_menu_params.page_url = url;
  TestRenderViewContextMenu menu(browser()->GetSelectedWebContents(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEIMAGEAS);
  waiter_context_menu->WaitForFinished();
  EXPECT_EQ(
      1u, waiter_context_menu->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(2, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded via the context menu.
  download_items.clear();
  GetDownloads(browser(), &download_items);
  EXPECT_TRUE(waiter_context_menu->select_file_dialog_seen());
  ASSERT_EQ(2u, download_items.size());
  ASSERT_EQ(url, download_items[0]->GetOriginalUrl());
  ASSERT_EQ(url, download_items[1]->GetOriginalUrl());
}

IN_PROC_BROWSER_TEST_F(DownloadTest, SavePageNonHTMLViaPost) {
  // Do initial setup.
  ASSERT_TRUE(test_server()->Start());
  NullSelectFile(browser());
  std::vector<DownloadItem*> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_TRUE(download_items.empty());

  // Navigate to a form page.
  GURL form_url = test_server()->GetURL(
      "files/downloads/form_page_to_post.html");
  ASSERT_TRUE(form_url.is_valid());
  ui_test_utils::NavigateToURL(browser(), form_url);

  // Submit the form. This will send a POST reqeuest, and the response is a
  // JPEG image. The resource also has Cache-Control: no-cache set,
  // which normally requires revalidation each time.
  GURL jpeg_url = test_server()->GetURL("files/post/downloads/image.jpg");
  ASSERT_TRUE(jpeg_url.is_valid());
  WebContents* web_contents = browser()->GetSelectedWebContents();
  ASSERT_TRUE(web_contents != NULL);
  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(
          &web_contents->GetController()));
  content::RenderViewHost* render_view_host = web_contents->GetRenderViewHost();
  ASSERT_TRUE(render_view_host != NULL);
  render_view_host->ExecuteJavascriptInWebFrame(
        string16(), ASCIIToUTF16("SubmitForm()"));
  observer.Wait();
  EXPECT_EQ(jpeg_url, web_contents->GetURL());

  // Stop the test server, and then try to save the page. If cache validation
  // is not bypassed then this will fail since the server is no longer
  // reachable. This will also fail if it tries to be retrieved via "GET"
  // rather than "POST".
  ASSERT_TRUE(test_server()->Stop());
  scoped_ptr<DownloadTestObserver> waiter(
      new DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          false, DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  browser()->SavePage();
  waiter->WaitForFinished();
  EXPECT_EQ(1u, waiter->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(1, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded.
  GetDownloads(browser(), &download_items);
  EXPECT_TRUE(waiter->select_file_dialog_seen());
  ASSERT_EQ(1u, download_items.size());
  ASSERT_EQ(jpeg_url, download_items[0]->GetOriginalUrl());

  // Try to download it via a context menu.
  scoped_ptr<DownloadTestObserver> waiter_context_menu(
      new DownloadTestObserverTerminal(
          DownloadManagerForBrowser(browser()), 1,
          false, DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  content::ContextMenuParams context_menu_params;
  context_menu_params.media_type = WebKit::WebContextMenuData::MediaTypeImage;
  context_menu_params.src_url = jpeg_url;
  context_menu_params.page_url = jpeg_url;
  TestRenderViewContextMenu menu(web_contents, context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_SAVEIMAGEAS);
  waiter_context_menu->WaitForFinished();
  EXPECT_EQ(
      1u, waiter_context_menu->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStates(2, DownloadItem::COMPLETE);

  // Validate that the correct file was downloaded via the context menu.
  download_items.clear();
  GetDownloads(browser(), &download_items);
  EXPECT_TRUE(waiter_context_menu->select_file_dialog_seen());
  ASSERT_EQ(2u, download_items.size());
  ASSERT_EQ(jpeg_url, download_items[0]->GetOriginalUrl());
  ASSERT_EQ(jpeg_url, download_items[1]->GetOriginalUrl());
}

IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadErrorsServer) {
  DownloadInfo download_info[] = {
    {  // Normal navigated download.
      "a_zip_file.zip",
      DOWNLOAD_NAVIGATE,
      content::DOWNLOAD_INTERRUPT_REASON_NONE,
      true,
      false
    },
    {  // Normal direct download.
      "a_zip_file.zip",
      DOWNLOAD_DIRECT,
      content::DOWNLOAD_INTERRUPT_REASON_NONE,
      true,
      false
    },
    {  // Direct download with 404 error.
      "there_IS_no_spoon.zip",
      DOWNLOAD_DIRECT,
      content::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
      true,
      false
    },
    {  // Navigated download with 404 error.
      "there_IS_no_spoon.zip",
      DOWNLOAD_NAVIGATE,
      content::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
      false,
      false
    },
    {  // Direct download with 400 error.
      "zip_file_not_found.zip",
      DOWNLOAD_DIRECT,
      content::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
      true,
      false
    },
    {  // Navigated download with 400 error.
      "zip_file_not_found.zip",
      DOWNLOAD_NAVIGATE,
      content::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
      false,
      false
    }
  };

  DownloadFilesCheckErrors(ARRAYSIZE_UNSAFE(download_info), download_info);
}

IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadErrorsFile) {
  FileErrorInjectInfo error_info[] = {
    {  // Navigated download with injected "Disk full" error in Initialize().
      { "a_zip_file.zip",
        DOWNLOAD_NAVIGATE,
        content::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
        1
      },
      {
        "",
        content::TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
        0,
        net::ERR_FILE_NO_SPACE
      }
    },
    {  // Direct download with injected "Disk full" error in Initialize().
      { "a_zip_file.zip",
        DOWNLOAD_DIRECT,
        content::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
        1
      },
      {
        "",
        content::TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
        0,
        net::ERR_FILE_NO_SPACE
      }
    },
    {  // Navigated download with injected "Disk full" error in Write().
      { "a_zip_file.zip",
        DOWNLOAD_NAVIGATE,
        content::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
        1
      },
      {
        "",
        content::TestFileErrorInjector::FILE_OPERATION_WRITE,
        0,
        net::ERR_FILE_NO_SPACE
      }
    },
    {  // Direct download with injected "Disk full" error in Write().
      { "a_zip_file.zip",
        DOWNLOAD_DIRECT,
        content::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
        1
      },
      {
        "",
        content::TestFileErrorInjector::FILE_OPERATION_WRITE,
        0,
        net::ERR_FILE_NO_SPACE
      }
    },
    {  // Navigated download with injected "Failed" error in Initialize().
      { "a_zip_file.zip",
        DOWNLOAD_NAVIGATE,
        content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
        1
      },
      {
        "",
        content::TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
        0,
        net::ERR_FAILED
      }
    },
    {  // Direct download with injected "Failed" error in Initialize().
      { "a_zip_file.zip",
        DOWNLOAD_DIRECT,
        content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
        1
      },
      {
        "",
        content::TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
        0,
        net::ERR_FAILED
      }
    },
    {  // Navigated download with injected "Failed" error in Write().
      { "a_zip_file.zip",
        DOWNLOAD_NAVIGATE,
        content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
        1
      },
      {
        "",
        content::TestFileErrorInjector::FILE_OPERATION_WRITE,
        0,
        net::ERR_FAILED
      }
    },
    {  // Direct download with injected "Failed" error in Write().
      { "a_zip_file.zip",
        DOWNLOAD_DIRECT,
        content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
        1
      },
      {
        "",
        content::TestFileErrorInjector::FILE_OPERATION_WRITE,
        0,
        net::ERR_FAILED
      }
    },
    {  // Navigated download with injected "Name too long" error in
       // Initialize().
      { "a_zip_file.zip",
        DOWNLOAD_NAVIGATE,
        content::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG,
        1
      },
      {
        "",
        content::TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
        0,
        net::ERR_FILE_PATH_TOO_LONG
      }
    },
    {  // Direct download with injected "Name too long" error in Initialize().
      { "a_zip_file.zip",
        DOWNLOAD_DIRECT,
        content::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG,
        1
      },
      {
        "",
        content::TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
        0,
        net::ERR_FILE_PATH_TOO_LONG
      }
    },
    {  // Navigated download with injected "Name too long" error in Write().
      { "a_zip_file.zip",
        DOWNLOAD_NAVIGATE,
        content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
        1
      },
      {
        "",
        content::TestFileErrorInjector::FILE_OPERATION_WRITE,
        0,
        net::ERR_INVALID_HANDLE
      }
    },
    {  // Direct download with injected "Name too long" error in Write().
      { "a_zip_file.zip",
        DOWNLOAD_DIRECT,
        content::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
        1
      },
      {
        "",
        content::TestFileErrorInjector::FILE_OPERATION_WRITE,
        0,
        net::ERR_INVALID_HANDLE
      }
    },
    {  // Direct download with injected "Disk full" error in 2nd Write().
      { "06bESSE21Evolution.ppt",
        DOWNLOAD_DIRECT,
        content::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
        1
      },
      {
        "",
        content::TestFileErrorInjector::FILE_OPERATION_WRITE,
        1,
        net::ERR_FILE_NO_SPACE
      }
    }
  };

  DownloadInsertFilesErrorCheckErrors(ARRAYSIZE_UNSAFE(error_info), error_info);
}

IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadErrorReadonlyFolder) {
  DownloadInfo download_info[] = {
    {
      "a_zip_file.zip",
      DOWNLOAD_DIRECT,
      // This passes because we switch to the My Documents folder.
      content::DOWNLOAD_INTERRUPT_REASON_NONE,
      true,
      true
    },
    {
      "a_zip_file.zip",
      DOWNLOAD_NAVIGATE,
      // This passes because we switch to the My Documents folder.
      content::DOWNLOAD_INTERRUPT_REASON_NONE,
      true,
      true
    }
  };

  DownloadFilesToReadonlyFolder(ARRAYSIZE_UNSAFE(download_info), download_info);
}

// Test that we show a dangerous downloads warning for a dangerous file
// downloaded through a blob: URL.
IN_PROC_BROWSER_TEST_F(DownloadTest, DownloadDangerousBlobData) {
#if defined(OS_WIN)
  // On Windows, if SafeBrowsing is enabled, certain file types (.exe, .cab,
  // .msi) will be handled by the DownloadProtectionService. However, if the URL
  // is non-standard (e.g. blob:) then those files won't be handled by the
  // DPS. We should be showing the dangerous download warning for any file
  // considered dangerous and isn't handled by the DPS.
  const char kFilename[] = "foo.exe";
#else
  const char kFilename[] = "foo.jar";
#endif

  std::string path("files/downloads/download-dangerous-blob.html?filename=");
  path += kFilename;

  // Need to use http urls because the blob js doesn't work on file urls for
  // security reasons.
  ASSERT_TRUE(test_server()->Start());
  GURL url(test_server()->GetURL(path));

  DownloadTestObserver* observer(DangerousDownloadWaiter(
      browser(), 1, DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));
  ui_test_utils::NavigateToURL(browser(), url);
  observer->WaitForFinished();

  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  EXPECT_EQ(1u, observer->NumDangerousDownloadsSeen());
}
