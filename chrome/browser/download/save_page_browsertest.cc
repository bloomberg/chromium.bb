// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/prefs/public/pref_member.h"
#include "base/test/test_file_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/history/download_row.h"
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
using content::URLRequestMockHTTPJob;
using content::WebContents;

// Waits for an item record in the downloads database to match |filter|. See
// DownloadStoredProperly() below for an example filter.
class DownloadPersistedObserver : public DownloadHistory::Observer {
 public:
  typedef base::Callback<bool(
      DownloadItem* item,
      const history::DownloadRow&)> PersistedFilter;

  DownloadPersistedObserver(Profile* profile, const PersistedFilter& filter)
    : profile_(profile),
      filter_(filter) {
    DownloadServiceFactory::GetForProfile(profile_)->
      GetDownloadHistory()->AddObserver(this);
  }

  virtual ~DownloadPersistedObserver() {
    DownloadService* service = DownloadServiceFactory::GetForProfile(profile_);
    if (service && service->GetDownloadHistory())
      service->GetDownloadHistory()->RemoveObserver(this);
  }

  bool WaitForPersisted() {
    if (persisted_)
      return true;
    waiting_ = true;
    content::RunMessageLoop();
    waiting_ = false;
    return persisted_;
  }

  virtual void OnDownloadStored(DownloadItem* item,
                                const history::DownloadRow& info) {
    persisted_ = filter_.Run(item, info);
    if (persisted_ && waiting_)
      MessageLoopForUI::current()->Quit();
  }

 private:
  Profile* profile_;
  DownloadItem* item_;
  PersistedFilter filter_;
  bool waiting_;
  bool persisted_;

  DISALLOW_COPY_AND_ASSIGN(DownloadPersistedObserver);
};

// Waits for an item record to be removed from the downloads database.
class DownloadRemovedObserver : public DownloadPersistedObserver {
 public:
  DownloadRemovedObserver(Profile* profile, int32 download_id)
      : DownloadPersistedObserver(profile, PersistedFilter()),
        removed_(false),
        waiting_(false),
        download_id_(download_id) {
  }
  virtual ~DownloadRemovedObserver() {}

  bool WaitForRemoved() {
    if (removed_)
      return true;
    waiting_ = true;
    content::RunMessageLoop();
    waiting_ = false;
    return removed_;
  }

  virtual void OnDownloadStored(DownloadItem* item,
                                const history::DownloadRow& info) {
  }

  virtual void OnDownloadsRemoved(const DownloadHistory::IdSet& ids) {
    removed_ = ids.find(download_id_) != ids.end();
    if (removed_ && waiting_)
      MessageLoopForUI::current()->Quit();
  }

 private:
  bool removed_;
  bool waiting_;
  int32 download_id_;

  DISALLOW_COPY_AND_ASSIGN(DownloadRemovedObserver);
};

bool DownloadStoredProperly(
    const GURL& expected_url,
    const FilePath& expected_path,
    int64 num_files,
    DownloadItem::DownloadState expected_state,
    DownloadItem* item,
    const history::DownloadRow& info) {
  // This function may be called multiple times for a given test. Returning
  // false doesn't necessarily mean that the test has failed or will fail, it
  // might just mean that the test hasn't passed yet.
  if (info.path != expected_path) {
    VLOG(20) << __FUNCTION__ << " " << info.path.value()
             << " != " << expected_path.value();
    return false;
  }
  if (info.url != expected_url) {
    VLOG(20) << __FUNCTION__ << " " << info.url.spec()
             << " != " << expected_url.spec();
    return false;
  }
  if ((num_files >= 0) && (info.received_bytes != num_files)) {
    VLOG(20) << __FUNCTION__ << " " << num_files
             << " != " << info.received_bytes;
    return false;
  }
  if (info.state != expected_state) {
    VLOG(20) << __FUNCTION__ << " " << info.state
             << " != " << expected_state;
    return false;
  }
  return true;
}

const FilePath::CharType kTestDir[] = FILE_PATH_LITERAL("save_page");

static const char kAppendedExtension[] =
#if defined(OS_WIN)
    ".htm";
#else
    ".html";
#endif

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

  WebContents* GetCurrentTab(Browser* browser) const {
    WebContents* current_tab = chrome::GetActiveWebContents(browser);
    EXPECT_TRUE(current_tab);
    return current_tab;
  }

  // Returns true if and when there was a single download created, and its url
  // is |expected_url|.
  bool WaitForSavePackageToFinish(
      Browser* browser,
      const GURL& expected_url) const {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_SAVE_PACKAGE_SUCCESSFULLY_FINISHED,
        content::NotificationService::AllSources());
    observer.Wait();

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

    return ((expected_url == download_item->GetOriginalUrl()) &&
            (expected_url == content::Details<DownloadItem>(
                observer.details()).ptr()->GetOriginalUrl()));
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
  FilePath test_dir_;

  // Temporary directory we will save pages to.
  base::ScopedTempDir save_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SavePageBrowserTest);
};

SavePageBrowserTest::~SavePageBrowserTest() {
}

// Disabled on Windows due to flakiness. http://crbug.com/162323
#if defined(OS_WIN)
#define MAYBE_SaveHTMLOnly DISABLED_SaveHTMLOnly
#else
#define MAYBE_SaveHTMLOnly SaveHTMLOnly
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, MAYBE_SaveHTMLOnly) {
  GURL url = NavigateToMockURL("a");

  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  DownloadPersistedObserver persisted(browser()->profile(), base::Bind(
      &DownloadStoredProperly, url, full_file_name, 1,
      DownloadItem::COMPLETE));
  ASSERT_TRUE(GetCurrentTab(browser())->SavePage(full_file_name, dir,
                                        content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  ASSERT_TRUE(WaitForSavePackageToFinish(browser(), url));
  persisted.WaitForPersisted();
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_FALSE(file_util::PathExists(dir));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).Append(FILE_PATH_LITERAL("a.htm")),
      full_file_name));
}

// Disabled on Windows due to flakiness. http://crbug.com/162323
#if defined(OS_WIN)
#define MAYBE_SaveHTMLOnlyCancel DISABLED_SaveHTMLOnlyCancel
#else
#define MAYBE_SaveHTMLOnlyCancel SaveHTMLOnlyCancel
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, MAYBE_SaveHTMLOnlyCancel) {
  GURL url = NavigateToMockURL("a");
  DownloadManager* manager(GetDownloadManager());
  std::vector<DownloadItem*> downloads;
  manager->GetAllDownloads(&downloads);
  ASSERT_EQ(0u, downloads.size());

  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  DownloadItemCreatedObserver creation_observer(manager);
  DownloadPersistedObserver persisted(browser()->profile(), base::Bind(
      &DownloadStoredProperly, url, full_file_name, -1,
      DownloadItem::CANCELLED));
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

  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // TODO(benjhayden): Figure out how to safely wait for SavePackage's finished
  // notification, then expect the contents of the downloaded file.
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveHTMLOnlyTabDestroy) {
  GURL url = NavigateToMockURL("a");
  DownloadManager* manager(GetDownloadManager());
  std::vector<DownloadItem*> downloads;
  manager->GetAllDownloads(&downloads);
  ASSERT_EQ(0u, downloads.size());

  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  DownloadItemCreatedObserver creation_observer(manager);
  ASSERT_TRUE(GetCurrentTab(browser())->SavePage(full_file_name, dir,
                                        content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  std::vector<DownloadItem*> items;
  creation_observer.WaitForDownloadItem(&items);
  ASSERT_TRUE(items.size() == 1);

  // Close the tab; does this cancel the download?
  GetCurrentTab(browser())->Close();
  EXPECT_TRUE(items[0]->IsCancelled());

  EXPECT_FALSE(file_util::PathExists(full_file_name));
  EXPECT_FALSE(file_util::PathExists(dir));
}

// Disabled on Windows due to flakiness. http://crbug.com/162323
#if defined(OS_WIN)
#define MAYBE_SaveViewSourceHTMLOnly DISABLED_SaveViewSourceHTMLOnly
#else
#define MAYBE_SaveViewSourceHTMLOnly SaveViewSourceHTMLOnly
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, MAYBE_SaveViewSourceHTMLOnly) {
  FilePath file_name(FILE_PATH_LITERAL("a.htm"));
  GURL view_source_url = URLRequestMockHTTPJob::GetMockViewSourceUrl(
      FilePath(kTestDir).Append(file_name));
  GURL actual_page_url = URLRequestMockHTTPJob::GetMockUrl(
      FilePath(kTestDir).Append(file_name));
  ui_test_utils::NavigateToURL(browser(), view_source_url);

  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  DownloadPersistedObserver persisted(browser()->profile(), base::Bind(
      &DownloadStoredProperly, actual_page_url, full_file_name, 1,
      DownloadItem::COMPLETE));
  ASSERT_TRUE(GetCurrentTab(browser())->SavePage(full_file_name, dir,
                                        content::SAVE_PAGE_TYPE_AS_ONLY_HTML));
  ASSERT_TRUE(WaitForSavePackageToFinish(browser(), actual_page_url));
  persisted.WaitForPersisted();

  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_FALSE(file_util::PathExists(dir));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).Append(file_name),
      full_file_name));
}

// Disabled on Windows due to flakiness. http://crbug.com/162323
#if defined(OS_WIN)
#define MAYBE_SaveCompleteHTML DISABLED_SaveCompleteHTML
#else
#define MAYBE_SaveCompleteHTML SaveCompleteHTML
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, MAYBE_SaveCompleteHTML) {
  GURL url = NavigateToMockURL("b");

  FilePath full_file_name, dir;
  GetDestinationPaths("b", &full_file_name, &dir);
  DownloadPersistedObserver persisted(browser()->profile(), base::Bind(
      &DownloadStoredProperly, url, full_file_name, 3,
      DownloadItem::COMPLETE));
  ASSERT_TRUE(GetCurrentTab(browser())->SavePage(
      full_file_name, dir, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML));
  ASSERT_TRUE(WaitForSavePackageToFinish(browser(), url));
  persisted.WaitForPersisted();

  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

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

// Invoke a save page during the initial navigation.
// (Regression test for http://crbug.com/156538).
// Disabled on Windows due to flakiness. http://crbug.com/162323
// TODO(linux_aura) http://crbug.com/163931
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA))
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
  GURL url = URLRequestMockHTTPJob::GetMockUrl(
      FilePath(kTestDir).AppendASCII("b.htm"));
  NavigateToURLWithDisposition(incognito, url, NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Save the page before completion.
  FilePath full_file_name, dir;
  GetDestinationPaths("b", &full_file_name, &dir);
  ASSERT_TRUE(GetCurrentTab(incognito)->SavePage(
      full_file_name, dir, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML));

  ASSERT_TRUE(WaitForSavePackageToFinish(incognito, url));

  // Confirm download shelf is visible.
  EXPECT_TRUE(incognito->window()->IsDownloadShelfVisible());

  // We can't check more than this because SavePackage is racing with
  // the page load.  If the page load won the race, then SavePackage
  // might have completed. If the page load lost the race, then
  // SavePackage will cancel because there aren't any resources to
  // save.
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, NoSave) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kAboutBlankURL));
  EXPECT_FALSE(chrome::CanSavePage(browser()));
}

// Disabled on Windows due to flakiness. http://crbug.com/162323
#if defined(OS_WIN)
#define MAYBE_FileNameFromPageTitle DISABLED_FileNameFromPageTitle
#else
#define MAYBE_FileNameFromPageTitle FileNameFromPageTitle
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, MAYBE_FileNameFromPageTitle) {
  GURL url = NavigateToMockURL("b");

  FilePath full_file_name = save_dir_.path().AppendASCII(
      std::string("Test page for saving page feature") + kAppendedExtension);
  FilePath dir = save_dir_.path().AppendASCII(
      "Test page for saving page feature_files");
  DownloadPersistedObserver persisted(browser()->profile(), base::Bind(
      &DownloadStoredProperly, url, full_file_name, 3,
      DownloadItem::COMPLETE));
  ASSERT_TRUE(GetCurrentTab(browser())->SavePage(
      full_file_name, dir, content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML));

  ASSERT_TRUE(WaitForSavePackageToFinish(browser(), url));
  persisted.WaitForPersisted();

  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

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

// Disabled on Windows due to flakiness. http://crbug.com/162323
#if defined(OS_WIN)
#define MAYBE_RemoveFromList DISABLED_RemoveFromList
#else
#define MAYBE_RemoveFromList RemoveFromList
#endif
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, MAYBE_RemoveFromList) {
  GURL url = NavigateToMockURL("a");

  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  DownloadPersistedObserver persisted(browser()->profile(), base::Bind(
      &DownloadStoredProperly, url, full_file_name, 1,
      DownloadItem::COMPLETE));
  ASSERT_TRUE(GetCurrentTab(browser())->SavePage(full_file_name, dir,
                                        content::SAVE_PAGE_TYPE_AS_ONLY_HTML));

  ASSERT_TRUE(WaitForSavePackageToFinish(browser(), url));
  persisted.WaitForPersisted();

  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  DownloadManager* manager(GetDownloadManager());
  std::vector<DownloadItem*> downloads;
  manager->GetAllDownloads(&downloads);
  ASSERT_EQ(1UL, downloads.size());
  DownloadRemovedObserver removed(browser()->profile(), downloads[0]->GetId());

  EXPECT_EQ(manager->RemoveAllDownloads(), 1);

  removed.WaitForRemoved();

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
  DownloadPersistedObserver persisted(browser()->profile(), base::Bind(
      &DownloadStoredProperly, url, full_file_name, -1,
      DownloadItem::COMPLETE));
  chrome::SavePage(browser());
  ASSERT_TRUE(WaitForSavePackageToFinish(browser(), url));
  persisted.WaitForPersisted();

  EXPECT_TRUE(file_util::PathExists(full_file_name));
  int64 actual_file_size = -1;
  EXPECT_TRUE(file_util::GetFileSize(full_file_name, &actual_file_size));
  EXPECT_LE(kFileSizeMin, actual_file_size);
}
