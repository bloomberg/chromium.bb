// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/active_downloads_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/download/download_persistent_store_info.h"
#include "content/browser/net/url_request_mock_http_job.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

static const FilePath::CharType* kTestDir = FILE_PATH_LITERAL("save_page");

static const char* kAppendedExtension =
#if defined(OS_WIN)
    ".htm";
#else
    ".html";
#endif

class SavePageBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() OVERRIDE {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir_));
    ASSERT_TRUE(save_dir_.CreateUniqueTempDir());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() OVERRIDE {
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

  TabContents* GetCurrentTab() const {
    TabContents* current_tab = browser()->GetSelectedTabContents();
    EXPECT_TRUE(current_tab);
    return current_tab;
  }


  GURL WaitForSavePackageToFinish() const {
    ui_test_utils::TestNotificationObserver observer;
    ui_test_utils::RegisterAndWait(&observer,
        content::NOTIFICATION_SAVE_PACKAGE_SUCCESSFULLY_FINISHED,
        content::NotificationService::AllSources());
    return content::Details<DownloadItem>(observer.details()).ptr()->
        original_url();
  }

#if defined(OS_CHROMEOS) && defined(TOUCH_UI)
  const ActiveDownloadsUI::DownloadList& GetDownloads() const {
    TabContents* download_contents = ActiveDownloadsUI::GetPopup(NULL);
    ActiveDownloadsUI* downloads_ui = static_cast<ActiveDownloadsUI*>(
        download_contents->web_ui());
    EXPECT_TRUE(downloads_ui);
    return downloads_ui->GetDownloads();
  }
#elif defined(OS_CHROMEOS)
  const ActiveDownloadsUI::DownloadList& GetDownloads() const {
    Browser* popup = ActiveDownloadsUI::GetPopup();
    EXPECT_TRUE(popup);
    ActiveDownloadsUI* downloads_ui = static_cast<ActiveDownloadsUI*>(
        popup->GetSelectedTabContents()->web_ui());
    EXPECT_TRUE(downloads_ui);
    return downloads_ui->GetDownloads();
  }
#endif

  void CheckDownloadUI(const FilePath& download_path) const {
    // Expectations must be in sync with the implementation in
    // Browser::OnStartDownload().
#if defined(OS_CHROMEOS)
    const ActiveDownloadsUI::DownloadList& downloads = GetDownloads();
    EXPECT_EQ(downloads.size(), 1U);

    bool found = false;
    for (size_t i = 0; i < downloads.size(); ++i) {
      if (downloads[i]->full_path() == download_path) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
#elif !defined(USE_AURA)
    EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
#else
    // TODO(jamescook): Downloads UI for non-ChromeOS Aura, crbug.com/103488
    NOTIMPLEMENTED();
#endif
  }

  DownloadManager* GetDownloadManager() const {
    DownloadManager* download_manager =
        DownloadServiceFactory::GetForProfile(
            browser()->profile())->GetDownloadManager();
    EXPECT_TRUE(download_manager);
    return download_manager;
  }

  void QueryDownloadHistory() {
    // Query the history system.
    ChromeDownloadManagerDelegate* delegate =
      static_cast<ChromeDownloadManagerDelegate*>(
          GetDownloadManager()->delegate());
    delegate->download_history()->Load(
        base::Bind(&SavePageBrowserTest::OnQueryDownloadEntriesComplete,
                   base::Unretained(this)));

    // Run message loop until a quit message is sent from
    // OnQueryDownloadEntriesComplete().
    ui_test_utils::RunMessageLoop();
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
                                     int64 num_files)
      : url_(url),
        path_(path),
        num_files_(num_files) {
    }

    bool operator() (const DownloadPersistentStoreInfo& info) const {
      return info.url == url_ &&
        info.path == path_ &&
        // For save packages, received bytes is actually the number of files.
        info.received_bytes == num_files_ &&
        info.total_bytes == 0 &&
        info.state == DownloadItem::COMPLETE;
    }

    GURL url_;
    FilePath path_;
    int64 num_files_;
  };

  void CheckDownloadHistory(const GURL& url,
                            const FilePath& path,
                            int64 num_files_) {
    QueryDownloadHistory();

    EXPECT_NE(std::find_if(history_entries_.begin(), history_entries_.end(),
        DownloadPersistentStoreInfoMatch(url, path, num_files_)),
        history_entries_.end());
  }

  std::vector<DownloadPersistentStoreInfo> history_entries_;

  // Path to directory containing test data.
  FilePath test_dir_;

  // Temporary directory we will save pages to.
  ScopedTempDir save_dir_;
};

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveHTMLOnly) {
  GURL url = NavigateToMockURL("a");

  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  ASSERT_TRUE(GetCurrentTab()->SavePage(full_file_name, dir,
                                        SavePackage::SAVE_AS_ONLY_HTML));

  EXPECT_EQ(url, WaitForSavePackageToFinish());

  CheckDownloadUI(full_file_name);
  CheckDownloadHistory(url, full_file_name, 1);  // a.htm is 1 file.

  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_FALSE(file_util::PathExists(dir));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).Append(FILE_PATH_LITERAL("a.htm")),
      full_file_name));
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
                                        SavePackage::SAVE_AS_ONLY_HTML));

  EXPECT_EQ(actual_page_url, WaitForSavePackageToFinish());

  CheckDownloadUI(full_file_name);
  CheckDownloadHistory(actual_page_url, full_file_name, 1);  // a.htm is 1 file.

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
  ASSERT_TRUE(GetCurrentTab()->SavePage(full_file_name, dir,
                                        SavePackage::SAVE_AS_COMPLETE_HTML));

  EXPECT_EQ(url, WaitForSavePackageToFinish());

  CheckDownloadUI(full_file_name);
  CheckDownloadHistory(url, full_file_name, 3);  // b.htm is 3 files.

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
  ASSERT_TRUE(browser()->command_updater()->SupportsCommand(IDC_SAVE_PAGE));
  EXPECT_FALSE(browser()->command_updater()->IsCommandEnabled(IDC_SAVE_PAGE));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, FileNameFromPageTitle) {
  GURL url = NavigateToMockURL("b");

  FilePath full_file_name = save_dir_.path().AppendASCII(
      std::string("Test page for saving page feature") + kAppendedExtension);
  FilePath dir = save_dir_.path().AppendASCII(
      "Test page for saving page feature_files");
  ASSERT_TRUE(GetCurrentTab()->SavePage(full_file_name, dir,
                                        SavePackage::SAVE_AS_COMPLETE_HTML));

  EXPECT_EQ(url, WaitForSavePackageToFinish());

  CheckDownloadUI(full_file_name);
  CheckDownloadHistory(url, full_file_name, 3);  // b.htm is 3 files.

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
                                        SavePackage::SAVE_AS_ONLY_HTML));

  EXPECT_EQ(url, WaitForSavePackageToFinish());

  CheckDownloadUI(full_file_name);
  CheckDownloadHistory(url, full_file_name, 1);  // a.htm is 1 file.

  EXPECT_EQ(GetDownloadManager()->RemoveAllDownloads(), 1);

#if defined(OS_CHROMEOS)
  EXPECT_EQ(GetDownloads().size(), 0U);
#endif

  // Should not be in history.
  QueryDownloadHistory();
  EXPECT_EQ(std::find_if(history_entries_.begin(), history_entries_.end(),
      DownloadPersistentStoreInfoMatch(url, full_file_name, 1)),
      history_entries_.end());

  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_FALSE(file_util::PathExists(dir));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).Append(FILE_PATH_LITERAL("a.htm")),
      full_file_name));
}

// Create a SavePackage and delete it without calling Init.
// SavePackage dtor has various asserts/checks that should not fire.
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, ImplicitCancel) {
  GURL url = NavigateToMockURL("a");
  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  scoped_refptr<SavePackage> save_package(new SavePackage(GetCurrentTab(),
      SavePackage::SAVE_AS_ONLY_HTML, full_file_name, dir));
}

// Create a SavePackage, call Cancel, then delete it.
// SavePackage dtor has various asserts/checks that should not fire.
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, ExplicitCancel) {
  GURL url = NavigateToMockURL("a");
  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  scoped_refptr<SavePackage> save_package(new SavePackage(GetCurrentTab(),
      SavePackage::SAVE_AS_ONLY_HTML, full_file_name, dir));
  save_package->Cancel(true);
}

}  // namespace
