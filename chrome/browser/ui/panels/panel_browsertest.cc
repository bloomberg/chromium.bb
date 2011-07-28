// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/net/url_request_mock_http_job.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

class PanelBrowserTest : public InProcessBrowserTest {
 public:
  PanelBrowserTest() : InProcessBrowserTest() { }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnablePanels);
  }
};

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, CreatePanel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  EXPECT_EQ(0, panel_manager->active_count()); // No panels initially.

  Browser* panel_browser = Browser::CreateForApp(Browser::TYPE_PANEL,
                                                 "PanelTest",
                                                 gfx::Rect(),
                                                 browser()->profile());
  EXPECT_TRUE(panel_browser->is_type_panel());

  TabContentsWrapper* tab_contents =
      new TabContentsWrapper(new TestTabContents(browser()->profile(), NULL));
  panel_browser->AddTab(tab_contents, PageTransition::LINK);

  panel_browser->window()->Show();
  EXPECT_EQ(1, panel_manager->active_count());

  gfx::Rect bounds = panel_browser->window()->GetBounds();
  EXPECT_GT(bounds.x(), 0);
  EXPECT_GT(bounds.y(), 0);
  EXPECT_GT(bounds.width(), 0);
  EXPECT_GT(bounds.height(), 0);

  panel_browser->window()->Close();
  EXPECT_EQ(0, panel_manager->active_count());
}

class PanelDownloadTest : public PanelBrowserTest {
 public:
  PanelDownloadTest() : PanelBrowserTest() { }

  // Creates a temporary directory for downloads that is auto-deleted
  // on destruction.
  bool CreateDownloadDirectory(Profile* profile) {
    bool created = downloads_directory_.CreateUniqueTempDir();
    if (!created)
      return false;
    profile->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory,
        downloads_directory_.path());
    return true;
  }

 private:
  // Location of the downloads directory for download tests.
  ScopedTempDir downloads_directory_;
};

class DownloadObserver : public DownloadManager::Observer {
 public:
  explicit DownloadObserver(Profile* profile)
      : download_manager_(profile->GetDownloadManager()),
        saw_download_(false),
        waiting_(false) {
    download_manager_->AddObserver(this);
  }

  ~DownloadObserver() {
    download_manager_->RemoveObserver(this);
  }

  void WaitForDownload() {
    if (!saw_download_) {
      waiting_ = true;
      ui_test_utils::RunMessageLoop();
      EXPECT_TRUE(saw_download_);
      waiting_ = false;
    }
  }

  // DownloadManager::Observer
  virtual void ModelChanged() {
    std::vector<DownloadItem*> downloads;
    download_manager_->GetCurrentDownloads(FilePath(), &downloads);
    if (downloads.empty())
      return;

    EXPECT_EQ(1U, downloads.size());
    downloads.front()->Cancel(false);  // Don't actually need to download it.

    saw_download_ = true;
    EXPECT_TRUE(waiting_);
    MessageLoopForUI::current()->Quit();
  }

 private:
  DownloadManager* download_manager_;
  bool saw_download_;
  bool waiting_;
};

// Verify that the download shelf is opened in the existing tabbed browser
// when a download is started in a Panel.
IN_PROC_BROWSER_TEST_F(PanelDownloadTest, Download) {
  Profile* profile = browser()->profile();
  ASSERT_TRUE(CreateDownloadDirectory(profile));
  Browser* panel_browser = Browser::CreateForApp(Browser::TYPE_PANEL,
                                                 "PanelTest",
                                                 gfx::Rect(),
                                                 profile);
  EXPECT_EQ(2U, BrowserList::size());
  ASSERT_FALSE(browser()->window()->IsDownloadShelfVisible());
  ASSERT_FALSE(panel_browser->window()->IsDownloadShelfVisible());

  scoped_ptr<DownloadObserver> observer(new DownloadObserver(profile));
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL download_url(URLRequestMockHTTPJob::GetMockUrl(file));
  ui_test_utils::NavigateToURLWithDisposition(
      panel_browser,
      download_url,
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  observer->WaitForDownload();

#if defined(OS_CHROMEOS)
  // ChromeOS uses a download panel instead of a download shelf.
  EXPECT_EQ(3U, BrowserList::size());
  ASSERT_FALSE(browser()->window()->IsDownloadShelfVisible());

  std::set<Browser*> original_browsers;
  original_browsers.insert(browser());
  original_browsers.insert(panel_browser);
  Browser* added = ui_test_utils::GetBrowserNotInSet(original_browsers);
  ASSERT_TRUE(added->is_type_panel());
  ASSERT_FALSE(added->window()->IsDownloadShelfVisible());
#else
  EXPECT_EQ(2U, BrowserList::size());
  ASSERT_TRUE(browser()->window()->IsDownloadShelfVisible());
#endif

  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, panel_browser->tab_count());
  ASSERT_FALSE(panel_browser->window()->IsDownloadShelfVisible());

  panel_browser->CloseWindow();
  browser()->CloseWindow();
}

// Verify that a new tabbed browser is created to display a download
// shelf when a download is started in a Panel and there is no existing
// tabbed browser.
IN_PROC_BROWSER_TEST_F(PanelDownloadTest, DownloadNoTabbedBrowser) {
  Profile* profile = browser()->profile();
  ASSERT_TRUE(CreateDownloadDirectory(profile));
  Browser* panel_browser = Browser::CreateForApp(Browser::TYPE_PANEL,
                                                 "PanelTest",
                                                 gfx::Rect(),
                                                 profile);
  EXPECT_EQ(2U, BrowserList::size());
  ASSERT_FALSE(browser()->window()->IsDownloadShelfVisible());
  ASSERT_FALSE(panel_browser->window()->IsDownloadShelfVisible());

  ui_test_utils::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      Source<Browser>(browser()));
  browser()->CloseWindow();
  signal.Wait();
  ASSERT_EQ(1U, BrowserList::size());
  ASSERT_EQ(NULL, Browser::GetTabbedBrowser(profile, false));

  scoped_ptr<DownloadObserver> observer(new DownloadObserver(profile));
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL download_url(URLRequestMockHTTPJob::GetMockUrl(file));
  ui_test_utils::NavigateToURLWithDisposition(
      panel_browser,
      download_url,
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  observer->WaitForDownload();

  EXPECT_EQ(2U, BrowserList::size());

#if defined(OS_CHROMEOS)
  // ChromeOS uses a download panel instead of a download shelf.
  std::set<Browser*> original_browsers;
  original_browsers.insert(panel_browser);
  Browser* added = ui_test_utils::GetBrowserNotInSet(original_browsers);
  ASSERT_TRUE(added->is_type_panel());
  ASSERT_FALSE(added->window()->IsDownloadShelfVisible());
#else
  Browser* tabbed_browser = Browser::GetTabbedBrowser(profile, false);
  EXPECT_EQ(1, tabbed_browser->tab_count());
  ASSERT_TRUE(tabbed_browser->window()->IsDownloadShelfVisible());
  tabbed_browser->CloseWindow();
#endif

  EXPECT_EQ(1, panel_browser->tab_count());
  ASSERT_FALSE(panel_browser->window()->IsDownloadShelfVisible());

  panel_browser->CloseWindow();
}
