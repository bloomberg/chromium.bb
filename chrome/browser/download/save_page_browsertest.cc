// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/test/test_file_util.h"
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
  DownloadRemovedObserver(Profile* profile, int32 download_id)
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
  int32 download_id_;

  DISALLOW_COPY_AND_ASSIGN(DownloadRemovedObserver);
};

bool DownloadStoredProperly(
    const GURL& expected_url,
    const base::FilePath& expected_path,
    int64 num_files,
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
    DVLOG(20) << __FUNCTION__ << " " << info.url_chain.size()
              << " != 1";
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
    DVLOG(20) << __FUNCTION__ << " " << info.state
              << " != " << expected_state;
    return false;
  }
  return true;
}

const base::FilePath::CharType kTestDir[] = FILE_PATH_LITERAL("save_page");

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
                           base::FilePath* dir) {
    *full_file_name = save_dir_.path().AppendASCII(prefix + ".htm");
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
    GetDestinationPaths(prefix_for_output_files, main_file_name, output_dir);
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

  // Path to directory containing test data.
  base::FilePath test_dir_;

  // Temporary directory we will save pages to.
  base::ScopedTempDir save_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SavePageBrowserTest);
};

// Disabled on Windows due to flakiness. http://crbug.com/162323
#if defined(OS_WIN)
#define MAYBE_SaveHTMLOnly DISABLED_SaveHTMLOnly
#else
#define MAYBE_SaveHTMLOnly SaveHTMLOnly
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, MAYBE_SaveHTMLOnly) {
  GURL url = NavigateToMockURL("a");

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_ONLY_HTML, "a", 1, &dir,
                 &full_file_name);
  ASSERT_FALSE(HasFailure());

  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_FALSE(base::PathExists(dir));
  EXPECT_TRUE(base::ContentsEqual(test_dir_.Append(base::FilePath(
      kTestDir)).Append(FILE_PATH_LITERAL("a.htm")), full_file_name));
}

// http://crbug.com/162323
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, DISABLED_SaveHTMLOnlyCancel) {
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

// Disabled on Windows due to flakiness. http://crbug.com/162323
#if defined(OS_WIN)
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
      ->SetDownloadManagerDelegateForTesting(delaying_delegate.Pass());
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

// Disabled on Windows due to flakiness. http://crbug.com/162323
#if defined(OS_WIN)
#define MAYBE_SaveViewSourceHTMLOnly DISABLED_SaveViewSourceHTMLOnly
#else
#define MAYBE_SaveViewSourceHTMLOnly SaveViewSourceHTMLOnly
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, MAYBE_SaveViewSourceHTMLOnly) {
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
  EXPECT_TRUE(base::ContentsEqual(
      test_dir_.Append(base::FilePath(kTestDir)).AppendASCII("a.htm"),
      full_file_name));
}

// Disabled on Windows due to flakiness. http://crbug.com/162323
#if defined(OS_WIN)
#define MAYBE_SaveCompleteHTML DISABLED_SaveCompleteHTML
#else
#define MAYBE_SaveCompleteHTML SaveCompleteHTML
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, DISABLED_SaveCompleteHTML) {
  GURL url = NavigateToMockURL("b");

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML, "b", 3, &dir,
                 &full_file_name);
  ASSERT_FALSE(HasFailure());

  EXPECT_TRUE(base::PathExists(full_file_name));
  EXPECT_TRUE(base::PathExists(dir));
  EXPECT_TRUE(base::TextContentsEqual(
      test_dir_.Append(base::FilePath(kTestDir)).AppendASCII("b.saved1.htm"),
      full_file_name));
  EXPECT_TRUE(base::ContentsEqual(
      test_dir_.Append(base::FilePath(kTestDir)).AppendASCII("1.png"),
      dir.AppendASCII("1.png")));
  EXPECT_TRUE(base::ContentsEqual(
      test_dir_.Append(base::FilePath(kTestDir)).AppendASCII("1.css"),
      dir.AppendASCII("1.css")));
}

// Invoke a save page during the initial navigation.
// (Regression test for http://crbug.com/156538).
// Disabled on Windows due to flakiness. http://crbug.com/162323
#if defined(OS_WIN)
#define MAYBE_SaveDuringInitialNavigationIncognito DISABLED_SaveDuringInitialNavigationIncognito
#else
#define MAYBE_SaveDuringInitialNavigationIncognito SaveDuringInitialNavigationIncognito
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest,
                       MAYBE_SaveDuringInitialNavigationIncognito) {
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

// Disabled on Windows due to flakiness. http://crbug.com/162323
#if defined(OS_WIN)
#define MAYBE_FileNameFromPageTitle DISABLED_FileNameFromPageTitle
#else
#define MAYBE_FileNameFromPageTitle FileNameFromPageTitle
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, DISABLED_FileNameFromPageTitle) {
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
  EXPECT_TRUE(base::TextContentsEqual(
      test_dir_.Append(base::FilePath(kTestDir)).AppendASCII("b.saved2.htm"),
      full_file_name));
  EXPECT_TRUE(base::ContentsEqual(
      test_dir_.Append(base::FilePath(kTestDir)).AppendASCII("1.png"),
      dir.AppendASCII("1.png")));
  EXPECT_TRUE(base::ContentsEqual(
      test_dir_.Append(base::FilePath(kTestDir)).AppendASCII("1.css"),
      dir.AppendASCII("1.css")));
}

// Disabled on Windows due to flakiness. http://crbug.com/162323
#if defined(OS_WIN)
#define MAYBE_RemoveFromList DISABLED_RemoveFromList
#else
#define MAYBE_RemoveFromList RemoveFromList
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, MAYBE_RemoveFromList) {
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
  EXPECT_TRUE(base::ContentsEqual(test_dir_.Append(base::FilePath(
      kTestDir)).Append(FILE_PATH_LITERAL("a.htm")), full_file_name));
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
  static const int64 kFileSizeMin = 2758;
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
  int64 actual_file_size = -1;
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
                 "iframe-src-is-a-download", 2, &dir, &full_file_name);
  ASSERT_FALSE(HasFailure());

  EXPECT_TRUE(base::PathExists(full_file_name));
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
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    content::SetupCrossSiteRedirector(embedded_test_server());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SavePageSitePerProcessBrowserTest);
};

// Test for crbug.com/526786.
//
// Disabled because the test will crash until the bug is fixed (but note that
// the crash only happens with --site-per-process flag).
// Currently, the crash would happen under
// blink::WebLocalFrameImpl::fromFrameOwnerElement called from
// blink::WebPageSerializerImpl::openTagToString).
IN_PROC_BROWSER_TEST_F(SavePageSitePerProcessBrowserTest,
                       DISABLED_SaveAsCompleteHtml) {
  GURL url(embedded_test_server()->GetURL("a.com", "/save_page/iframes.htm"));
  ui_test_utils::NavigateToURL(browser(), url);

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML, "iframes", 5,
                 &dir, &full_file_name);
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
    int64 actual_file_size = 0;
    EXPECT_TRUE(base::GetFileSize(file_path, &actual_file_size));
    EXPECT_NE(0, actual_file_size) << "Is " << file_path.value()
                                   << " non-empty?";
  }

  // Verify that local links got correctly replaced with local paths
  // (most importantly for iframe elements, which are only exercised
  // by this particular test).
  std::string main_contents;
  ASSERT_TRUE(base::ReadFileToString(full_file_name, &main_contents));
  EXPECT_THAT(main_contents,
              HasSubstr("<iframe src=\"./iframes_files/a.html\"></iframe>"));
  EXPECT_THAT(main_contents,
              HasSubstr("<iframe src=\"./iframes_files/b.html\"></iframe>"));
  EXPECT_THAT(main_contents,
              HasSubstr("<img src=\"./iframes_files/1.png\">"));

  // Verification of html contents.
  EXPECT_THAT(main_contents, HasSubstr("896fd88d-a77a-4f46-afd8-24db7d5af9c2"))
      << "Verifing if content from iframes.htm is present";
  std::string a_contents;
  ASSERT_TRUE(base::ReadFileToString(dir.AppendASCII("a.html"), &a_contents));
  EXPECT_THAT(a_contents, HasSubstr("1b8aae2b-e164-462f-bd5b-98aa366205f2"))
      << "Verifing if content from a.htm is present";
  std::string b_contents;
  ASSERT_TRUE(base::ReadFileToString(dir.AppendASCII("b.html"), &b_contents));
  EXPECT_THAT(b_contents, HasSubstr("3a35f7fa-96a9-4487-9f18-4470263907fa"))
      << "Verifing if content from b.htm is present";
}

// Test for crbug.com/538766.
// Disabled because the test will fail until the bug is fixed
// (but note that the test only fails with --site-per-process flag).
IN_PROC_BROWSER_TEST_F(SavePageSitePerProcessBrowserTest,
                       DISABLED_SaveAsMHTML) {
  GURL url(embedded_test_server()->GetURL("a.com", "/save_page/iframes.htm"));
  ui_test_utils::NavigateToURL(browser(), url);

  base::FilePath full_file_name, dir;
  SaveCurrentTab(url, content::SAVE_PAGE_TYPE_AS_MHTML, "iframes", -1, &dir,
                 &full_file_name);
  ASSERT_FALSE(HasFailure());

  std::string mhtml;
  ASSERT_TRUE(base::ReadFileToString(full_file_name, &mhtml));

  // Verify content of main frame, subframes and some savable resources.
  EXPECT_THAT(mhtml, HasSubstr("896fd88d-a77a-4f46-afd8-24db7d5af9c2"))
      << "Verifing if content from iframes.htm is present";
  EXPECT_THAT(mhtml, HasSubstr("1b8aae2b-e164-462f-bd5b-98aa366205f2"))
      << "Verifing if content from a.htm is present";
  EXPECT_THAT(mhtml, HasSubstr("3a35f7fa-96a9-4487-9f18-4470263907fa"))
      << "Verifing if content from b.htm is present";
  EXPECT_THAT(mhtml, HasSubstr("font-size: 20px;"))
      << "Verifing if content from 1.css is present";

  // Verify presence of URLs associated with main frame, subframes and some
  // savable resources.
  // (note that these are single-line regexes).
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location.*/save_page/iframes.htm"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location.*/save_page/a.htm"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location.*/save_page/b.htm"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location.*/save_page/1.css"));
  EXPECT_THAT(mhtml, ContainsRegex("Content-Location.*/save_page/1.png"));

  // Verify that 1.png appear in the output only once (despite being referred to
  // twice - from iframes.htm and from b.htm).
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

}  // namespace
