// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/test/test_file_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/api/prefs/pref_member.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_persistent_store_info.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/download/save_package_file_picker_chromeos.h"
#else
#include "chrome/browser/download/save_package_file_picker.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadItem;
using content::DownloadManager;
using content::DownloadPersistentStoreInfo;
using content::WebContents;

namespace {

static const FilePath::CharType* kTestDir = FILE_PATH_LITERAL("save_page");

static const char* kAppendedExtension =
#if defined(OS_WIN)
    ".htm";
#else
    ".html";
#endif

void NullFunction() {
}

}  // namespace

// Loosely based on logic in DownloadTestObserver.
class DownloadItemCreatedObserver : public DownloadManager::Observer {
 public:
  explicit DownloadItemCreatedObserver(DownloadManager* manager)
      : waiting_(false), manager_(manager) {
    manager->AddObserver(this);
  }

  ~DownloadItemCreatedObserver() {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  // Wait for the first download item created after object creation.
  // Note that this class provides no protection against the download
  // being destroyed between creation and return of WaitForNewDownloadItem();
  // the caller must guarantee that in some other fashion.
  void WaitForDownloadItem(std::vector<DownloadItem*>* items_seen) {
    if (!manager_) {
      // The manager went away before we were asked to wait; return
      // what we have, even if it's null.
      *items_seen = items_seen_;
      return;
    }

    if (items_seen_.empty()) {
      waiting_ = true;
      content::RunMessageLoop();
      waiting_ = false;
    }

    *items_seen = items_seen_;
    return;
  }

 private:

  // DownloadManager::Observer
  virtual void OnDownloadCreated(
      DownloadManager* manager, DownloadItem* item) OVERRIDE {
    DCHECK_EQ(manager, manager_);
    items_seen_.push_back(item);

    if (waiting_)
      MessageLoopForUI::current()->Quit();
  }

  virtual void ManagerGoingDown(DownloadManager* manager) OVERRIDE {
    manager_->RemoveObserver(this);
    manager_ = NULL;
    if (waiting_)
      MessageLoopForUI::current()->Quit();
  }

  bool waiting_;
  DownloadManager* manager_;
  std::vector<DownloadItem*> items_seen_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemCreatedObserver);
};

class DownloadPersistedObserver : public DownloadItem::Observer {
 public:
  explicit DownloadPersistedObserver(DownloadItem* item)
      : waiting_(false), item_(item) {
    item->AddObserver(this);
  }

  ~DownloadPersistedObserver() {
    if (item_)
      item_->RemoveObserver(this);
  }

  // Wait for download item to get the persisted bit set.
  // Note that this class provides no protection against the download
  // being destroyed between creation and return of WaitForPersisted();
  // the caller must guarantee that in some other fashion.
  void WaitForPersisted() {
    // In combination with OnDownloadDestroyed() below, verify the
    // above interface contract.
    DCHECK(item_);

    if (item_->IsPersisted())
      return;

    waiting_ = true;
    content::RunMessageLoop();
    waiting_ = false;

    return;
  }

 private:
  // DownloadItem::Observer
  virtual void OnDownloadUpdated(DownloadItem* item) OVERRIDE {
    DCHECK_EQ(item, item_);

    if (waiting_ && item->IsPersisted())
      MessageLoopForUI::current()->Quit();
  }

  virtual void OnDownloadDestroyed(DownloadItem* item) OVERRIDE {
    if (item != item_)
      return;

    item_->RemoveObserver(this);
    item_ = NULL;
  }

  bool waiting_;
  DownloadItem* item_;

  DISALLOW_COPY_AND_ASSIGN(DownloadPersistedObserver);
};

class SavePageBrowserTest : public InProcessBrowserTest {
 public:
  SavePageBrowserTest() {}
  virtual ~SavePageBrowserTest();

 protected:
  void SetUp() OVERRIDE {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir_));
    ASSERT_TRUE(save_dir_.CreateUniqueTempDir());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() OVERRIDE {
    browser()->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory, save_dir_.path());
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
    item_creation_observer_.reset(
        new DownloadItemCreatedObserver(GetDownloadManager()));
  }

  GURL NavigateToMockURL(const std::string& prefix) {
    GURL url = URLRequestMockHTTPJob::GetMockUrl(
        FilePath(kTestDir).AppendASCII(prefix + ".htm"));
    ui_test_utils::NavigateToURL(browser(), url);
    return url;
  }

  // Returns full paths of destination file and directory.
  void GetDestinationPaths(const std::string& prefix,
                FilePath* full_file_name,
                FilePath* dir) {
    *full_file_name = save_dir_.path().AppendASCII(prefix + ".htm");
    *dir = save_dir_.path().AppendASCII(prefix + "_files");
  }

  WebContents* GetCurrentTab() const {
    WebContents* current_tab = chrome::GetActiveWebContents(browser());
    EXPECT_TRUE(current_tab);
    return current_tab;
  }

  bool WaitForSavePackageToFinish(GURL* url_at_finish) const {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_SAVE_PACKAGE_SUCCESSFULLY_FINISHED,
        content::NotificationService::AllSources());
    observer.Wait();

    // Generally, there should only be one download item created
    // in all of these tests.  Wait for it, and wait for it to
    // be persisted.
    std::vector<DownloadItem*> items;
    item_creation_observer_->WaitForDownloadItem(&items);

    EXPECT_EQ(1u, items.size());
    if (1u != items.size())
      return false;
    DownloadItem* download_item(items[0]);

    // Note on synchronization:
    //
    // For each Save Page As operation, we create a corresponding shell
    // DownloadItem to display progress to the user.  That DownloadItem
    // goes through its own state transitions, including being persisted
    // out to the history database, and the download shelf is not shown
    // until after the persistence occurs.  Save Package completion (and
    // marking the DownloadItem as completed) occurs asynchronously from
    // persistence.  Thus if we want to examine either UI state or DB
    // state, we need to wait until both the save package operation is
    // complete and the relevant download item has been persisted.
    DownloadPersistedObserver(download_item).WaitForPersisted();

    *url_at_finish = content::Details<DownloadItem>(observer.details()).ptr()->
        GetOriginalUrl();
    return true;
  }

  DownloadManager* GetDownloadManager() const {
    DownloadManager* download_manager =
        BrowserContext::GetDownloadManager(browser()->profile());
    EXPECT_TRUE(download_manager);
    return download_manager;
  }

  void QueryDownloadHistory() {
    // Query the history system.
    ChromeDownloadManagerDelegate* delegate =
      static_cast<ChromeDownloadManagerDelegate*>(
          GetDownloadManager()->GetDelegate());
    delegate->download_history()->Load(
        base::Bind(&SavePageBrowserTest::OnQueryDownloadEntriesComplete,
                   base::Unretained(this)));

    // Run message loop until a quit message is sent from
    // OnQueryDownloadEntriesComplete().
    content::RunMessageLoop();
  }

  void OnQueryDownloadEntriesComplete(
      std::vector<DownloadPersistentStoreInfo>* entries) {
    history_entries_ = *entries;

    // Indicate thet we have received the history and can continue.
    MessageLoopForUI::current()->Quit();
  }

  struct DownloadPersistentStoreInfoMatch
    : public std::unary_function<DownloadPersistentStoreInfo, bool> {

    DownloadPersistentStoreInfoMatch(const GURL& url,
                                     const FilePath& path,
                                     int64 num_files,
                                     DownloadItem::DownloadState state)
      : url_(url),
        path_(path),
        num_files_(num_files),
        state_(state) {}

    bool operator() (const DownloadPersistentStoreInfo& info) const {
      return info.url == url_ &&
             info.path == path_ &&
             // For non-MHTML save packages, received_bytes is actually the
             // number of files.
             ((num_files_ < 0) ||
              (info.received_bytes == num_files_)) &&
             info.total_bytes == 0 &&
             info.state == state_;
    }

    GURL url_;
    FilePath path_;
    int64 num_files_;
    DownloadItem::DownloadState state_;
  };

  void CheckDownloadHistory(const GURL& url,
                            const FilePath& path,
                            int64 num_files,
                            DownloadItem::DownloadState state) {
    // Make sure the relevant download item made it into the history.
    std::vector<DownloadItem*> downloads;
    GetDownloadManager()->SearchDownloads(string16(), &downloads);
    ASSERT_EQ(1u, downloads.size());

    QueryDownloadHistory();

    std::vector<DownloadPersistentStoreInfo>::iterator found =
      std::find_if(history_entries_.begin(), history_entries_.end(),
                   DownloadPersistentStoreInfoMatch(url, path, num_files,
                                                    state));

    if (found == history_entries_.end()) {
      LOG(ERROR) << "Missing url=" << url.spec()
                 << " path=" << path.value()
                 << " received=" << num_files;
      for (size_t index = 0; index < history_entries_.size(); ++index) {
        LOG(ERROR) << "History@" << index << ": url="
                   << history_entries_[index].url.spec()
                   << " path=" << history_entries_[index].path.value()
                   << " received=" << history_entries_[index].received_bytes
                   << " total=" << history_entries_[index].total_bytes
                   << " state=" << history_entries_[index].state;
      }
      EXPECT_TRUE(false);
    }
  }

  std::vector<DownloadPersistentStoreInfo> history_entries_;

  // Path to directory containing test data.
  FilePath test_dir_;

  // Temporary directory we will save pages to.
  ScopedTempDir save_dir_;

 private:
  scoped_ptr<DownloadItemCreatedObserver> item_creation_observer_;

  DISALLOW_COPY_AND_ASSIGN(SavePageBrowserTest);
};

SavePageBrowserTest::~SavePageBrowserTest() {
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveHTMLOnly) {
  GURL url = NavigateToMockURL("a");

  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  ASSERT_TRUE(GetCurrentTab()->SavePage(full_file_name, dir,
                                        content::SAVE_PAGE_TYPE_AS_ONLY_HTML));

  GURL output_url;
  ASSERT_TRUE(WaitForSavePackageToFinish(&output_url));
  EXPECT_EQ(url, output_url);

  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  // a.htm is 1 file.
  CheckDownloadHistory(url, full_file_name, 1, DownloadItem::COMPLETE);

  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_FALSE(file_util::PathExists(dir));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).Append(FILE_PATH_LITERAL("a.htm")),
      full_file_name));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveHTMLOnlyCancel) {
  GURL url = NavigateToMockURL("a");
  DownloadManager* manager(GetDownloadManager());
  std::vector<DownloadItem*> downloads;
  manager->SearchDownloads(string16(), &downloads);
  ASSERT_EQ(0u, downloads.size());

  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  DownloadItemCreatedObserver creation_observer(manager);
  ASSERT_TRUE(GetCurrentTab()->SavePage(full_file_name, dir,
                                        content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  std::vector<DownloadItem*> items;
  creation_observer.WaitForDownloadItem(&items);
  ASSERT_TRUE(items.size() == 1);
  items[0]->Cancel(true);

  // TODO(rdsmith): Fix DII::Cancel() to actually cancel the save package.
  // Currently it's ignored.
  GURL output_url;
  ASSERT_TRUE(WaitForSavePackageToFinish(&output_url));
  EXPECT_EQ(url, output_url);

  // -1 to disable number of files check; we don't update after cancel, and
  // we don't know when the single file completed in relationship to
  // the cancel.
  CheckDownloadHistory(url, full_file_name, -1, DownloadItem::CANCELLED);

  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_FALSE(file_util::PathExists(dir));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).Append(FILE_PATH_LITERAL("a.htm")),
      full_file_name));
}

// SavePageBrowserTest.SaveHTMLOnlyTabDestroy is flaky.
// See http://crbug.com/144751.
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, DISABLED_SaveHTMLOnlyTabDestroy) {
  GURL url = NavigateToMockURL("a");
  DownloadManager* manager(GetDownloadManager());
  std::vector<DownloadItem*> downloads;
  manager->SearchDownloads(string16(), &downloads);
  ASSERT_EQ(0u, downloads.size());

  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  DownloadItemCreatedObserver creation_observer(manager);
  ASSERT_TRUE(GetCurrentTab()->SavePage(full_file_name, dir,
                                        content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  std::vector<DownloadItem*> items;
  creation_observer.WaitForDownloadItem(&items);
  ASSERT_TRUE(items.size() == 1);

  // Close the tab; does this cancel the download?
  GetCurrentTab()->Close();
  EXPECT_TRUE(items[0]->IsCancelled());

  EXPECT_FALSE(file_util::PathExists(full_file_name));
  EXPECT_FALSE(file_util::PathExists(dir));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveViewSourceHTMLOnly) {
  FilePath file_name(FILE_PATH_LITERAL("a.htm"));
  GURL view_source_url = URLRequestMockHTTPJob::GetMockViewSourceUrl(
      FilePath(kTestDir).Append(file_name));
  GURL actual_page_url = URLRequestMockHTTPJob::GetMockUrl(
      FilePath(kTestDir).Append(file_name));
  ui_test_utils::NavigateToURL(browser(), view_source_url);

  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  ASSERT_TRUE(GetCurrentTab()->SavePage(full_file_name, dir,
                                        content::SAVE_PAGE_TYPE_AS_ONLY_HTML));

  GURL output_url;
  ASSERT_TRUE(WaitForSavePackageToFinish(&output_url));
  EXPECT_EQ(actual_page_url, output_url);

  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  // a.htm is 1 file.
  CheckDownloadHistory(actual_page_url, full_file_name, 1,
                       DownloadItem::COMPLETE);

  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_FALSE(file_util::PathExists(dir));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).Append(file_name),
      full_file_name));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveCompleteHTML) {
  GURL url = NavigateToMockURL("b");

  FilePath full_file_name, dir;
  GetDestinationPaths("b", &full_file_name, &dir);
  ASSERT_TRUE(GetCurrentTab()->SavePage(
      full_file_name, dir, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML));

  GURL output_url;
  ASSERT_TRUE(WaitForSavePackageToFinish(&output_url));
  EXPECT_EQ(url, output_url);

  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  // b.htm is 3 files.
  CheckDownloadHistory(url, full_file_name, 3, DownloadItem::COMPLETE);

  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_TRUE(file_util::PathExists(dir));
  EXPECT_TRUE(file_util::TextContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).AppendASCII("b.saved1.htm"),
      full_file_name));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).AppendASCII("1.png"),
      dir.AppendASCII("1.png")));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).AppendASCII("1.css"),
      dir.AppendASCII("1.css")));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, NoSave) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kAboutBlankURL));
  EXPECT_FALSE(chrome::CanSavePage(browser()));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, FileNameFromPageTitle) {
  GURL url = NavigateToMockURL("b");

  FilePath full_file_name = save_dir_.path().AppendASCII(
      std::string("Test page for saving page feature") + kAppendedExtension);
  FilePath dir = save_dir_.path().AppendASCII(
      "Test page for saving page feature_files");
  ASSERT_TRUE(GetCurrentTab()->SavePage(
      full_file_name, dir, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML));

  GURL output_url;
  ASSERT_TRUE(WaitForSavePackageToFinish(&output_url));
  EXPECT_EQ(url, output_url);

  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  // b.htm is 3 files.
  CheckDownloadHistory(url, full_file_name, 3, DownloadItem::COMPLETE);

  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_TRUE(file_util::PathExists(dir));
  EXPECT_TRUE(file_util::TextContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).AppendASCII("b.saved2.htm"),
      full_file_name));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).AppendASCII("1.png"),
      dir.AppendASCII("1.png")));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).AppendASCII("1.css"),
      dir.AppendASCII("1.css")));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, RemoveFromList) {
  GURL url = NavigateToMockURL("a");

  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  ASSERT_TRUE(GetCurrentTab()->SavePage(full_file_name, dir,
                                        content::SAVE_PAGE_TYPE_AS_ONLY_HTML));

  GURL output_url;
  ASSERT_TRUE(WaitForSavePackageToFinish(&output_url));
  EXPECT_EQ(url, output_url);

  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  // a.htm is 1 file.
  CheckDownloadHistory(url, full_file_name, 1, DownloadItem::COMPLETE);

  EXPECT_EQ(GetDownloadManager()->RemoveAllDownloads(), 1);

  // Should not be in history.
  QueryDownloadHistory();
  EXPECT_EQ(std::find_if(history_entries_.begin(), history_entries_.end(),
                         DownloadPersistentStoreInfoMatch(
                             url, full_file_name, 1, DownloadItem::COMPLETE)),
      history_entries_.end());

  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_FALSE(file_util::PathExists(dir));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).Append(FILE_PATH_LITERAL("a.htm")),
      full_file_name));
}

// This tests that a webpage with the title "test.exe" is saved as
// "test.exe.htm".
// We probably don't care to handle this on Linux or Mac.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, CleanFilenameFromPageTitle) {
  const FilePath file_name(FILE_PATH_LITERAL("c.htm"));
  FilePath download_dir =
      DownloadPrefs::FromDownloadManager(GetDownloadManager())->
          DownloadPath();
  FilePath full_file_name =
      download_dir.AppendASCII(std::string("test.exe") + kAppendedExtension);
  FilePath dir = download_dir.AppendASCII("test.exe_files");

  EXPECT_FALSE(file_util::PathExists(full_file_name));
  GURL url = URLRequestMockHTTPJob::GetMockUrl(
      FilePath(kTestDir).Append(file_name));
  ui_test_utils::NavigateToURL(browser(), url);

  SavePackageFilePicker::SetShouldPromptUser(false);
  content::WindowedNotificationObserver observer(
        content::NOTIFICATION_SAVE_PACKAGE_SUCCESSFULLY_FINISHED,
        content::NotificationService::AllSources());
  chrome::SavePage(browser());
  observer.Wait();

  EXPECT_TRUE(file_util::PathExists(full_file_name));

  EXPECT_TRUE(file_util::DieFileDie(full_file_name, false));
  EXPECT_TRUE(file_util::DieFileDie(dir, true));
}
#endif

class SavePageAsMHTMLBrowserTest : public SavePageBrowserTest {
 public:
  SavePageAsMHTMLBrowserTest() {}
  virtual ~SavePageAsMHTMLBrowserTest();
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kSavePageAsMHTML);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SavePageAsMHTMLBrowserTest);
};

SavePageAsMHTMLBrowserTest::~SavePageAsMHTMLBrowserTest() {
}

IN_PROC_BROWSER_TEST_F(SavePageAsMHTMLBrowserTest, SavePageAsMHTML) {
  static const int64 kFileSizeMin = 2758;
  GURL url = NavigateToMockURL("b");
  FilePath download_dir = DownloadPrefs::FromDownloadManager(
      GetDownloadManager())->DownloadPath();
  FilePath full_file_name = download_dir.AppendASCII(std::string(
      "Test page for saving page feature.mhtml"));
#if defined(OS_CHROMEOS)
  SavePackageFilePickerChromeOS::SetShouldPromptUser(false);
#else
  SavePackageFilePicker::SetShouldPromptUser(false);
#endif
  content::WindowedNotificationObserver observer(
        content::NOTIFICATION_SAVE_PACKAGE_SUCCESSFULLY_FINISHED,
        content::NotificationService::AllSources());
  chrome::SavePage(browser());
  observer.Wait();
  CheckDownloadHistory(url, full_file_name, -1, DownloadItem::COMPLETE);

  EXPECT_TRUE(file_util::PathExists(full_file_name));
  int64 actual_file_size = -1;
  EXPECT_TRUE(file_util::GetFileSize(full_file_name, &actual_file_size));
  EXPECT_LE(kFileSizeMin, actual_file_size);
}
