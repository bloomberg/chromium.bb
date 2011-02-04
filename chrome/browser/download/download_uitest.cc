// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/browser/net/url_request_slow_download_job.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request_test_util.h"

#if defined(OS_WIN)
#include <shlwapi.h>
#endif

namespace {

const wchar_t kDocRoot[] = L"chrome/test/data";

class DownloadTest : public UITest {
 protected:
  DownloadTest() : UITest() {}

  void CheckDownload(const FilePath& client_filename,
                       const FilePath& server_filename) {
    // Find the path on the client.
    FilePath file_on_client = download_prefix_.Append(client_filename);

    // Find the path on the server.
    FilePath file_on_server;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA,
                                 &file_on_server));
    file_on_server = file_on_server.Append(server_filename);
    ASSERT_TRUE(file_util::PathExists(file_on_server));

    WaitForGeneratedFileAndCheck(file_on_client, file_on_server,
                                 true, true, false);

#if defined(OS_WIN)
    // Check if the Zone Identifier is correctly set.
    if (file_util::VolumeSupportsADS(file_on_client))
      EXPECT_TRUE(file_util::HasInternetZoneIdentifier(file_on_client));
#endif

    // Delete the client copy of the file.
    EXPECT_TRUE(file_util::Delete(file_on_client, false));
  }

  void CheckDownload(const FilePath& file) {
    CheckDownload(file, file);
  }

  void CleanupDownloadFiles(const FilePath& prefix) {
    FilePath path_prefix(download_prefix_);
    path_prefix = path_prefix.Append(prefix);
    download_util::EraseUniqueDownloadFiles(path_prefix);
  }

  virtual void SetUp() {
    UITest::SetUp();
    download_prefix_ = GetDownloadDirectory();
  }

 protected:
  void RunSizeTest(const GURL& url,
                   const string16& expected_title_in_progress,
                   const string16& expected_title_finished) {
    FilePath filename;
    net::FileURLToFilePath(url, &filename);
    filename = filename.BaseName();
    CleanupDownloadFiles(filename);

    {
      EXPECT_EQ(1, GetTabCount());

      NavigateToURL(url);
      // Downloads appear in the shelf
      WaitUntilTabCount(1);
      // TODO(tc): check download status text

      // Complete sending the request.  We do this by loading a second URL in a
      // separate tab.
      scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
      ASSERT_TRUE(window.get());
      EXPECT_TRUE(window->AppendTab(GURL(
          URLRequestSlowDownloadJob::kFinishDownloadUrl)));
      EXPECT_EQ(2, GetTabCount());
      // TODO(tc): check download status text

      // Make sure the download shelf is showing.
      EXPECT_TRUE(WaitForDownloadShelfVisible(window.get()));
    }

    FilePath download_path = download_prefix_.Append(filename);
    EXPECT_TRUE(file_util::PathExists(download_path));

    // Delete the file we just downloaded.
    EXPECT_TRUE(file_util::DieFileDie(download_path, true));
    EXPECT_FALSE(file_util::PathExists(download_path));
  }

  FilePath download_prefix_;
};

// Download a file with non-viewable content, verify that the
// download tab opened and the file exists.
// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, DISABLED_DownloadMimeType) {
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CleanupDownloadFiles(file);

  EXPECT_EQ(1, GetTabCount());

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(file));
  // No new tabs created, downloads appear in the current tab's download shelf.
  WaitUntilTabCount(1);

  CheckDownload(file);

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  EXPECT_TRUE(WaitForDownloadShelfVisible(browser.get()));
}

// Access a file with a viewable mime-type, verify that a download
// did not initiate.
// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, FLAKY_NoDownload) {
  FilePath file(FILE_PATH_LITERAL("download-test2.html"));
  FilePath file_path = download_prefix_.Append(file);
  CleanupDownloadFiles(file);

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(file));
  WaitUntilTabCount(1);

  // Wait to see if the file will be downloaded.
  base::PlatformThread::Sleep(TestTimeouts::action_timeout_ms());

  EXPECT_FALSE(file_util::PathExists(file_path));
  if (file_util::PathExists(file_path))
    ASSERT_TRUE(file_util::Delete(file_path, false));

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  EXPECT_FALSE(WaitForDownloadShelfVisible(browser.get()));
}

// Download a 0-size file with a content-disposition header, verify that the
// download tab opened and the file exists as the filename specified in the
// header.  This also ensures we properly handle empty file downloads.
// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, DISABLED_ContentDisposition) {
  FilePath file(FILE_PATH_LITERAL("download-test3.gif"));
  FilePath download_file(FILE_PATH_LITERAL("download-test3-attachment.gif"));
  CleanupDownloadFiles(file);

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(file));
  WaitUntilTabCount(1);

  CheckDownload(download_file, file);

  // Ensure the download shelf is visible on the window.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  EXPECT_TRUE(WaitForDownloadShelfVisible(browser.get()));
}

// Test that the download shelf is per-window by starting a download in one
// tab, opening a second tab, closing the shelf, going back to the first tab,
// and checking that the shelf is closed.
// See bug http://crbug.com/26325
// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, DISABLED_PerWindowShelf) {
  FilePath file(FILE_PATH_LITERAL("download-test3.gif"));
  FilePath download_file(FILE_PATH_LITERAL("download-test3-attachment.gif"));
  CleanupDownloadFiles(download_file);

  NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(file));
  WaitUntilTabCount(1);

  CheckDownload(download_file, file);

  // Ensure the download shelf is visible on the window.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  EXPECT_TRUE(WaitForDownloadShelfVisible(browser.get()));

  // Open a second tab
  ASSERT_TRUE(browser->AppendTab(GURL()));
  WaitUntilTabCount(2);

  // Hide shelf
  EXPECT_TRUE(browser->SetShelfVisible(false));
  EXPECT_TRUE(WaitForDownloadShelfInvisible(browser.get()));

  // Go to first tab
  EXPECT_TRUE(browser->ActivateTab(0));
  int tab_count;
  EXPECT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(2, tab_count);

  bool shelf_visible;
  EXPECT_TRUE(browser->IsShelfVisible(&shelf_visible));
  ASSERT_FALSE(shelf_visible);
}

// UnknownSize and KnownSize are tests which depend on
// URLRequestSlowDownloadJob to serve content in a certain way. Data will be
// sent in two chunks where the first chunk is 35K and the second chunk is 10K.
// The test will first attempt to download a file; but the server will "pause"
// in the middle until the server receives a second request for
// "download-finish.  At that time, the download will finish.
// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, FLAKY_UnknownSize) {
  GURL url(URLRequestSlowDownloadJob::kUnknownSizeUrl);
  FilePath filename;
  net::FileURLToFilePath(url, &filename);
  filename = filename.BaseName();
  RunSizeTest(url,
              ASCIIToUTF16("32.0 KB - ") + filename.LossyDisplayName(),
              ASCIIToUTF16("100% - ") + filename.LossyDisplayName());
}

// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, FLAKY_KnownSize) {
  GURL url(URLRequestSlowDownloadJob::kKnownSizeUrl);
  FilePath filename;
  net::FileURLToFilePath(url, &filename);
  filename = filename.BaseName();
  RunSizeTest(url,
              ASCIIToUTF16("71% - ") + filename.LossyDisplayName(),
              ASCIIToUTF16("100% - ") + filename.LossyDisplayName());
}

// Test that when downloading an item in Incognito mode, we don't crash when
// closing the last Incognito window (http://crbug.com/13983).
// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, DISABLED_IncognitoDownload) {
  // Open a regular window and sanity check default values for window / tab
  // count and shelf visibility.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  int window_count = 0;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  EXPECT_EQ(1, GetTabCount());
  bool is_shelf_visible;
  EXPECT_TRUE(browser->IsShelfVisible(&is_shelf_visible));
  EXPECT_FALSE(is_shelf_visible);

  // Open an Incognito window.
  ASSERT_TRUE(browser->RunCommand(IDC_NEW_INCOGNITO_WINDOW));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(2, window_count);
  scoped_refptr<BrowserProxy> incognito(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(incognito.get());
  // Wait for the new tab UI to load.
  int load_time;
  ASSERT_TRUE(automation()->WaitForInitialNewTabUILoad(&load_time));

  // Download something.
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CleanupDownloadFiles(file);
  scoped_refptr<TabProxy> tab(incognito->GetTab(0));
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(file)));

  // Verify that the download shelf is showing for the Incognito window.
  EXPECT_TRUE(WaitForDownloadShelfVisible(incognito.get()));

  // Close the Incognito window and don't crash.
  ASSERT_TRUE(incognito->RunCommand(IDC_CLOSE_WINDOW));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);

  // Verify that the regular window does not have a download shelf.
  EXPECT_TRUE(browser->IsShelfVisible(&is_shelf_visible));
  EXPECT_FALSE(is_shelf_visible);

  CheckDownload(file);
}

// All of the following tests are disabled, see http://crbug.com/43066
// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, FLAKY_DontCloseNewTab1) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  int window_count = 0;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  EXPECT_EQ(1, GetTabCount());

  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  FilePath file1(FILE_PATH_LITERAL("download-test2.html"));
  ASSERT_TRUE(tab_proxy->NavigateToURLAsyncWithDisposition(
      URLRequestMockHTTPJob::GetMockUrl(file1),
      NEW_BACKGROUND_TAB));
  // We should have two tabs now.
  WaitUntilTabCount(2);
}

// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, FLAKY_CloseNewTab1) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  int window_count = 0;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  EXPECT_EQ(1, GetTabCount());

  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CleanupDownloadFiles(file);
  ASSERT_TRUE(tab_proxy->NavigateToURLAsyncWithDisposition(
      URLRequestMockHTTPJob::GetMockUrl(file),
      NEW_BACKGROUND_TAB));
  // When the download starts, we should still have one tab.
  ASSERT_TRUE(WaitForDownloadShelfVisible(browser));
  EXPECT_EQ(1, GetTabCount());

  CheckDownload(file);
}

// Disabled, see http://crbug.com/43066
// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, FLAKY_DontCloseNewTab2) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  int window_count = 0;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  EXPECT_EQ(1, GetTabCount());

  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  ASSERT_TRUE(tab_proxy->NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
      FilePath(FILE_PATH_LITERAL("download_page1.html")))));

  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CleanupDownloadFiles(file);
  ASSERT_TRUE(tab_proxy->NavigateToURLAsync(GURL("javascript:openNew()")));

  ASSERT_TRUE(WaitForDownloadShelfVisible(browser));
  EXPECT_EQ(2, GetTabCount());

  CheckDownload(file);
}

// Flaky, see http://crbug.com/43066
// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, FLAKY_DontCloseNewTab3) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  int window_count = 0;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  EXPECT_EQ(1, GetTabCount());

  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  ASSERT_TRUE(tab_proxy->NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
      FilePath(FILE_PATH_LITERAL("download_page2.html")))));

  ASSERT_TRUE(tab_proxy->NavigateToURLAsync(GURL("javascript:openNew()")));

  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CleanupDownloadFiles(file);
  ASSERT_TRUE(tab_proxy->NavigateToURLAsync(
      URLRequestMockHTTPJob::GetMockUrl(file)));

  ASSERT_TRUE(WaitForDownloadShelfVisible(browser));
  EXPECT_EQ(2, GetTabCount());

  CheckDownload(file);
}

// Flaky, see http://crbug.com/43066
// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, FLAKY_CloseNewTab2) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  int window_count = 0;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  EXPECT_EQ(1, GetTabCount());

  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  ASSERT_TRUE(tab_proxy->NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
      FilePath(FILE_PATH_LITERAL("download_page3.html")))));

  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CleanupDownloadFiles(file);
  ASSERT_TRUE(tab_proxy->NavigateToURLAsync(GURL("javascript:openNew()")));

  ASSERT_TRUE(WaitForDownloadShelfVisible(browser));
  EXPECT_EQ(1, GetTabCount());

  CheckDownload(file);
}

// Flaky, see http://crbug.com/43066
// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, FLAKY_CloseNewTab3) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  int window_count = 0;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  EXPECT_EQ(1, GetTabCount());

  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  ASSERT_TRUE(tab_proxy->NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(
      FilePath(FILE_PATH_LITERAL("download_page4.html")))));

  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CleanupDownloadFiles(file);
  ASSERT_TRUE(tab_proxy->NavigateToURLAsync(
      GURL("javascript:document.getElementById('form').submit()")));

  ASSERT_TRUE(WaitForDownloadShelfVisible(browser));
  EXPECT_EQ(1, GetTabCount());

  CheckDownload(file);
}

// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, DISABLED_DontCloseNewWindow) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  int window_count = 0;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  EXPECT_EQ(1, GetTabCount());

  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CleanupDownloadFiles(file);
  ASSERT_TRUE(tab_proxy->NavigateToURLAsyncWithDisposition(
      URLRequestMockHTTPJob::GetMockUrl(file), NEW_WINDOW));

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2));

  CheckDownload(file);
}

// Regression test for http://crbug.com/44454
// All download tests are flaky on all platforms, http://crbug.com/35275,
// http://crbug.com/48913.
// Additionally, there is Windows-specific flake, http://crbug.com/20809.
TEST_F(DownloadTest, DISABLED_NewWindow) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  int window_count = 0;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  ASSERT_EQ(1, window_count);
  EXPECT_EQ(1, GetTabCount());

  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  CleanupDownloadFiles(file);
  ASSERT_TRUE(tab_proxy->NavigateToURLAsyncWithDisposition(
    URLRequestMockHTTPJob::GetMockUrl(file), NEW_WINDOW));

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2));

  CheckDownload(file);
}

}  // namespace
