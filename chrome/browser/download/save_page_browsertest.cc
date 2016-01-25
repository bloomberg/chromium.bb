// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/save_package_file_picker.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadItem;
using content::DownloadManager;
using content::WebContents;
using net::URLRequestMockHTTPJob;
using testing::ContainsRegex;
using testing::HasSubstr;

namespace {

// Returns file contents with each continuous run of whitespace replaced by a
// single space.
std::string ReadFileAndCollapseWhitespace(const base::FilePath& file_path) {
  std::string file_contents;
  if (!base::ReadFileToString(file_path, &file_contents)) {
    ADD_FAILURE() << "Failed to read \"" << file_path.value() << "\" file.";
    return std::string();
  }

  return base::CollapseWhitespaceASCII(file_contents, false);
}

// Waits for an item record in the downloads database to match |filter|. See
// DownloadStoredProperly() below for an example filter.
class DownloadPersistedObserver : public DownloadHistory::Observer {
 public:
  typedef base::Callback<bool(
      DownloadItem* item,
      const history::DownloadRow&)> PersistedFilter;

  DownloadPersistedObserver(Profile* profile, const PersistedFilter& filter)
      : profile_(profile),
        filter_(filter),
        persisted_(false) {
    DownloadServiceFactory::GetForBrowserContext(profile_)->
      GetDownloadHistory()->AddObserver(this);
  }

  ~DownloadPersistedObserver() override {
    DownloadService* service = DownloadServiceFactory::GetForBrowserContext(
        profile_);
    if (service && service->GetDownloadHistory())
      service->GetDownloadHistory()->RemoveObserver(this);
  }

  bool WaitForPersisted() {
    if (persisted_)
      return true;
    base::RunLoop run_loop;
    quit_waiting_callback_ = run_loop.QuitClosure();
    run_loop.Run();
    quit_waiting_callback_ = base::Closure();
    return persisted_;
  }

  void OnDownloadStored(DownloadItem* item,
                        const history::DownloadRow& info) override {
    persisted_ = persisted_ || filter_.Run(item, info);
    if (persisted_ && !quit_waiting_callback_.is_null())
      quit_waiting_callback_.Run();
  }

 private:
  Profile* profile_;
  PersistedFilter filter_;
  base::Closure quit_waiting_callback_;
  bool persisted_;

  DISALLOW_COPY_AND_ASSIGN(DownloadPersistedObserver);
};

// Waits for an item record to be removed from the downloads database.
class DownloadRemovedObserver : public DownloadPersistedObserver {
 public:
  DownloadRemovedObserver(Profile* profile, int32_t download_id)
      : DownloadPersistedObserver(profile, PersistedFilter()),
        removed_(false),
        download_id_(download_id) {}
  ~DownloadRemovedObserver() override {}

  bool WaitForRemoved() {
    if (removed_)
      return true;
    base::RunLoop run_loop;
    quit_waiting_callback_ = run_loop.QuitClosure();
    run_loop.Run();
    quit_waiting_callback_ = base::Closure();
    return removed_;
  }

  void OnDownloadStored(DownloadItem* item,
                        const history::DownloadRow& info) override {}

  void OnDownloadsRemoved(const DownloadHistory::IdSet& ids) override {
    removed_ = ids.find(download_id_) != ids.end();
    if (removed_ && !quit_waiting_callback_.is_null())
      quit_waiting_callback_.Run();
  }

 private:
  bool removed_;
  base::Closure quit_waiting_callback_;
  int32_t download_id_;

  DISALLOW_COPY_AND_ASSIGN(DownloadRemovedObserver);
};

bool DownloadStoredProperly(const GURL& expected_url,
                            const base::FilePath& expected_path,
                            int64_t num_files,
                            history::DownloadState expected_state,
                            DownloadItem* item,
                            const history::DownloadRow& info) {
  // This function may be called multiple times for a given test. Returning
  // false doesn't necessarily mean that the test has failed or will fail, it
  // might just mean that the test hasn't passed yet.
  if (!expected_path.empty() && info.target_path != expected_path) {
    DVLOG(20) << __FUNCTION__ << " " << info.target_path.value()
              << " != " << expected_path.value();
    return false;
  }
  if (info.url_chain.size() != 1u) {
    DVLOG(20) << __FUNCTION__ << " " << info.url_chain.size() << " != 1";
    return false;
  }
  if (info.url_chain[0] != expected_url) {
    DVLOG(20) << __FUNCTION__ << " " << info.url_chain[0].spec()
              << " != " << expected_url.spec();
    return false;
  }
  if ((num_files >= 0) && (info.received_bytes != num_files)) {
    DVLOG(20) << __FUNCTION__ << " " << num_files
              << " != " << info.received_bytes;
    return false;
  }
  if (info.state != expected_state) {
    DVLOG(20) << __FUNCTION__ << " " << info.state << " != " << expected_state;
    return false;
  }
  return true;
}

static const char kAppendedExtension[] = ".html";

// Loosely based on logic in DownloadTestObserver.
class DownloadItemCreatedObserver : public DownloadManager::Observer {
 public:
  explicit DownloadItemCreatedObserver(DownloadManager* manager)
      : manager_(manager) {
    manager->AddObserver(this);
  }

  ~DownloadItemCreatedObserver() override {
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
      base::RunLoop run_loop;
      quit_waiting_callback_ = run_loop.QuitClosure();
      run_loop.Run();
      quit_waiting_callback_ = base::Closure();
    }

    *items_seen = items_seen_;
    return;
  }

 private:
  // DownloadManager::Observer
  void OnDownloadCreated(DownloadManager* manager,
                         DownloadItem* item) override {
    DCHECK_EQ(manager, manager_);
    items_seen_.push_back(item);

    if (!quit_waiting_callback_.is_null())
      quit_waiting_callback_.Run();
  }

  void ManagerGoingDown(DownloadManager* manager) override {
    manager_->RemoveObserver(this);
    manager_ = NULL;
    if (!quit_waiting_callback_.is_null())
      quit_waiting_callback_.Run();
  }

  base::Closure quit_waiting_callback_;
  DownloadManager* manager_;
  std::vector<DownloadItem*> items_seen_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemCreatedObserver);
};

class SavePackageFinishedObserver : public content::DownloadManager::Observer {
 public:
  SavePackageFinishedObserver(content::DownloadManager* manager,
                              const base::Closure& callback)
      : download_manager_(manager),
        callback_(callback) {
    download_manager_->AddObserver(this);
  }

  ~SavePackageFinishedObserver() override {
    if (download_manager_)
      download_manager_->RemoveObserver(this);
  }

  // DownloadManager::Observer:
  void OnSavePackageSuccessfullyFinished(content::DownloadManager* manager,
                                         content::DownloadItem* item) override {
    callback_.Run();
  }
  void ManagerGoingDown(content::DownloadManager* manager) override {
    download_manager_->RemoveObserver(this);
    download_manager_ = NULL;
  }

 private:
  content::DownloadManager* download_manager_;
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(SavePackageFinishedObserver);
};

class SavePageBrowserTest : public InProcessBrowserTest {
 public:
  SavePageBrowserTest() {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir_));
    ASSERT_TRUE(save_dir_.CreateUniqueTempDir());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    browser()->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory, save_dir_.path());
    browser()->profile()->GetPrefs()->SetFilePath(
        prefs::kSaveFileDefaultDirectory, save_dir_.path());
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

  GURL NavigateToMockURL(const std::string& prefix) {
    GURL url = URLRequestMockHTTPJob::GetMockUrl(
        "save_page/" + prefix + ".htm");
    ui_test_utils::NavigateToURL(browser(), url);
    return url;
  }

  // Returns full paths of destination file and directory.
  void GetDestinationPaths(const std::string& prefix,
                           base::FilePath* full_file_name,
                           base::FilePath* dir,
                           content::SavePageType save_page_type =
                               content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML) {
    std::string extension =
        (save_page_type == content::SAVE_PAGE_TYPE_AS_MHTML) ? ".mht" : ".htm";
    *full_file_name = save_dir_.path().AppendASCII(prefix + extension);
    *dir = save_dir_.path().AppendASCII(prefix + "_files");
  }

  WebContents* GetCurrentTab(Browser* browser) const {
    WebContents* current_tab =
        browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(current_tab);
    return current_tab;
  }

  // Returns true if and when there was a single download created, and its url
  // is |expected_url|.
  bool VerifySavePackageExpectations(
      Browser* browser,
      const GURL& expected_url) const {
    // Generally, there should only be one download item created
    // in all of these tests.  If it's already here, grab it; if not,
    // wait for it to show up.
    std::vector<DownloadItem*> items;
    DownloadManager* manager(
        BrowserContext::GetDownloadManager(browser->profile()));
    manager->GetAllDownloads(&items);
    if (items.size() == 0u) {
      DownloadItemCreatedObserver(manager).WaitForDownloadItem(&items);
    }

    EXPECT_EQ(1u, items.size());
    if (1u != items.size())
      return false;
    DownloadItem* download_item(items[0]);

    return (expected_url == download_item->GetOriginalUrl());
  }

  void SaveCurrentTab(const GURL& url,
                      content::SavePageType save_page_type,
                      const std::string& prefix_for_output_files,
                      int expected_number_of_files,
                      base::FilePath* output_dir,
                      base::FilePath* main_file_name) {
    GetDestinationPaths(prefix_for_output_files, main_file_name, output_dir,
                        save_page_type);
    DownloadPersistedObserver persisted(
        browser()->profile(),
        base::Bind(&DownloadStoredProperly, url, *main_file_name,
                   expected_number_of_files, history::DownloadState::COMPLETE));
    base::RunLoop run_loop;
    SavePackageFinishedObserver observer(
        content::BrowserContext::GetDownloadManager(browser()->profile()),
        run_loop.QuitClosure());
    ASSERT_TRUE(GetCurrentTab(browser())
                    ->SavePage(*main_file_name, *output_dir, save_page_type));
    run_loop.Run();
    ASSERT_TRUE(VerifySavePackageExpectations(browser(), url));
    persisted.WaitForPersisted();
  }

  // Note on synchronization:
  //
  // For each Save Page As operation, we create a corresponding shell
  // DownloadItem to display progress to the user.  That DownloadItem goes
  // through its own state transitions, including being persisted out to the
  // history database, and the download shelf is not shown until after the
  // persistence occurs.  Save Package completion (and marking the DownloadItem
  // as completed) occurs asynchronously from persistence.  Thus if we want to
  // examine either UI state or DB state, we need to wait until both the save
  // package operation is complete and the relevant download item has been
  // persisted.

  DownloadManager* GetDownloadManager() const {
    DownloadManager* download_manager =
        BrowserContext::GetDownloadManager(browser()->profile());
    EXPECT_TRUE(download_manager);
    return download_manager;
  }

  // Returns full path to a file in chrome/test/data/save_page directory.
  base::FilePath GetTestDirFile(const std::string& file_name) {
    const base::FilePath::CharType kTestDir[] = FILE_PATH_LITERAL("save_page");
    return test_dir_.Append(base::FilePath(kTestDir)).AppendASCII(file_name);
  }

  // Path to directory containing test data.
  base::FilePath test_dir_;

  // Temporary directory we will save pages to.
  base::ScopedTempDir save_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SavePageBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveHTMLOnly) {
  GURL url = NavigateToMockURL("a");

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_ONLY_HTML, "a", 1, &dir,
                 &full_file_name);
  ASSERT_FALSE(HasFailure());

  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));
  EXPECT_TRUE(base::ContentsEqual(GetTestDirFile("a.htm"), full_file_name));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveHTMLOnlyCancel) {
  GURL url = NavigateToMockURL("a");
  DownloadManager* manager(GetDownloadManager());
  std::vector<DownloadItem*> downloads;
  manager->GetAllDownloads(&downloads);
  ASSERT_EQ(0u, downloads.size());

  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  DownloadItemCreatedObserver creation_observer(manager);
  DownloadPersistedObserver persisted(browser()->profile(), base::Bind(
      &DownloadStoredProperly, url, full_file_name, -1,
      history::DownloadState::CANCELLED));
  // -1 to disable number of files check; we don't update after cancel, and
  // we don't know when the single file completed in relationship to
  // the cancel.

  ASSERT_TRUE(GetCurrentTab(browser())->SavePage(full_file_name, dir,
                                        content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  std::vector<DownloadItem*> items;
  creation_observer.WaitForDownloadItem(&items);
  ASSERT_EQ(1UL, items.size());
  ASSERT_EQ(url.spec(), items[0]->GetOriginalUrl().spec());
  items[0]->Cancel(true);
  // TODO(rdsmith): Fix DII::Cancel() to actually cancel the save package.
  // Currently it's ignored.

  persisted.WaitForPersisted();

  // TODO(benjhayden): Figure out how to safely wait for SavePackage's finished
  // notification, then expect the contents of the downloaded file.
}

class DelayingDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit DelayingDownloadManagerDelegate(Profile* profile)
    : ChromeDownloadManagerDelegate(profile) {
  }
  ~DelayingDownloadManagerDelegate() override {}

  bool ShouldCompleteDownload(
      content::DownloadItem* item,
      const base::Closure& user_complete_callback) override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DelayingDownloadManagerDelegate);
};

// Disabled on ChromeOS due to flakiness. crbug.com/580766
#if defined(OS_CHROMEOS)
#define MAYBE_SaveHTMLOnlyTabDestroy DISABLED_SaveHTMLOnlyTabDestroy
#else
#define MAYBE_SaveHTMLOnlyTabDestroy SaveHTMLOnlyTabDestroy
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, MAYBE_SaveHTMLOnlyTabDestroy) {
  GURL url = NavigateToMockURL("a");
  scoped_ptr<DelayingDownloadManagerDelegate> delaying_delegate(
      new DelayingDownloadManagerDelegate(browser()->profile()));
  delaying_delegate->GetDownloadIdReceiverCallback().Run(
      content::DownloadItem::kInvalidId + 1);
  DownloadServiceFactory::GetForBrowserContext(browser()->profile())
      ->SetDownloadManagerDelegateForTesting(std::move(delaying_delegate));
  DownloadManager* manager(GetDownloadManager());
  std::vector<DownloadItem*> downloads;
  manager->GetAllDownloads(&downloads);
  ASSERT_EQ(0u, downloads.size());

  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  DownloadItemCreatedObserver creation_observer(manager);
  ASSERT_TRUE(GetCurrentTab(browser())->SavePage(full_file_name, dir,
                                        content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  std::vector<DownloadItem*> items;
  creation_observer.WaitForDownloadItem(&items);
  ASSERT_TRUE(items.size() == 1);

  // Close the tab; does this cancel the download?
  GetCurrentTab(browser())->Close();
  EXPECT_EQ(DownloadItem::CANCELLED, items[0]->GetState());

  EXPECT_FALSE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveViewSourceHTMLOnly) {
  GURL mock_url = URLRequestMockHTTPJob::GetMockUrl("save_page/a.htm");
  GURL view_source_url =
      GURL(content::kViewSourceScheme + std::string(":") + mock_url.spec());
  GURL actual_page_url = URLRequestMockHTTPJob::GetMockUrl(
      "save_page/a.htm");
  ui_test_utils::NavigateToURL(browser(), view_source_url);

  base::FilePath full_file_name, dir;
  SaveCurrentTab(actual_page_url, content::SAVE_PAGE_TYPE_AS_ONLY_HTML, "a", 1,
                 &dir, &full_file_name);
  ASSERT_FALSE(HasFailure());

  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));
  EXPECT_TRUE(base::ContentsEqual(GetTestDirFile("a.htm"), full_file_name));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveCompleteHTML) {
  GURL url = NavigateToMockURL("b");

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML, "b", 3, &dir,
                 &full_file_name);
  ASSERT_FALSE(HasFailure());

  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_TRUE(base::PathExists(dir));

  EXPECT_EQ(ReadFileAndCollapseWhitespace(full_file_name),
            ReadFileAndCollapseWhitespace(GetTestDirFile("b.saved1.htm")));
  EXPECT_TRUE(
      base::ContentsEqual(GetTestDirFile("1.png"), dir.AppendASCII("1.png")));
  EXPECT_EQ(ReadFileAndCollapseWhitespace(dir.AppendASCII("1.css")),
            ReadFileAndCollapseWhitespace(GetTestDirFile("1.css")));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest,
                       SaveDuringInitialNavigationIncognito) {
  // Open an Incognito window.
  Browser* incognito = CreateIncognitoBrowser();  // Waits.
  ASSERT_TRUE(incognito);

  // Create a download item creation waiter on that window.
  DownloadItemCreatedObserver creation_observer(
      BrowserContext::GetDownloadManager(incognito->profile()));

  // Navigate, unblocking with new tab.
  GURL url = URLRequestMockHTTPJob::GetMockUrl("save_page/b.htm");
  NavigateToURLWithDisposition(incognito, url, NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Save the page before completion.
  base::FilePath full_file_name, dir;
  GetDestinationPaths("b", &full_file_name, &dir);
  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  SavePackageFinishedObserver observer(
      content::BrowserContext::GetDownloadManager(incognito->profile()),
      loop_runner->QuitClosure());
  ASSERT_TRUE(GetCurrentTab(incognito)->SavePage(
      full_file_name, dir, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML));

  loop_runner->Run();
  ASSERT_TRUE(VerifySavePackageExpectations(incognito, url));

  // We can't check more than this because SavePackage is racing with
  // the page load.  If the page load won the race, then SavePackage
  // might have completed. If the page load lost the race, then
  // SavePackage will cancel because there aren't any resources to
  // save.
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, NoSave) {
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  EXPECT_FALSE(chrome::CanSavePage(browser()));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, FileNameFromPageTitle) {
  GURL url = NavigateToMockURL("b");

  base::FilePath full_file_name = save_dir_.path().AppendASCII(
      std::string("Test page for saving page feature") + kAppendedExtension);
  base::FilePath dir = save_dir_.path().AppendASCII(
      "Test page for saving page feature_files");
  DownloadPersistedObserver persisted(browser()->profile(), base::Bind(
      &DownloadStoredProperly, url, full_file_name, 3,
      history::DownloadState::COMPLETE));
  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  SavePackageFinishedObserver observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()),
      loop_runner->QuitClosure());
  ASSERT_TRUE(GetCurrentTab(browser())->SavePage(
      full_file_name, dir, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML));

  loop_runner->Run();
  ASSERT_TRUE(VerifySavePackageExpectations(browser(), url));
  persisted.WaitForPersisted();

  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_TRUE(base::PathExists(dir));

  EXPECT_EQ(ReadFileAndCollapseWhitespace(full_file_name),
            ReadFileAndCollapseWhitespace(GetTestDirFile("b.saved2.htm")));
  EXPECT_TRUE(
      base::ContentsEqual(GetTestDirFile("1.png"), dir.AppendASCII("1.png")));
  EXPECT_EQ(ReadFileAndCollapseWhitespace(dir.AppendASCII("1.css")),
            ReadFileAndCollapseWhitespace(GetTestDirFile("1.css")));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, RemoveFromList) {
  GURL url = NavigateToMockURL("a");

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_ONLY_HTML, "a", 1, &dir,
                 &full_file_name);
  ASSERT_FALSE(HasFailure());

  DownloadManager* manager(GetDownloadManager());
  std::vector<DownloadItem*> downloads;
  manager->GetAllDownloads(&downloads);
  ASSERT_EQ(1UL, downloads.size());
  DownloadRemovedObserver removed(browser()->profile(), downloads[0]->GetId());

  EXPECT_EQ(manager->RemoveAllDownloads(), 1);

  removed.WaitForRemoved();

  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));
  EXPECT_TRUE(base::ContentsEqual(GetTestDirFile("a.htm"), full_file_name));
}

// This tests that a webpage with the title "test.exe" is saved as
// "test.exe.htm".
// We probably don't care to handle this on Linux or Mac.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, CleanFilenameFromPageTitle) {
  base::FilePath download_dir =
      DownloadPrefs::FromDownloadManager(GetDownloadManager())->
          DownloadPath();
  base::FilePath full_file_name =
      download_dir.AppendASCII(std::string("test.exe") + kAppendedExtension);
  base::FilePath dir = download_dir.AppendASCII("test.exe_files");

  EXPECT_FALSE(base::PathExists(full_file_name));
  GURL url = URLRequestMockHTTPJob::GetMockUrl("save_page/c.htm");
  ui_test_utils::NavigateToURL(browser(), url);

  SavePackageFilePicker::SetShouldPromptUser(false);
  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  SavePackageFinishedObserver observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()),
      loop_runner->QuitClosure());
  chrome::SavePage(browser());
  loop_runner->Run();

  EXPECT_TRUE(base::PathExists(full_file_name));

  EXPECT_TRUE(base::DieFileDie(full_file_name, false));
  EXPECT_TRUE(base::DieFileDie(dir, true));
}
#endif

class SavePageAsMHTMLBrowserTest : public SavePageBrowserTest {
 public:
  SavePageAsMHTMLBrowserTest() {}
  ~SavePageAsMHTMLBrowserTest() override;
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSavePageAsMHTML);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SavePageAsMHTMLBrowserTest);
};

SavePageAsMHTMLBrowserTest::~SavePageAsMHTMLBrowserTest() {
}

IN_PROC_BROWSER_TEST_F(SavePageAsMHTMLBrowserTest, SavePageAsMHTML) {
  static const int64_t kFileSizeMin = 2758;
  GURL url = NavigateToMockURL("b");
  base::FilePath download_dir = DownloadPrefs::FromDownloadManager(
      GetDownloadManager())->DownloadPath();
  base::FilePath full_file_name = download_dir.AppendASCII(std::string(
      "Test page for saving page feature.mhtml"));
  SavePackageFilePicker::SetShouldPromptUser(false);
  DownloadPersistedObserver persisted(browser()->profile(), base::Bind(
      &DownloadStoredProperly, url, full_file_name, -1,
      history::DownloadState::COMPLETE));
  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  SavePackageFinishedObserver observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()),
      loop_runner->QuitClosure());
  chrome::SavePage(browser());
  loop_runner->Run();
  ASSERT_TRUE(VerifySavePackageExpectations(browser(), url));
  persisted.WaitForPersisted();

  ASSERT_TRUE(base::PathExists(full_file_name));
  int64_t actual_file_size = -1;
  EXPECT_TRUE(base::GetFileSize(full_file_name, &actual_file_size));
  EXPECT_LE(kFileSizeMin, actual_file_size);
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SavePageBrowserTest_NonMHTML) {
  SavePackageFilePicker::SetShouldPromptUser(false);
  GURL url("data:text/plain,foo");
  ui_test_utils::NavigateToURL(browser(), url);
  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  SavePackageFinishedObserver observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()),
      loop_runner->QuitClosure());
  chrome::SavePage(browser());
  loop_runner->Run();
  base::FilePath download_dir = DownloadPrefs::FromDownloadManager(
      GetDownloadManager())->DownloadPath();
  base::FilePath filename = download_dir.AppendASCII("dataurl.txt");
  ASSERT_TRUE(base::PathExists(filename));
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(filename, &contents));
  EXPECT_EQ("foo", contents);
}

// Test that we don't crash when the page contains an iframe that
// was handled as a download (http://crbug.com/42212).
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveDownloadableIFrame) {
  GURL url = URLRequestMockHTTPJob::GetMockUrl(
      "downloads/iframe-src-is-a-download.htm");

  // Wait for and then dismiss the non-save-page-as-related download item
  // (the one associated with downloading of "thisdayinhistory.xls" file).
  {
    GURL download_url("http://mock.http/downloads/thisdayinhistory.xls");
    DownloadPersistedObserver persisted(
        browser()->profile(),
        base::Bind(&DownloadStoredProperly, download_url, base::FilePath(), -1,
                   history::DownloadState::COMPLETE));

    ui_test_utils::NavigateToURL(browser(), url);

    ASSERT_TRUE(VerifySavePackageExpectations(browser(), download_url));
    persisted.WaitForPersisted();
    GetDownloadManager()->RemoveAllDownloads();
  }

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML,
                 "iframe-src-is-a-download", 3, &dir, &full_file_name);
  ASSERT_FALSE(HasFailure());

  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("thisdayinhistory.html")));
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("no-such-file.html")));
}

// Test suite that allows testing --site-per-process against cross-site frames.
// See http://dev.chromium.org/developers/design-documents/site-isolation.
class SavePageSitePerProcessBrowserTest : public SavePageBrowserTest {
 public:
  SavePageSitePerProcessBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SavePageBrowserTest::SetUpCommandLine(command_line);

    // Append --site-per-process flag.
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    SavePageBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    content::SetupCrossSiteRedirector(embedded_test_server());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SavePageSitePerProcessBrowserTest);
};

// Test for crbug.com/526786.
IN_PROC_BROWSER_TEST_F(SavePageSitePerProcessBrowserTest, SaveAsCompleteHtml) {
  GURL url(
      embedded_test_server()->GetURL("a.com", "/save_page/frames-xsite.htm"));
  ui_test_utils::NavigateToURL(browser(), url);

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML,
                 "frames-xsite-complete-html", 5, &dir, &full_file_name);
  ASSERT_FALSE(HasFailure());

  EXPECT_TRUE(base::DirectoryExists(dir));
  base::FilePath expected_files[] = {
      full_file_name,
      dir.AppendASCII("a.html"),  // From iframes.htm
      dir.AppendASCII("b.html"),  // From iframes.htm
      dir.AppendASCII("1.css"),   // From b.htm
      dir.AppendASCII("1.png"),   // Deduplicated from iframes.htm and b.htm.
  };
  for (auto file_path : expected_files) {
    EXPECT_TRUE(base::PathExists(file_path)) << "Does " << file_path.value()
                                             << " exist?";
    int64_t actual_file_size = 0;
    EXPECT_TRUE(base::GetFileSize(file_path, &actual_file_size));
    EXPECT_NE(0, actual_file_size) << "Is " << file_path.value()
                                   << " non-empty?";
  }

  // Verify that local links got correctly replaced with local paths
  // (most importantly for iframe elements, which are only exercised
  // by this particular test).
  std::string main_contents;
  ASSERT_TRUE(base::ReadFileToString(full_file_name, &main_contents));
  EXPECT_THAT(
      main_contents,
      HasSubstr("<iframe "
                "src=\"./frames-xsite-complete-html_files/a.html\"></iframe>"));
  EXPECT_THAT(
      main_contents,
      HasSubstr("<iframe "
                "src=\"./frames-xsite-complete-html_files/b.html\"></iframe>"));
  EXPECT_THAT(
      main_contents,
      HasSubstr("<img src=\"./frames-xsite-complete-html_files/1.png\">"));

  // Verification of html contents.
  EXPECT_THAT(
      main_contents,
      HasSubstr("frames-xsite.htm: 896fd88d-a77a-4f46-afd8-24db7d5af9c2"));
  std::string a_contents;
  ASSERT_TRUE(base::ReadFileToString(dir.AppendASCII("a.html"), &a_contents));
  EXPECT_THAT(a_contents,
              HasSubstr("a.htm: 1b8aae2b-e164-462f-bd5b-98aa366205f2"));
  std::string b_contents;
  ASSERT_TRUE(base::ReadFileToString(dir.AppendASCII("b.html"), &b_contents));
  EXPECT_THAT(b_contents,
              HasSubstr("b.htm: 3a35f7fa-96a9-4487-9f18-4470263907fa"));
}

// Test for crbug.com/538766.
IN_PROC_BROWSER_TEST_F(SavePageSitePerProcessBrowserTest, SaveAsMHTML) {
  GURL url(
      embedded_test_server()->GetURL("a.com", "/save_page/frames-xsite.htm"));
  ui_test_utils::NavigateToURL(browser(), url);

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_MHTML, "frames-xsite-mhtml",
                 -1, &dir, &full_file_name);
  ASSERT_FALSE(HasFailure());

  std::string mhtml;
  ASSERT_TRUE(base::ReadFileToString(full_file_name, &mhtml));

  // Verify content of main frame, subframes and some savable resources.
  EXPECT_THAT(
      mhtml,
      HasSubstr("frames-xsite.htm: 896fd88d-a77a-4f46-afd8-24db7d5af9c2"));
  EXPECT_THAT(mhtml, HasSubstr("a.htm: 1b8aae2b-e164-462f-bd5b-98aa366205f2"));
  EXPECT_THAT(mhtml, HasSubstr("b.htm: 3a35f7fa-96a9-4487-9f18-4470263907fa"));
  EXPECT_THAT(mhtml, HasSubstr("font-size: 20px;"))
      << "Verifying if content from 1.css is present";

  // Verify presence of URLs associated with main frame, subframes and some
  // savable resources.
  // (note that these are single-line regexes).
  EXPECT_THAT(mhtml,
              ContainsRegex("Content-Location.*/save_page/frames-xsite.htm"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location.*/save_page/a.htm"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location.*/save_page/b.htm"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location.*/save_page/1.css"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location.*/save_page/1.png"));

  // Verify that 1.png appears in the output only once (despite being referred
  // to twice - from iframes.htm and from b.htm).
  int count = 0;
  size_t pos = 0;
  for (;;) {
    pos = mhtml.find("Content-Type: image/png", pos);
    if (pos == std::string::npos)
      break;
    count++;
    pos++;
  }
  EXPECT_EQ(1, count) << "Verify number of image/png parts in the mhtml output";
}

// Test suite that verifies that the frame tree "looks" the same before
// and after a save-page-as.
class SavePageMultiFrameBrowserTest
    : public SavePageSitePerProcessBrowserTest,
      public ::testing::WithParamInterface<content::SavePageType> {
 protected:
  void TestMultiFramePage(content::SavePageType save_page_type,
                          const GURL& url,
                          int expected_number_of_frames,
                          const std::vector<std::string>& expected_substrings,
                          bool skip_verification_of_original_page = false) {
    // Navigate to the test page and verify if test expectations
    // are met (this is mostly a sanity check - a failure to meet
    // expectations would probably mean that there is a test bug
    // (i.e. that we got called with wrong expected_foo argument).
    ui_test_utils::NavigateToURL(browser(), url);
    DLOG(INFO) << "Verifying test expectations for original page... : "
               << GetCurrentTab(browser())->GetLastCommittedURL();
    if (!skip_verification_of_original_page) {
      AssertExpectationsAboutCurrentTab(expected_number_of_frames,
                                        expected_substrings);
    }

    // Save the page.
    base::FilePath full_file_name, dir;
    SaveCurrentTab(url, save_page_type, "save_result", -1, &dir,
                   &full_file_name);
    ASSERT_FALSE(HasFailure());

    // Stop the test server (to make sure the locally saved page
    // is self-contained / won't try to open original resources).
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

    // Open the saved page and verify if test expectations are
    // met (i.e. if the same expectations are met for "after"
    // [saved version of the page] as for the "before"
    // [the original version of the page].
    ui_test_utils::NavigateToURL(browser(),
                                 GURL(net::FilePathToFileURL(full_file_name)));
    DLOG(INFO) << "Verifying test expectations for saved page... : "
               << GetCurrentTab(browser())->GetLastCommittedURL();
    AssertExpectationsAboutCurrentTab(expected_number_of_frames,
                                      expected_substrings);
  }

 private:
  void AssertExpectationsAboutCurrentTab(
      int expected_number_of_frames,
      const std::vector<std::string>& expected_substrings) {
    int actual_number_of_frames = 0;
    GetCurrentTab(browser())->ForEachFrame(base::Bind(
        &IncrementInteger, base::Unretained(&actual_number_of_frames)));
    EXPECT_EQ(expected_number_of_frames, actual_number_of_frames);

    for (const auto& expected_substring : expected_substrings) {
      int actual_number_of_matches = ui_test_utils::FindInPage(
          GetCurrentTab(browser()), base::UTF8ToUTF16(expected_substring),
          true,  // |forward|
          true,  // |case_sensitive|
          nullptr, nullptr);

      EXPECT_EQ(1, actual_number_of_matches)
          << "Verifying that \"" << expected_substring << "\" appears "
          << "exactly once in the text of web contents";
    }

    std::string forbidden_substrings[] = {
        "head", // Html markup should not be visible.
        "err",  // "err" is a prefix of error messages + is strategically
                // included in some tests in contents that should not render
                // (i.e. inside of an object element and/or inside of a frame
                // that should be hidden).
    };
    for (const auto& forbidden_substring : forbidden_substrings) {
      int actual_number_of_matches = ui_test_utils::FindInPage(
          GetCurrentTab(browser()), base::UTF8ToUTF16(forbidden_substring),
          true,  // |forward|
          false,  // |case_sensitive|
          nullptr, nullptr);
      EXPECT_EQ(0, actual_number_of_matches)
          << "Verifying that \"" << forbidden_substring << "\" doesn't "
          << "appear in the text of web contents";
    }
  }

  static void IncrementInteger(int* i, content::RenderFrameHost* /* unused */) {
    (*i)++;
  }
};

// Test coverage for:
// - crbug.com/526786: OOPIFs support for CompleteHtml
// - crbug.com/538766: OOPIFs support for MHTML
// - crbug.com/539936: Subframe gets redirected.
// Test compares original-vs-saved for a page with cross-site frames
// (subframes get redirected to a different domain - see frames-xsite.htm).
IN_PROC_BROWSER_TEST_P(SavePageMultiFrameBrowserTest, CrossSite) {
  content::SavePageType save_page_type = GetParam();

  std::string arr[] = {
      "frames-xsite.htm: 896fd88d-a77a-4f46-afd8-24db7d5af9c2",
      "a.htm: 1b8aae2b-e164-462f-bd5b-98aa366205f2",
      "b.htm: 3a35f7fa-96a9-4487-9f18-4470263907fa",
  };
  std::vector<std::string> expected_substrings(std::begin(arr), std::end(arr));

  GURL url(
      embedded_test_server()->GetURL("a.com", "/save_page/frames-xsite.htm"));

  // TODO(lukasza/paulmeyer): crbug.com/457440: Can enable verification
  // of the original page once find-in-page works for OOP frames.
  bool skip_verification_of_original_page = true;

  TestMultiFramePage(save_page_type, url, 3, expected_substrings,
                     skip_verification_of_original_page);
}

// Test compares original-vs-saved for a page with <object> elements.
// (see crbug.com/553478).
IN_PROC_BROWSER_TEST_P(SavePageMultiFrameBrowserTest, ObjectElements) {
  content::SavePageType save_page_type = GetParam();

  // 4 = main frame + iframe + object w/ html doc + object w/ pdf doc
  // (svg and png objects do not get a separate frame)
  int expected_number_of_frames = 6;

  std::string arr[] = {
      "frames-objects.htm: 8da13db4-a512-4d9b-b1c5-dc1c134234b9",
      "a.htm: 1b8aae2b-e164-462f-bd5b-98aa366205f2",
      "b.htm: 3a35f7fa-96a9-4487-9f18-4470263907fa",
      "frames-nested.htm: 4388232f-8d45-4d2e-9807-721b381be153",
      "frames-nested2.htm: 6d23dc47-f283-4977-96ec-66bcf72301a4",
  };
  std::vector<std::string> expected_substrings(std::begin(arr), std::end(arr));

  GURL url(
      embedded_test_server()->GetURL("a.com", "/save_page/frames-objects.htm"));

  // TODO(lukasza): crbug.com/553478: Enable <object> testing of MHTML.
  if (save_page_type == content::SAVE_PAGE_TYPE_AS_MHTML)
    return;

  TestMultiFramePage(save_page_type, url, expected_number_of_frames,
                     expected_substrings);
}

// Test compares original-vs-saved for a page with frames at about:blank uri.
// This tests handling of iframe elements without src attribute (only with
// srcdoc attribute) and how they get saved / cross-referenced.
IN_PROC_BROWSER_TEST_P(SavePageMultiFrameBrowserTest, AboutBlank) {
  content::SavePageType save_page_type = GetParam();

  std::string arr[] = {
      "main: acb0609d-eb10-4c26-83e2-ad8afb7b0ff3",
      "sub1: b124df3a-d39f-47a1-ae04-5bb5d0bf549e",
      "sub2: 07014068-604d-45ae-884f-a068cfe7bc0a",
      "sub3: 06cc8fcc-c692-4a1a-a10f-1645b746e8f4",
  };
  std::vector<std::string> expected_substrings(std::begin(arr), std::end(arr));

  GURL url(embedded_test_server()->GetURL("a.com",
                                          "/save_page/frames-about-blank.htm"));

  TestMultiFramePage(save_page_type, url, 4, expected_substrings);
}

// Test compares original-vs-saved for a page with nested frames.
// Two levels of nesting are especially good for verifying correct
// link rewriting for subframes-vs-main-frame (see crbug.com/554666).
IN_PROC_BROWSER_TEST_P(SavePageMultiFrameBrowserTest, NestedFrames) {
  content::SavePageType save_page_type = GetParam();

  std::string arr[] = {
      "frames-nested.htm: 4388232f-8d45-4d2e-9807-721b381be153",
      "frames-nested2.htm: 6d23dc47-f283-4977-96ec-66bcf72301a4",
      "b.htm: 3a35f7fa-96a9-4487-9f18-4470263907fa",
  };
  std::vector<std::string> expected_substrings(std::begin(arr), std::end(arr));

  GURL url(
      embedded_test_server()->GetURL("a.com", "/save_page/frames-nested.htm"));

  TestMultiFramePage(save_page_type, url, 3, expected_substrings);
}

// Test for crbug.com/106364 and crbug.com/538188.
// Test frames have the same uri ...
//   subframe1 and subframe2 - both have src=b.htm
//   subframe3 and subframe4 - about:blank (no src, only srcdoc attribute).
// ... but different content (generated by main frame's javascript).
IN_PROC_BROWSER_TEST_P(SavePageMultiFrameBrowserTest, RuntimeChanges) {
  content::SavePageType save_page_type = GetParam();

  std::string arr[] = {
      "frames-runtime-changes.htm: 4388232f-8d45-4d2e-9807-721b381be153",
      "subframe1: 21595339-61fc-4854-b6df-0668328ea263",
      "subframe2: adf55719-15e7-45be-9eda-d12fe782a1bd",
      "subframe3: 50e294bf-3a5b-499d-8772-651ead26952f",
      "subframe4: e0ea9289-7467-4d32-ba5c-c604e8d84cb7",
  };
  std::vector<std::string> expected_substrings(std::begin(arr), std::end(arr));

  if (save_page_type == content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML) {
    // TODO(lukasza): crbug.com/106364: Expand complete-html test to cover all
    // test frames.  In particular, the |complete_html_arr| below should be the
    // same as the |arr| above (and at this point the special-casing of
    // complete-html can be removed).
    // Draft CLs with fix proposals that should accomplish this:
    // - crrev.com/1502563004
    // - crrev.com/1500103002
    std::string complete_html_arr[] = {
        "frames-runtime-changes.htm: 4388232f-8d45-4d2e-9807-721b381be153",
        "subframe1: 21595339-61fc-4854-b6df-0668328ea263",
        "subframe2: adf55719-15e7-45be-9eda-d12fe782a1bd",
    };
    expected_substrings = std::vector<std::string>(
        std::begin(complete_html_arr), std::end(complete_html_arr));
  }

  GURL url(embedded_test_server()->GetURL(
      "a.com", "/save_page/frames-runtime-changes.htm?do_runtime_changes=1"));

  TestMultiFramePage(save_page_type, url, 5, expected_substrings);
}

// Test for saving frames with various encodings:
// - iso-8859-2: encoding declared via <meta> element
// - utf16-le-bom.htm, utf16-be-bom.htm: encoding detected via BOM
// - utf16-le-nobom.htm, utf16-le-nobom.htm, utf32.htm - encoding declared via
//                                                       mocked http headers
IN_PROC_BROWSER_TEST_P(SavePageMultiFrameBrowserTest, Encoding) {
  content::SavePageType save_page_type = GetParam();

  std::string arr[] = {
      "frames-encodings.htm: f53295dd-a95b-4b32-85f5-b6e15377fb20",
      "iso-8859-2.htm: Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 "
          "g\xc4\x99\xc5\x9bl\xc4\x85 ja\xc5\xba\xc5\x84",
      "utf16-le-nobom.htm: Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 "
          "g\xc4\x99\xc5\x9bl\xc4\x85 ja\xc5\xba\xc5\x84",
      "utf16-le-bom.htm: Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 "
          "g\xc4\x99\xc5\x9bl\xc4\x85 ja\xc5\xba\xc5\x84",
      "utf16-be-nobom.htm: Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 "
          "g\xc4\x99\xc5\x9bl\xc4\x85 ja\xc5\xba\xc5\x84",
      "utf16-be-bom.htm: Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 "
          "g\xc4\x99\xc5\x9bl\xc4\x85 ja\xc5\xba\xc5\x84",
      "utf32.htm: Za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 "
          "g\xc4\x99\xc5\x9bl\xc4\x85 ja\xc5\xba\xc5\x84",
  };
  std::vector<std::string> expected_substrings(std::begin(arr), std::end(arr));

  GURL url(embedded_test_server()->GetURL("a.com",
                                          "/save_page/frames-encodings.htm"));

  // TODO(lukasza): crbug.com/541699: MHTML needs to handle multi-byte encodings
  // by either:
  // 1. Continuing to preserve the original encoding, but starting to round-trip
  //    the encoding declaration (in Content-Type MIME/MHTML header?)
  // 2. Saving html docs in UTF8.
  // 3. Saving the BOM (not sure if this will help for all cases though).
  if (save_page_type == content::SAVE_PAGE_TYPE_AS_MHTML)
    return;

  TestMultiFramePage(save_page_type, url, 7, expected_substrings);
}

// Test for saving style element and attribute (see also crbug.com/568293).
IN_PROC_BROWSER_TEST_P(SavePageMultiFrameBrowserTest, Style) {
  content::SavePageType save_page_type = GetParam();

  std::string arr[] = {
      "style.htm: af84c3ca-0fc6-4b0d-bf7a-5ac18a4dab62",
      "frameE: c9539ccd-47b0-47cf-a03b-734614865872",
  };
  std::vector<std::string> expected_substrings(std::begin(arr), std::end(arr));

  GURL url(embedded_test_server()->GetURL("a.com", "/save_page/style.htm"));

  TestMultiFramePage(save_page_type, url, 6, expected_substrings);
}

INSTANTIATE_TEST_CASE_P(
    SaveType,
    SavePageMultiFrameBrowserTest,
    ::testing::Values(content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML,
                      content::SAVE_PAGE_TYPE_AS_MHTML));

}  // namespace
