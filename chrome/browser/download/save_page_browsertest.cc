// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  void SetUp() {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir_));
    ASSERT_TRUE(save_dir_.CreateUniqueTempDir());
    InProcessBrowserTest::SetUp();
  }

  GURL WaitForSavePackageToFinish() {
    ui_test_utils::TestNotificationObserver observer;
    ui_test_utils::RegisterAndWait(&observer,
        NotificationType::SAVE_PACKAGE_SUCCESSFULLY_FINISHED,
        NotificationService::AllSources());
    return *Details<GURL>(observer.details()).ptr();
  }

  void QueryDownloadHistory(TabContents* current_tab) {
    DownloadManager* download_manager =
      current_tab->profile()->GetDownloadManager();

    // Query the history system.
    download_manager->download_history()->Load(
      NewCallback(this,
        &SavePageBrowserTest::OnQueryDownloadEntriesComplete));

    // Run message loop until a quit message is sent from
    // OnQueryDownloadEntriesComplete().
    ui_test_utils::RunMessageLoop();
  }

  void OnQueryDownloadEntriesComplete(
    std::vector<DownloadCreateInfo>* entries) {

    // Make a copy of the URLs returned by the history system.
    history_urls_.clear();
    for (size_t i = 0; i < entries->size(); ++i) {
      history_urls_.push_back(entries->at(i).url);
    }

    // Indicate thet we have received the history and
    // can continue.
    MessageLoopForUI::current()->Quit();
  }

  // URLs found in the history.
  std::vector<GURL> history_urls_;

  // Path to directory containing test data.
  FilePath test_dir_;

  // Temporary directory we will save pages to.
  ScopedTempDir save_dir_;
};

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveHTMLOnly) {
  FilePath file_name(FILE_PATH_LITERAL("a.htm"));
  GURL url = URLRequestMockHTTPJob::GetMockUrl(
      FilePath(kTestDir).Append(file_name));
  ui_test_utils::NavigateToURL(browser(), url);

  TabContents* current_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(current_tab);

  FilePath full_file_name = save_dir_.path().Append(file_name);
  FilePath dir = save_dir_.path().AppendASCII("a_files");
  ASSERT_TRUE(current_tab->SavePage(full_file_name, dir,
                                    SavePackage::SAVE_AS_ONLY_HTML));

  EXPECT_EQ(url, WaitForSavePackageToFinish());

  if (browser()->SupportsWindowFeature(Browser::FEATURE_DOWNLOADSHELF))
    EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  QueryDownloadHistory(current_tab);
  EXPECT_TRUE(std::find(history_urls_.begin(), history_urls_.end(),
                        url) != history_urls_.end());

  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_FALSE(file_util::PathExists(dir));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).Append(file_name),
      full_file_name));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveViewSourceHTMLOnly) {
  FilePath file_name(FILE_PATH_LITERAL("a.htm"));
  GURL view_source_url = URLRequestMockHTTPJob::GetMockViewSourceUrl(
      FilePath(kTestDir).Append(file_name));
  GURL actual_page_url = URLRequestMockHTTPJob::GetMockUrl(
      FilePath(kTestDir).Append(file_name));
  ui_test_utils::NavigateToURL(browser(), view_source_url);

  TabContents* current_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(current_tab);

  FilePath full_file_name = save_dir_.path().Append(file_name);
  FilePath dir = save_dir_.path().AppendASCII("a_files");

  ASSERT_TRUE(current_tab->SavePage(full_file_name, dir,
                                    SavePackage::SAVE_AS_ONLY_HTML));

  EXPECT_EQ(actual_page_url, WaitForSavePackageToFinish());

  if (browser()->SupportsWindowFeature(Browser::FEATURE_DOWNLOADSHELF))
    EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  QueryDownloadHistory(current_tab);
  EXPECT_TRUE(std::find(history_urls_.begin(), history_urls_.end(),
                        actual_page_url) != history_urls_.end());

  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_FALSE(file_util::PathExists(dir));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).Append(file_name),
      full_file_name));
}

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveCompleteHTML) {
  FilePath file_name(FILE_PATH_LITERAL("b.htm"));
  GURL url = URLRequestMockHTTPJob::GetMockUrl(
      FilePath(kTestDir).Append(file_name));
  ui_test_utils::NavigateToURL(browser(), url);

  TabContents* current_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(current_tab);

  FilePath full_file_name = save_dir_.path().Append(file_name);
  FilePath dir = save_dir_.path().AppendASCII("b_files");
  ASSERT_TRUE(current_tab->SavePage(full_file_name, dir,
                                    SavePackage::SAVE_AS_COMPLETE_HTML));

  EXPECT_EQ(url, WaitForSavePackageToFinish());

  if (browser()->SupportsWindowFeature(Browser::FEATURE_DOWNLOADSHELF))
    EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  QueryDownloadHistory(current_tab);
  EXPECT_TRUE(std::find(history_urls_.begin(), history_urls_.end(),
                        url) != history_urls_.end());

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
  FilePath file_name(FILE_PATH_LITERAL("b.htm"));

  GURL url = URLRequestMockHTTPJob::GetMockUrl(
      FilePath(kTestDir).Append(file_name));
  ui_test_utils::NavigateToURL(browser(), url);

  FilePath full_file_name = save_dir_.path().AppendASCII(
      std::string("Test page for saving page feature") + kAppendedExtension);
  FilePath dir = save_dir_.path().AppendASCII(
      "Test page for saving page feature_files");

  TabContents* current_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(current_tab);

  ASSERT_TRUE(current_tab->SavePage(full_file_name, dir,
                                    SavePackage::SAVE_AS_COMPLETE_HTML));

  EXPECT_EQ(url, WaitForSavePackageToFinish());

  if (browser()->SupportsWindowFeature(Browser::FEATURE_DOWNLOADSHELF))
    EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  QueryDownloadHistory(current_tab);
  EXPECT_TRUE(std::find(history_urls_.begin(), history_urls_.end(),
                        url) != history_urls_.end());

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

}  // namespace
