// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/download/download_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/active_downloads_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/net/url_request_mock_http_job.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
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

  void CheckDownloadUI(const FilePath& download_path) {
#if defined(OS_CHROMEOS)
    Browser* popup = ActiveDownloadsUI::GetPopup();
    EXPECT_TRUE(popup);
    ActiveDownloadsUI* downloads_ui = static_cast<ActiveDownloadsUI*>(
        popup->GetSelectedTabContents()->web_ui());
    ASSERT_TRUE(downloads_ui);
    const ActiveDownloadsUI::DownloadList& downloads =
        downloads_ui->GetDownloads();
    EXPECT_EQ(downloads.size(), 1U);

    bool found = false;
    for (size_t i = 0; i < downloads.size(); ++i) {
      if (downloads[i]->full_path() == download_path) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
#else
    EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
#endif
  }

  // Changes the default folder prefs. This method saves the current folder
  // for saving HTML, the current folder for saving downloaded files,
  // the current user's "Downloads" folder and a save type (HTML only or
  // complete HTML files), and then changes them to |website_save_dir|,
  // |download_save_dir| and |save_type|, respectively.
  // If we call this method, we must call RestoreDirectoryPrefs()
  // after the test to restore the default folder prefs.
  void ChangeDirectoryPrefs(
      Profile* profile,
      const FilePath& website_save_dir,
      const FilePath& download_save_dir,
      const SavePackage::SavePackageType save_type) {
    DCHECK(profile);
    PrefService* prefs = profile->GetPrefs();

    DCHECK(prefs->FindPreference(prefs::kDownloadDefaultDirectory));
    prev_download_save_dir_ = prefs->GetFilePath(
        prefs::kDownloadDefaultDirectory);

    // Check whether the preference has the default folder for saving HTML.
    // If not, initialize it with the default folder for downloaded files.
    if (!prefs->FindPreference(prefs::kSaveFileDefaultDirectory)) {
      prefs->RegisterFilePathPref(prefs::kSaveFileDefaultDirectory,
                                  prev_download_save_dir_,
                                  PrefService::UNSYNCABLE_PREF);
    }
    prev_website_save_dir_ = prefs->GetFilePath(
        prefs::kSaveFileDefaultDirectory);

    DownloadPrefs* download_prefs =
        profile->GetDownloadManager()->download_prefs();
    prev_save_type_ =
        static_cast<SavePackage::SavePackageType>
        (download_prefs->save_file_type());

    prefs->SetFilePath(
        prefs::kSaveFileDefaultDirectory, website_save_dir);
    prefs->SetFilePath(
        prefs::kDownloadDefaultDirectory, download_save_dir);
    prefs->SetInteger(prefs::kSaveFileType, save_type);
  }

  // Restores the default folder prefs.
  void RestoreDirectoryPrefs(Profile* profile) {
    DCHECK(profile);
    PrefService* prefs = profile->GetPrefs();
    prefs->SetFilePath(
        prefs::kSaveFileDefaultDirectory, prev_website_save_dir_);
    prefs->SetFilePath(
        prefs::kDownloadDefaultDirectory, prev_download_save_dir_);
    prefs->SetInteger(prefs::kSaveFileType, prev_save_type_);
  }

  // Path to directory containing test data.
  FilePath test_dir_;

  // Temporary directory we will save pages to.
  ScopedTempDir save_dir_;

  // Temporarily stores the default folder prefs.
  FilePath prev_website_save_dir_;
  FilePath prev_download_save_dir_;
  SavePackage::SavePackageType prev_save_type_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveHTMLOnly) {
  FilePath file_name(FILE_PATH_LITERAL("a.htm"));
  GURL url = URLRequestMockHTTPJob::GetMockUrl(
      FilePath(kTestDir).Append(file_name));
  ui_test_utils::NavigateToURL(browser(), url);

  TabContentsWrapper* current_tab = browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(current_tab);

  FilePath full_file_name = save_dir_.path().Append(file_name);
  FilePath dir = save_dir_.path().AppendASCII("a_files");
  ASSERT_TRUE(current_tab->download_tab_helper()->SavePage(
      full_file_name, dir, SavePackage::SAVE_AS_ONLY_HTML));

  EXPECT_EQ(url, WaitForSavePackageToFinish());

  CheckDownloadUI(full_file_name);

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

  TabContentsWrapper* current_tab = browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(current_tab);

  FilePath full_file_name = save_dir_.path().Append(file_name);
  FilePath dir = save_dir_.path().AppendASCII("a_files");

  ASSERT_TRUE(current_tab->download_tab_helper()->SavePage(
      full_file_name, dir, SavePackage::SAVE_AS_ONLY_HTML));

  EXPECT_EQ(actual_page_url, WaitForSavePackageToFinish());

  CheckDownloadUI(full_file_name);

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

  TabContentsWrapper* current_tab = browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(current_tab);

  FilePath full_file_name = save_dir_.path().Append(file_name);
  FilePath dir = save_dir_.path().AppendASCII("b_files");
  ASSERT_TRUE(current_tab->download_tab_helper()->SavePage(
      full_file_name, dir, SavePackage::SAVE_AS_COMPLETE_HTML));

  EXPECT_EQ(url, WaitForSavePackageToFinish());

  CheckDownloadUI(full_file_name);

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

// Checks if an HTML page is saved to the default folder for saving HTML
// in the following situation:
// The default folder for saving HTML exists.
// The default folder for downloaded files exists.
// The user's "Downloads" folder exists.
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveFolder1) {
  FilePath file_name(FILE_PATH_LITERAL("a.htm"));
  GURL url = URLRequestMockHTTPJob::GetMockUrl(
      FilePath(kTestDir).Append(file_name));
  ui_test_utils::NavigateToURL(browser(), url);

  TabContentsWrapper* current_tab = browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(current_tab);
  ASSERT_TRUE(current_tab->tab_contents());
  ASSERT_TRUE(current_tab->tab_contents()->profile());

  ScopedTempDir website_save_dir, download_save_dir;
  // Prepare the default folder for saving HTML.
  ASSERT_TRUE(website_save_dir.CreateUniqueTempDir());
  // Prepare the default folder for downloaded files.
  ASSERT_TRUE(download_save_dir.CreateUniqueTempDir());

  // Changes the default prefs.
  ChangeDirectoryPrefs(
      current_tab->tab_contents()->profile(),
      website_save_dir.path(),
      download_save_dir.path(),
      SavePackage::SAVE_AS_ONLY_HTML);


  std::string ascii_basename =
      UTF16ToASCII(current_tab->download_tab_helper()->
                   SavePageBasedOnDefaultPrefs()) + ".html";
  FilePath::StringType basename;
#if defined(OS_WIN)
  basename = ASCIIToWide(ascii_basename);
#else
  basename = ascii_basename;
#endif
  file_util::ReplaceIllegalCharactersInPath(&basename, ' ');
  FilePath full_file_name = website_save_dir.path().Append(FilePath(basename));

  EXPECT_EQ(url, WaitForSavePackageToFinish());

  CheckDownloadUI(full_file_name);

  // Is the file downloaded to the default folder for saving HTML?
  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).Append(file_name),
      full_file_name));

  RestoreDirectoryPrefs(current_tab->tab_contents()->profile());
}

// Checks if an HTML page is saved to the default folder for downloaded files
// in the following situation:
// The default folder for saving HTML does not exist.
// The default folder for downloaded files exists.
// The user's "Downloads" folder exists.
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveFolder2) {
  FilePath file_name(FILE_PATH_LITERAL("a.htm"));
  GURL url = URLRequestMockHTTPJob::GetMockUrl(
      FilePath(kTestDir).Append(file_name));
  ui_test_utils::NavigateToURL(browser(), url);

  TabContentsWrapper* current_tab = browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(current_tab);
  ASSERT_TRUE(current_tab->tab_contents());
  ASSERT_TRUE(current_tab->tab_contents()->profile());

  ScopedTempDir download_save_dir;
  // Prepare the default folder for saving downloaded files.
  ASSERT_TRUE(download_save_dir.CreateUniqueTempDir());
  // Prepare non-existent folder.
  FilePath nonexistent_path(
      FILE_PATH_LITERAL("/tmp/koakuma_mikity_moemoe_nyannyan"));
  ASSERT_FALSE(file_util::PathExists(nonexistent_path));

  // Changes the default prefs.
  ChangeDirectoryPrefs(
      current_tab->tab_contents()->profile(),
      nonexistent_path,
      download_save_dir.path(),
      SavePackage::SAVE_AS_ONLY_HTML);

  std::string ascii_basename =
      UTF16ToASCII(current_tab->download_tab_helper()->
                   SavePageBasedOnDefaultPrefs()) + ".html";
  FilePath::StringType basename;
#if defined(OS_WIN)
  basename = ASCIIToWide(ascii_basename);
#else
  basename = ascii_basename;
#endif
  file_util::ReplaceIllegalCharactersInPath(&basename, ' ');
  FilePath full_file_name = download_save_dir.path().Append(FilePath(basename));

  EXPECT_EQ(url, WaitForSavePackageToFinish());

  CheckDownloadUI(full_file_name);

  // Is the file downloaded to the default folder for downloaded files?
  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_FALSE(file_util::PathExists(nonexistent_path));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).Append(file_name),
      full_file_name));

  RestoreDirectoryPrefs(current_tab->tab_contents()->profile());
}

// Checks if an HTML page is saved to the user's "Downloads" directory
// in the following situation:
// The default folder for saving HTML does not exist.
// The default folder for downloaded files does not exist.
// The user's "Downloads" folder exists.
IN_PROC_BROWSER_TEST_F(SavePageBrowserTest, SaveFolder3) {
  FilePath file_name(FILE_PATH_LITERAL("a.htm"));
  GURL url = URLRequestMockHTTPJob::GetMockUrl(
      FilePath(kTestDir).Append(file_name));
  ui_test_utils::NavigateToURL(browser(), url);

  TabContentsWrapper* current_tab = browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(current_tab);
  ASSERT_TRUE(current_tab->tab_contents());
  ASSERT_TRUE(current_tab->tab_contents()->profile());

  // Prepare non-existent folder.
  FilePath nonexistent_path1(
      FILE_PATH_LITERAL("/tmp/koakuma_mikity_moemoe_nyannyan"));
  FilePath nonexistent_path2(
      FILE_PATH_LITERAL("/tmp/koakuma_mikity_moemoe_pyonpyon"));
  ASSERT_FALSE(file_util::PathExists(nonexistent_path1));
  ASSERT_FALSE(file_util::PathExists(nonexistent_path2));

  // Changes the default prefs.
  ChangeDirectoryPrefs(
      current_tab->tab_contents()->profile(),
      nonexistent_path1,
      nonexistent_path2,
      SavePackage::SAVE_AS_ONLY_HTML);

  std::string ascii_basename =
      UTF16ToASCII(current_tab->download_tab_helper()->
                   SavePageBasedOnDefaultPrefs()) + ".html";
  FilePath::StringType basename;
#if defined(OS_WIN)
  basename = ASCIIToWide(ascii_basename);
#else
  basename = ascii_basename;
#endif
  file_util::ReplaceIllegalCharactersInPath(&basename, ' ');
  FilePath default_download_dir =
      download_util::GetDefaultDownloadDirectoryFromPathService();
  FilePath full_file_name =
      default_download_dir.Append(FilePath(basename));

  EXPECT_EQ(url, WaitForSavePackageToFinish());

  CheckDownloadUI(full_file_name);

  // Is the file downloaded to the user's "Downloads" directory?
  EXPECT_TRUE(file_util::PathExists(full_file_name));
  EXPECT_FALSE(file_util::PathExists(nonexistent_path1));
  EXPECT_FALSE(file_util::PathExists(nonexistent_path2));
  EXPECT_TRUE(file_util::ContentsEqual(
      test_dir_.Append(FilePath(kTestDir)).Append(file_name),
      full_file_name));

  RestoreDirectoryPrefs(current_tab->tab_contents()->profile());
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

  TabContentsWrapper* current_tab = browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(current_tab);

  ASSERT_TRUE(current_tab->download_tab_helper()->SavePage(
      full_file_name, dir, SavePackage::SAVE_AS_COMPLETE_HTML));

  EXPECT_EQ(url, WaitForSavePackageToFinish());

  CheckDownloadUI(full_file_name);

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
