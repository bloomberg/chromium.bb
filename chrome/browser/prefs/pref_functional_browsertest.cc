// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"

using content::BrowserContext;
using content::DownloadManager;

class PrefsFunctionalTest : public InProcessBrowserTest {
 protected:
  // Create a DownloadTestObserverTerminal that will wait for the
  // specified number of downloads to finish.
  scoped_ptr<content::DownloadTestObserver> CreateWaiter(Browser* browser,
                                                         int num_downloads) {
    DownloadManager* download_manager =
        BrowserContext::GetDownloadManager(browser->profile());

    content::DownloadTestObserver* downloads_observer =
         new content::DownloadTestObserverTerminal(
             download_manager,
             num_downloads,
             content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
    return make_scoped_ptr(downloads_observer);
  }
};

IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, TestDownloadDirPref) {
  ASSERT_TRUE(test_server()->Start());

  DownloadManager* download_manager =
      BrowserContext::GetDownloadManager(browser()->profile());
  base::FilePath download_dir =
      (DownloadPrefs::FromDownloadManager(download_manager))->DownloadPath();
  base::FilePath new_download_dir = download_dir.AppendASCII("my_downloads");
  base::FilePath downloaded_pkg =
      new_download_dir.AppendASCII("a_zip_file.zip");

  // If the directory exists, delete it.
  if (base::PathExists(new_download_dir)) {
    base::DeleteFile(new_download_dir, true);
  }

  // Create the new downloads directory.
  file_util::CreateDirectory(new_download_dir);
  // Set pref to download in new_download_dir.
  browser()->profile()->GetPrefs()->SetFilePath(
      prefs::kDownloadDefaultDirectory,
      new_download_dir);

  // Create a downloads observer.
  scoped_ptr<content::DownloadTestObserver> downloads_observer(
      CreateWaiter(browser(), 1));
  ui_test_utils::NavigateToURL(
      browser(),
      test_server()->GetURL("files/downloads/a_zip_file.zip"));
  // Waits for the download to complete.
  downloads_observer->WaitForFinished();
  EXPECT_TRUE(base::PathExists(downloaded_pkg));
}

// Verify image content settings show or hide images.
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, TestImageContentSettings) {
  ASSERT_TRUE(test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(),
      test_server()->GetURL("files/settings/image_page.html"));

  bool result = false;
  std::string script =
      "for (i=0; i < document.images.length; i++) {"
      "  if ((document.images[i].naturalWidth != 0) &&"
      "      (document.images[i].naturalHeight != 0)) {"
      "    window.domAutomationController.send(true);"
      "  }"
      "}"
      "window.domAutomationController.send(false);";
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      script,
      &result));
  EXPECT_TRUE(result);

  base::DictionaryValue value;
  value.SetInteger("images", 2);
  browser()->profile()->GetPrefs()->Set(prefs::kDefaultContentSettings,
                                        value);

  ui_test_utils::NavigateToURL(
      browser(),
      test_server()->GetURL("files/settings/image_page.html"));

  result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      script,
      &result));
  EXPECT_FALSE(result);
}

// Verify that enabling/disabling Javascript in prefs works.
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, TestJavascriptEnableDisable) {
  ASSERT_TRUE(test_server()->Start());

  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kWebKitJavascriptEnabled));
  ui_test_utils::NavigateToURL(
      browser(),
      test_server()->GetURL("files/javaScriptTitle.html"));
  EXPECT_EQ(ASCIIToUTF16("Title from script javascript enabled"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kWebKitJavascriptEnabled,
                                               false);
  ui_test_utils::NavigateToURL(
      browser(),
      test_server()->GetURL("files/javaScriptTitle.html"));
  EXPECT_EQ(ASCIIToUTF16("This is html title"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

// Verify DNS prefetching pref.
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest, TestNetworkPredictionEnabledPref) {
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kNetworkPredictionEnabled));
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kNetworkPredictionEnabled,
                                               false);
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kNetworkPredictionEnabled));
}

// Verify restore for bookmark bar visibility.
IN_PROC_BROWSER_TEST_F(PrefsFunctionalTest,
                       TestSessionRestoreShowBookmarkBar) {
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kShowBookmarkBar));
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowBookmarkBar, true);
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kShowBookmarkBar));

  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kShowBookmarkBar));
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());
}
