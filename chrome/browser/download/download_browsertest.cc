// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/test/test_file_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/browser/net/url_request_slow_download_job.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Variation of DownloadsCompleteObserver from ui_test_utils.cc; the
// specifically targeted download tests need finer granularity on waiting.
// Construction of this class defines a system state, based on some number
// of downloads being seen in a particular state + other events that
// may occur in the download system.  That state will be recorded if it
// occurs at any point after construction.  When that state occurs, the class
// is considered finished.  Callers may either probe for the finished state, or
// wait on it.
class DownloadsObserver : public DownloadManager::Observer,
                          public DownloadItem::Observer {
 public:
  // The action which should be considered the completion of the download.
  enum DownloadFinishedSignal { COMPLETE, FILE_RENAME };

  // Create an object that will be considered completed when |wait_count|
  // download items have entered state |download_finished_signal|.
  // If |finish_on_select_file| is true, the object will also be
  // considered finished if the DownloadManager raises a
  // SelectFileDialogDisplayed() notification.

  // TODO(rdsmith): Add option of "dangerous accept/reject dialog" as
  // a unblocking event; if that shows up when you aren't expecting it,
  // it'll result in a hang/timeout as we'll never get to final rename.
  // This probably means rewriting the interface to take a list of events
  // to treat as completion events.
  DownloadsObserver(DownloadManager* download_manager,
                    size_t wait_count,
                    DownloadFinishedSignal download_finished_signal,
                    bool finish_on_select_file)
      : download_manager_(download_manager),
        wait_count_(wait_count),
        finished_downloads_at_construction_(0),
        waiting_(false),
        download_finished_signal_(download_finished_signal),
        finish_on_select_file_(finish_on_select_file),
        select_file_dialog_seen_(false) {
    download_manager_->AddObserver(this);  // Will call initial ModelChanged().
    finished_downloads_at_construction_ = finished_downloads_.size();
  }

  ~DownloadsObserver() {
    std::set<DownloadItem*>::iterator it = downloads_observed_.begin();
    for (; it != downloads_observed_.end(); ++it) {
      (*it)->RemoveObserver(this);
    }
    download_manager_->RemoveObserver(this);
  }

  // State accessors.
  bool select_file_dialog_seen() { return select_file_dialog_seen_; }

  // Wait for whatever state was specified in the constructor.
  void WaitForFinished() {
    if (!IsFinished()) {
      waiting_ = true;
      ui_test_utils::RunMessageLoop();
      waiting_ = false;
    }
  }

  // Return true if everything's happened that we're configured for.
  bool IsFinished() {
    if (finished_downloads_.size() - finished_downloads_at_construction_
        >= wait_count_)
      return true;
    return (finish_on_select_file_ && select_file_dialog_seen_);
  }

  // DownloadItem::Observer
  virtual void OnDownloadUpdated(DownloadItem* download) {
    if (download_finished_signal_ == COMPLETE &&
        download->state() == DownloadItem::COMPLETE)
      DownloadInFinalState(download);
  }

  virtual void OnDownloadFileCompleted(DownloadItem* download) {
    if (download_finished_signal_ == FILE_RENAME)
      DownloadInFinalState(download);
  }

  virtual void OnDownloadOpened(DownloadItem* download) {}

  // DownloadManager::Observer
  virtual void ModelChanged() {
    // Regenerate DownloadItem observers.  If there are any download items
    // in the COMPLETE state and that's our final state, note them in
    // finished_downloads_ (done by OnDownloadUpdated).
    std::vector<DownloadItem*> downloads;
    download_manager_->SearchDownloads(string16(), &downloads);

    std::vector<DownloadItem*>::iterator it = downloads.begin();
    for (; it != downloads.end(); ++it) {
      OnDownloadUpdated(*it);  // Safe to call multiple times; checks state.

      std::set<DownloadItem*>::const_iterator
          finished_it(finished_downloads_.find(*it));
      std::set<DownloadItem*>::iterator
          observed_it(downloads_observed_.find(*it));

      // If it isn't finished and we're aren't observing it, start.
      if (finished_it == finished_downloads_.end() &&
          observed_it == downloads_observed_.end()) {
        (*it)->AddObserver(this);
        downloads_observed_.insert(*it);
        continue;
      }

      // If it is finished and we are observing it, stop.
      if (finished_it != finished_downloads_.end() &&
          observed_it != downloads_observed_.end()) {
        (*it)->RemoveObserver(this);
        downloads_observed_.erase(observed_it);
        continue;
      }
    }
  }

  virtual void SelectFileDialogDisplayed() {
    select_file_dialog_seen_ = true;
    SignalIfFinished();
  }

 private:
  // Called when we know that a download item is in a final state.
  // Note that this is not the same as it first transitioning in to the
  // final state; in the case of DownloadItem::COMPLETE, multiple
  // notifications may occur once the item is in that state.  So we
  // keep our own track of transitions into final.
  void DownloadInFinalState(DownloadItem* download) {
    if (finished_downloads_.find(download) != finished_downloads_.end()) {
      // We've already seen terminal state on this download.
      return;
    }

    // Record the transition.
    finished_downloads_.insert(download);

    SignalIfFinished();
  }

  void SignalIfFinished() {
    if (waiting_ && IsFinished())
      MessageLoopForUI::current()->Quit();
  }

  // The observed download manager.
  DownloadManager* download_manager_;

  // The set of DownloadItem's that have transitioned to their finished state
  // since construction of this object.  When the size of this array
  // reaches wait_count_, we're done.
  std::set<DownloadItem*> finished_downloads_;

  // The set of DownloadItem's we are currently observing.  Generally there
  // won't be any overlap with the above; once we see the final state
  // on a DownloadItem, we'll stop observing it.
  std::set<DownloadItem*> downloads_observed_;

  // The number of downloads to wait on completing.
  size_t wait_count_;

  // The number of downloads entered in final state in initial
  // ModelChanged().  We use |finished_downloads_| to track the incoming
  // transitions to final state we should ignore, and to track the
  // number of final state transitions that occurred between
  // construction and return from wait.  But if the final state is the
  // COMPLETE state, some downloads may be in it (and thus be entered
  // into finished_downloads_) when we construct this class.  We don't
  // want to count those;
  int finished_downloads_at_construction_;

  // Whether an internal message loop has been started and must be quit upon
  // all downloads completing.
  bool waiting_;

  // The action on which to consider the DownloadItem finished.
  DownloadFinishedSignal download_finished_signal_;

  // True if we should transition the DownloadsObserver to finished if
  // the select file dialog comes up.
  bool finish_on_select_file_;

  // True if we've seen the select file dialog.
  bool select_file_dialog_seen_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsObserver);
};

class DownloadTest : public InProcessBrowserTest {
 public:
  enum SelectExpectation {
    EXPECT_NO_SELECT_DIALOG = -1,
    EXPECT_NOTHING,
    EXPECT_SELECT_DIALOG
  };

  // Returning false indicates a failure of the setup, and should be asserted
  // in the caller.
  virtual bool InitialSetup(bool prompt_for_download) {
    bool have_test_dir = PathService::Get(chrome::DIR_TEST_DATA, &test_dir_);
    EXPECT_TRUE(have_test_dir);
    if (!have_test_dir)
      return false;

    // Sanity check default values for window / tab count and shelf visibility.
    int window_count = BrowserList::size();
    EXPECT_EQ(1, window_count);
    EXPECT_EQ(1, browser()->tab_count());
    bool is_shelf_visible = browser()->window()->IsDownloadShelfVisible();
    EXPECT_FALSE(is_shelf_visible);

    // Set up the temporary download folder.
    bool created_downloads_dir = CreateAndSetDownloadsDirectory(browser());
    EXPECT_TRUE(created_downloads_dir);
    if (!created_downloads_dir)
      return false;
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                                 prompt_for_download);

    return true;
  }

 protected:

  // Must be called after browser creation.  Creates a temporary
  // directory for downloads that is auto-deleted on destruction.
  // Returning false indicates a failure of the function, and should be asserted
  // in the caller.
  bool CreateAndSetDownloadsDirectory(Browser* browser) {
    if (!browser)
      return false;

    if (!downloads_directory_.CreateUniqueTempDir())
      return false;

    browser->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory,
        downloads_directory_.path());

    return true;
  }

  FilePath GetDownloadDirectory(Browser* browser) {
    DownloadManager* download_mananger =
        browser->profile()->GetDownloadManager();
    return download_mananger->download_prefs()->download_path();
  }

  DownloadsObserver* CreateWaiter(Browser* browser, int num_downloads) {
    DownloadManager* download_manager =
        browser->profile()->GetDownloadManager();
    return new DownloadsObserver(
        download_manager, num_downloads,
        DownloadsObserver::FILE_RENAME,  // Really done
        true);                           // Bail on select file
  }

  // Download |url|, then wait for the download to finish.
  // |disposition| indicates where the navigation occurs (current tab, new
  // foreground tab, etc).
  // |expectation| indicates whether or not a Select File dialog should be
  // open when the download is finished, or if we don't care.
  // If the dialog appears, the test completes.  The only effect |expectation|
  // has is whether or not the test succeeds.
  // |browser_test_flags| indicate what to wait for, and is an OR of 0 or more
  // values in the ui_test_utils::BrowserTestWaitFlags enum.
  void DownloadAndWaitWithDisposition(Browser* browser,
                                      const GURL& url,
                                      WindowOpenDisposition disposition,
                                      SelectExpectation expectation,
                                      int browser_test_flags) {
    // Setup notification, navigate, and block.
    scoped_ptr<DownloadsObserver> observer(CreateWaiter(browser, 1));
    // This call will block until the condition specified by
    // |browser_test_flags|, but will not wait for the download to finish.
    ui_test_utils::NavigateToURLWithDisposition(browser,
                                                url,
                                                disposition,
                                                browser_test_flags);
    // Waits for the download to complete.
    observer->WaitForFinished();

    // If specified, check the state of the select file dialog.
    if (expectation != EXPECT_NOTHING) {
      EXPECT_EQ(expectation == EXPECT_SELECT_DIALOG,
                observer->select_file_dialog_seen());
    }
  }

  // Download a file in the current tab, then wait for the download to finish.
  void DownloadAndWait(Browser* browser,
                       const GURL& url,
                       SelectExpectation expectation) {
    DownloadAndWaitWithDisposition(
        browser,
        url,
        CURRENT_TAB,
        expectation,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  }

  // Should only be called when the download is known to have finished
  // (in error or not).
  // Returning false indicates a failure of the function, and should be asserted
  // in the caller.
  bool CheckDownload(Browser* browser,
                     const FilePath& downloaded_filename,
                     const FilePath& origin_filename) {
    // Find the path to which the data will be downloaded.
    FilePath downloaded_file =
        GetDownloadDirectory(browser).Append(downloaded_filename);

    // Find the origin path (from which the data comes).
    FilePath origin_file(test_dir_.Append(origin_filename));
    bool origin_file_exists = file_util::PathExists(origin_file);
    EXPECT_TRUE(origin_file_exists);
    if (!origin_file_exists)
      return false;

    // Confirm the downloaded data file exists.
    bool downloaded_file_exists = file_util::PathExists(downloaded_file);
    EXPECT_TRUE(downloaded_file_exists);
    if (!downloaded_file_exists)
      return false;

    int64 origin_file_size = 0;
    int64 downloaded_file_size = 0;
    EXPECT_TRUE(file_util::GetFileSize(origin_file, &origin_file_size));
    EXPECT_TRUE(file_util::GetFileSize(downloaded_file, &downloaded_file_size));
    EXPECT_EQ(origin_file_size, downloaded_file_size);
    EXPECT_TRUE(file_util::ContentsEqual(downloaded_file, origin_file));

    // Delete the downloaded copy of the file.
    bool downloaded_file_deleted =
        file_util::DieFileDie(downloaded_file, false);
    EXPECT_TRUE(downloaded_file_deleted);
    return downloaded_file_deleted;
  }

  // TODO(ahendrickson) -- |expected_title_in_progress| and
  // |expected_title_finished| need to be checked.
  bool RunSizeTest(Browser* browser,
                   const GURL& url,
                   const string16& expected_title_in_progress,
                   const string16& expected_title_finished) {
    if (!InitialSetup(false))
      return false;

    // Download a partial web page in a background tab and wait.
    // The mock system will not complete until it gets a special URL.
    scoped_ptr<DownloadsObserver> observer(CreateWaiter(browser, 1));
    ui_test_utils::NavigateToURL(browser, url);

    // TODO(ahendrickson): check download status text before downloading.
    // Need to:
    //  - Add a member function to the |DownloadShelf| interface class, that
    //    indicates how many members it has.
    //  - Add a member function to |DownloadShelf| to get the status text
    //    of a given member (for example, via |DownloadItemView|'s
    //    GetAccessibleName() member function), by index.
    //  - Iterate over browser->window()->GetDownloadShelf()'s members
    //    to see if any match the status text we want.  Start with the last one.

    // Complete sending the request.  We do this by loading a second URL in a
    // separate tab.
    GURL finish_url(URLRequestSlowDownloadJob::kFinishDownloadUrl);
    ui_test_utils::NavigateToURLWithDisposition(
        browser,
        finish_url,
        NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    observer->WaitForFinished();

    EXPECT_EQ(2, browser->tab_count());

    // TODO(ahendrickson): check download status text after downloading.

    // Make sure the download shelf is showing.
    EXPECT_TRUE(browser->window()->IsDownloadShelfVisible());

    FilePath filename;
    net::FileURLToFilePath(url, &filename);
    filename = filename.BaseName();
    FilePath download_path = downloads_directory_.path().Append(filename);

    bool downloaded_path_exists = file_util::PathExists(download_path);
    EXPECT_TRUE(downloaded_path_exists);
    if (!downloaded_path_exists)
      return false;

    // Delete the file we just downloaded.
    EXPECT_TRUE(file_util::DieFileDie(download_path, true));
    EXPECT_FALSE(file_util::PathExists(download_path));

    return true;
  }

 private:
  // Location of the test data.
  FilePath test_dir_;

  // Location of the downloads directory for these tests
  ScopedTempDir downloads_directory_;
};

// NOTES:
//
// Files for these tests are found in DIR_TEST_DATA (currently
// "chrome\test\data\", see chrome_paths.cc).
// Mock responses have extension .mock-http-headers appended to the file name.

// Download a file due to the associated MIME type.
//
// Test is believed good (non-flaky) in itself, but it
// sometimes trips over underlying flakiness in the downloads
// subsystem in in http://crbug.com/63237.  Until that bug is
// fixed, this test should be considered flaky.  It's entered as
// DISABLED since if 63237 does cause a failure, it'll be a timeout.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_DownloadMimeType) {
  ASSERT_TRUE(InitialSetup(false));
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));

  // Download the file and wait.  We do not expect the Select File dialog.
  DownloadAndWait(browser(), url, EXPECT_NO_SELECT_DIALOG);

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  CheckDownload(browser(), file, file);
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
}

#if defined(OS_WIN)
// Download a file and confirm that the zone identifier (on windows)
// is set to internet.
// This is flaky due to http://crbug.com/20809.  It's marked as
// DISABLED because when the test fails it fails via a timeout.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_CheckInternetZone) {
  ASSERT_TRUE(InitialSetup(false));
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));

  // Download the file and wait.  We do not expect the Select File dialog.
  DownloadAndWait(browser(), url, EXPECT_NO_SELECT_DIALOG);

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  CheckDownload(browser(), file, file);
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  FilePath downloaded_file = GetDownloadDirectory(browser()).Append(file);
  if (file_util::VolumeSupportsADS(downloaded_file))
    EXPECT_TRUE(file_util::HasInternetZoneIdentifier(downloaded_file));
}
#endif

// Put up a Select File dialog when the file is downloaded, due to its MIME
// type.
//
// This test runs correctly, but leaves behind turds in the test user's
// download directory because of http://crbug.com/62099.  No big loss; it
// was primarily confirming DownloadsObserver wait on select file dialog
// functionality anyway.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_DownloadMimeTypeSelect) {
  ASSERT_TRUE(InitialSetup(true));
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));

  // Download the file and wait.  We expect the Select File dialog to appear
  // due to the MIME type.
  DownloadAndWait(browser(), url, EXPECT_SELECT_DIALOG);

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  // Since we exited while the Select File dialog was visible, there should not
  // be anything in the download shelf and so it should not be visible.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());
}

// Access a file with a viewable mime-type, verify that a download
// did not initiate.
IN_PROC_BROWSER_TEST_F(DownloadTest, NoDownload) {
  ASSERT_TRUE(InitialSetup(false));
  FilePath file(FILE_PATH_LITERAL("download-test2.html"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  FilePath file_path = GetDownloadDirectory(browser()).Append(file);

  // Open a web page and wait.
  ui_test_utils::NavigateToURL(browser(), url);

  // Check that we did not download the web page.
  EXPECT_FALSE(file_util::PathExists(file_path));

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());
}

// Download a 0-size file with a content-disposition header, verify that the
// download tab opened and the file exists as the filename specified in the
// header.  This also ensures we properly handle empty file downloads.
// The download shelf should be visible in the current tab.
//
// Test is believed mostly good (non-flaky) in itself, but it
// sometimes trips over underlying flakiness in the downloads
// subsystem in in http://crbug.com/63237.  Until that bug is
// fixed, this test should be considered flaky.  It's entered as
// DISABLED since if 63237 does cause a failure, it'll be a timeout.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_ContentDisposition) {
  ASSERT_TRUE(InitialSetup(false));
  FilePath file(FILE_PATH_LITERAL("download-test3.gif"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  FilePath download_file(FILE_PATH_LITERAL("download-test3-attachment.gif"));

  // Download a file and wait.
  DownloadAndWait(browser(), url, EXPECT_NO_SELECT_DIALOG);

  CheckDownload(browser(), download_file, file);

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
}

// Test that the download shelf is per-window by starting a download in one
// tab, opening a second tab, closing the shelf, going back to the first tab,
// and checking that the shelf is closed.
//
// The test sometimes trips over underlying flakiness in the downloads
// subsystem in in http://crbug.com/63237.  It's entered as
// DISABLED since if 63237 does cause a failure, it'll be a timeout.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_PerWindowShelf) {
  ASSERT_TRUE(InitialSetup(false));
  FilePath file(FILE_PATH_LITERAL("download-test3.gif"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  FilePath download_file(FILE_PATH_LITERAL("download-test3-attachment.gif"));

  // Download a file and wait.
  DownloadAndWait(browser(), url, EXPECT_NO_SELECT_DIALOG);

  CheckDownload(browser(), download_file, file);

  // Check state.
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // Open a second tab and wait.
  EXPECT_NE(static_cast<TabContentsWrapper*>(NULL),
            browser()->AddSelectedTabWithURL(GURL(), PageTransition::TYPED));
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // Hide the download shelf.
  browser()->window()->GetDownloadShelf()->Close();
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  // Go to the first tab.
  browser()->SelectTabContentsAt(0, true);
  EXPECT_EQ(2, browser()->tab_count());

  // The download shelf should not be visible.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());
}

// UnknownSize and KnownSize are tests which depend on
// URLRequestSlowDownloadJob to serve content in a certain way. Data will be
// sent in two chunks where the first chunk is 35K and the second chunk is 10K.
// The test will first attempt to download a file; but the server will "pause"
// in the middle until the server receives a second request for
// "download-finish".  At that time, the download will finish.
// These tests don't currently test much due to holes in |RunSizeTest()|.  See
// comments in that routine for details.

// Test is believed mostly good (non-flaky) in itself, but it
// very occasionally trips over underlying flakiness in the downloads
// subsystem in in http://crbug.com/63237.  Until that bug is
// fixed, this test should be considered flaky.  It's entered as
// DISABLED since if 63237 does cause a failure, it'll be a timeout.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_UnknownSize) {
  GURL url(URLRequestSlowDownloadJob::kUnknownSizeUrl);
  FilePath filename;
  net::FileURLToFilePath(url, &filename);
  filename = filename.BaseName();
  ASSERT_TRUE(RunSizeTest(
                  browser(),
                  url,
                  ASCIIToUTF16("32.0 KB - ") + filename.LossyDisplayName(),
                  ASCIIToUTF16("100% - ") + filename.LossyDisplayName()));
}

// Test is believed mostly good (non-flaky) in itself, but it
// very occasionally trips over underlying flakiness in the downloads
// subsystem in in http://crbug.com/63237.  Until that bug is
// fixed, this test should be considered flaky.  It's entered as
// DISABLED since if 63237 does cause a failure, it'll be a timeout.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_KnownSize) {
  GURL url(URLRequestSlowDownloadJob::kKnownSizeUrl);
  FilePath filename;
  net::FileURLToFilePath(url, &filename);
  filename = filename.BaseName();
  ASSERT_TRUE(RunSizeTest(
                  browser(),
                  url,
                  ASCIIToUTF16("71% - ") + filename.LossyDisplayName(),
                  ASCIIToUTF16("100% - ") + filename.LossyDisplayName()));
}

// Test that when downloading an item in Incognito mode, we don't crash when
// closing the last Incognito window (http://crbug.com/13983).
// Also check that the download shelf is not visible after closing the
// Incognito window.
//
// Test is believed mostly good (non-flaky) in itself, but it
// sometimes trips over underlying flakiness in the downloads
// subsystem in in http://crbug.com/63237.  Until that bug is
// fixed, this test should be considered flaky.  It's entered as
// DISABLED since if 63237 does cause a failure, it'll be a timeout.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_IncognitoDownload) {
  ASSERT_TRUE(InitialSetup(false));

  // Open an Incognito window.
  Browser* incognito = CreateIncognitoBrowser();  // Waits.
  ASSERT_TRUE(incognito);
  int window_count = BrowserList::size();
  EXPECT_EQ(2, window_count);

  // Download a file in the Incognito window and wait.
  CreateAndSetDownloadsDirectory(incognito);
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  // Since |incognito| is a separate browser, we have to set it up explicitly.
  incognito->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                               false);
  DownloadAndWait(incognito, url, EXPECT_NO_SELECT_DIALOG);

  // We should still have 2 windows.
  window_count = BrowserList::size();
  EXPECT_EQ(2, window_count);

  // Verify that the download shelf is showing for the Incognito window.
  bool is_shelf_visible = incognito->window()->IsDownloadShelfVisible();
  EXPECT_TRUE(is_shelf_visible);

  // Close the Incognito window and don't crash.
  incognito->CloseWindow();
  ui_test_utils::WaitForNotificationFrom(NotificationType::BROWSER_CLOSED,
                                         Source<Browser>(incognito));

  window_count = BrowserList::size();
  EXPECT_EQ(1, window_count);

  // Verify that the regular window does not have a download shelf.
  is_shelf_visible = browser()->window()->IsDownloadShelfVisible();
  EXPECT_FALSE(is_shelf_visible);

  CheckDownload(browser(), file, file);
}

// Navigate to a new background page, but don't download.  Confirm that the
// download shelf is not visible and that we have two tabs.
IN_PROC_BROWSER_TEST_F(DownloadTest, DontCloseNewTab1) {
  ASSERT_TRUE(InitialSetup(false));
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  FilePath file1(FILE_PATH_LITERAL("download-test2.html"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file1));

  // Open a web page and wait.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      url,
      NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // We should have two tabs now.
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());
}

// Download a file in a background tab. Verify that the tab is closed
// automatically, and that the download shelf is visible in the current tab.
//
// The test sometimes trips over underlying flakiness in the downloads
// subsystem in http://crbug.com/63237.  It's entered as
// DISABLED since if 63237 does cause a failure, it'll be a timeout.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_CloseNewTab1) {
  ASSERT_TRUE(InitialSetup(false));

  // Download a file in a new background tab and wait.  The tab is automatically
  // closed when the download begins.
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  DownloadAndWaitWithDisposition(
      browser(),
      url,
      NEW_BACKGROUND_TAB,
      EXPECT_NO_SELECT_DIALOG,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // When the download finishes, we should still have one tab.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  EXPECT_EQ(1, browser()->tab_count());

  CheckDownload(browser(), file, file);
}

// Open a web page in the current tab, then download a file in another tab via
// a Javascript call.
// Verify that we have 2 tabs, and the download shelf is visible in the current
// tab.
//
// The download_page1.html page contains an openNew() function that opens a
// tab and then downloads download-test1.lib.
//
// The test sometimes trips over underlying flakiness in the downloads
// subsystem in in http://crbug.com/63237.  It's entered as
// DISABLED since if 63237 does cause a failure, it'll be a timeout.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_DontCloseNewTab2) {
  ASSERT_TRUE(InitialSetup(false));
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  FilePath file1(FILE_PATH_LITERAL("download_page1.html"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file1));

  // Open a web page and wait.
  ui_test_utils::NavigateToURL(browser(), url);

  // Download a file in a new tab and wait (via Javascript).
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndWaitWithDisposition(browser(),
                                 GURL("javascript:openNew()"),
                                 CURRENT_TAB,
                                 EXPECT_NO_SELECT_DIALOG,
                                 ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // When the download finishes, we should have two tabs.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  EXPECT_EQ(2, browser()->tab_count());

  CheckDownload(browser(), file, file);
}

// Open a web page in the current tab, open another tab via a Javascript call,
// then download a file in the new tab.
// Verify that we have 2 tabs, and the download shelf is visible in the current
// tab.
//
// The download_page2.html page contains an openNew() function that opens a
// tab.
//
// The test sometimes trips over underlying flakiness in the downloads
// subsystem in in http://crbug.com/63237.  It's entered as
// DISABLED since if 63237 does cause a failure, it'll be a timeout.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_DontCloseNewTab3) {
  ASSERT_TRUE(InitialSetup(false));
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  FilePath file1(FILE_PATH_LITERAL("download_page2.html"));
  GURL url1(URLRequestMockHTTPJob::GetMockUrl(file1));

  // Open a web page and wait.
  ui_test_utils::NavigateToURL(browser(), url1);

  // Open a new tab and wait.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("javascript:openNew()"),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  EXPECT_EQ(2, browser()->tab_count());

  // Download a file and wait.
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  DownloadAndWaitWithDisposition(browser(),
                                 url,
                                 CURRENT_TAB,
                                 EXPECT_NO_SELECT_DIALOG,
                                 ui_test_utils::BROWSER_TEST_NONE);

  // When the download finishes, we should have two tabs.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  EXPECT_EQ(2, browser()->tab_count());

  CheckDownload(browser(), file, file);
}

// Open a web page in the current tab, then download a file via Javascript,
// which will do so in a temporary tab.
// Verify that we have 1 tab, and the download shelf is visible.
//
// The download_page3.html page contains an openNew() function that opens a
// tab with download-test1.lib in the URL.  When the URL is determined to be
// a download, the tab is closed automatically.
//
// The test sometimes trips over underlying flakiness in the downloads
// subsystem in in http://crbug.com/63237.  It's entered as
// DISABLED since if 63237 does cause a failure, it'll be a timeout.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_CloseNewTab2) {
  ASSERT_TRUE(InitialSetup(false));
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  FilePath file1(FILE_PATH_LITERAL("download_page3.html"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file1));

  // Open a web page and wait.
  ui_test_utils::NavigateToURL(browser(), url);

  // Download a file and wait.
  // The file to download is "download-test1.lib".
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndWaitWithDisposition(browser(),
                                 GURL("javascript:openNew()"),
                                 CURRENT_TAB,
                                 EXPECT_NO_SELECT_DIALOG,
                                 ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // When the download finishes, we should still have one tab.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  EXPECT_EQ(1, browser()->tab_count());

  CheckDownload(browser(), file, file);
}

// Open a web page in the current tab, then call Javascript via a button to
// download a file in a new tab, which is closed automatically when the
// download begins.
// Verify that we have 1 tab, and the download shelf is visible.
//
// The download_page4.html page contains a form with download-test1.lib as the
// action.
//
// The test sometimes trips over underlying flakiness in the downloads
// subsystem in in http://crbug.com/63237.  It's entered as
// DISABLED since if 63237 does cause a failure, it'll be a timeout.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_CloseNewTab3) {
  ASSERT_TRUE(InitialSetup(false));
  // Because it's an HTML link, it should open a web page rather than
  // downloading.
  FilePath file1(FILE_PATH_LITERAL("download_page4.html"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file1));

  // Open a web page and wait.
  ui_test_utils::NavigateToURL(browser(), url);

  // Download a file in a new tab and wait.  The tab will automatically close
  // when the download begins.
  // The file to download is "download-test1.lib".
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndWaitWithDisposition(
      browser(),
      GURL("javascript:document.getElementById('form').submit()"),
      CURRENT_TAB,
      EXPECT_NO_SELECT_DIALOG,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // When the download finishes, we should still have one tab.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  EXPECT_EQ(1, browser()->tab_count());

  CheckDownload(browser(), file, file);
}

// Download a file in a new window.
// Verify that we have 2 windows, and the download shelf is not visible in the
// first window, but is visible in the second window.
// Close the new window.
// Verify that we have 1 window, and the download shelf is not visible.
//
// Regression test for http://crbug.com/44454
//
// Test is believed mostly good (non-flaky) in itself, but it
// sometimes trips over underlying flakiness in the downloads
// subsystem in in http://crbug.com/63237.  Until that bug is
// fixed, this test should be considered flaky.  It's entered as
// DISABLED since if 63237 does cause a failure, it'll be a timeout.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_NewWindow) {
  ASSERT_TRUE(InitialSetup(false));
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  Browser* first_browser = browser();

  // Download a file in a new window and wait.
  DownloadAndWaitWithDisposition(browser(),
                                 url,
                                 NEW_WINDOW,
                                 EXPECT_NO_SELECT_DIALOG,
                                 ui_test_utils::BROWSER_TEST_NONE);

  // When the download finishes, the download shelf SHOULD NOT be visible in
  // the first window.
  int window_count = BrowserList::size();
  EXPECT_EQ(2, window_count);
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  // The download shelf SHOULD be visible in the second window.
  std::set<Browser*> original_browsers;
  original_browsers.insert(browser());
  Browser* download_browser =
      ui_test_utils::GetBrowserNotInSet(original_browsers);
  ASSERT_TRUE(download_browser != NULL);
  EXPECT_NE(download_browser, browser());
  EXPECT_EQ(1, download_browser->tab_count());
  EXPECT_TRUE(download_browser->window()->IsDownloadShelfVisible());

  // Close the new window.
  download_browser->CloseWindow();
  ui_test_utils::WaitForNotificationFrom(NotificationType::BROWSER_CLOSED,
                                         Source<Browser>(download_browser));
  EXPECT_EQ(first_browser, browser());
  window_count = BrowserList::size();
  EXPECT_EQ(1, window_count);
  EXPECT_EQ(1, browser()->tab_count());
  // The download shelf should not be visible in the remaining window.
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  CheckDownload(browser(), file, file);
}

}  // namespace
