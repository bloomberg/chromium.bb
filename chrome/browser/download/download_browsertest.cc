// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
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
      std::set<DownloadItem*>::const_iterator
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
 protected:
  void SetUpInProcessBrowserTestFixture() {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir_));
  }

  // Must be called after browser creation.  Creates a temporary
  // directory for downloads that is auto-deleted on destruction.
  bool CreateAndSetDownloadsDirectory() {
    if (downloads_directory_.CreateUniqueTempDir()) {
      browser()->profile()->GetPrefs()->SetFilePath(
          prefs::kDownloadDefaultDirectory,
          downloads_directory_.path());
      return true;
    }
    return false;
  }

  // May only be called inside of an individual test; browser() is NULL
  // outside of that context.
  FilePath GetDownloadDirectory() {
    DownloadManager* download_mananger =
        browser()->profile()->GetDownloadManager();
    return download_mananger->download_prefs()->download_path();
  }

  DownloadsObserver* CreateWaiter(int num_downloads) {
    DownloadManager* download_manager =
        browser()->profile()->GetDownloadManager();
    return new DownloadsObserver(
        download_manager, num_downloads,
        DownloadsObserver::FILE_RENAME,  // Really done
        true);                          // Bail on select file
  }

  // Should only be called when the download is known to have finished
  // (in error or not).
  void CheckDownload(const FilePath& downloaded_filename,
                     const FilePath& origin_filename) {
    // Find the path to which the data will be downloaded.
    FilePath downloaded_file =
        GetDownloadDirectory().Append(downloaded_filename);

    // Find the origin path (from which the data comes).
    FilePath origin_file(test_dir_.Append(origin_filename));
    ASSERT_TRUE(file_util::PathExists(origin_file));

    // Confirm the downloaded data file exists.
    ASSERT_TRUE(file_util::PathExists(downloaded_file));
    int64 origin_file_size = 0;
    int64 downloaded_file_size = 0;
    EXPECT_TRUE(file_util::GetFileSize(origin_file, &origin_file_size));
    EXPECT_TRUE(file_util::GetFileSize(downloaded_file, &downloaded_file_size));
    EXPECT_EQ(origin_file_size, downloaded_file_size);
    EXPECT_TRUE(file_util::ContentsEqual(downloaded_file, origin_file));

#if defined(OS_WIN)
    // Check if the Zone Identifier is correctly set.
    if (file_util::VolumeSupportsADS(downloaded_file))
      EXPECT_TRUE(file_util::HasInternetZoneIdentifier(downloaded_file));
#endif

    // Delete the downloaded copy of the file.
    EXPECT_TRUE(file_util::DieFileDie(downloaded_file, false));
  }

 private:
  // Location of the test data.
  FilePath test_dir_;

  // Location of the downloads directory for these tests
  ScopedTempDir downloads_directory_;
};

// Test is believed good (non-flaky) in itself, but it
// sometimes trips over underlying flakiness in the downloads
// subsystem in in http://crbug.com/63237.  Until that bug is
// fixed, this test should be considered flaky.  It's entered as
// DISABLED since if 63237 does cause a failure, it'll be a timeout.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_DownloadMimeType) {
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  ASSERT_TRUE(CreateAndSetDownloadsDirectory());

  EXPECT_EQ(1, browser()->tab_count());

  // Setup notification, navigate, and block.
  scoped_ptr<DownloadsObserver> observer(CreateWaiter(1));
  ui_test_utils::NavigateToURL(
      browser(), URLRequestMockHTTPJob::GetMockUrl(file));
  observer->WaitForFinished();

  // Download should be finished; check state.
  EXPECT_FALSE(observer->select_file_dialog_seen());
  EXPECT_EQ(1, browser()->tab_count());
  CheckDownload(file, file);
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
}

// This test runs correctly, but leaves behind turds in the test user's
// download directory because of http://crbug.com/62099.  No big loss; it
// was primarily confirming DownloadsObserver wait on select file dialog
// functionality anyway.
IN_PROC_BROWSER_TEST_F(DownloadTest, DISABLED_DownloadMimeTypeSelect) {
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  ASSERT_TRUE(CreateAndSetDownloadsDirectory());
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload, true);

  EXPECT_EQ(1, browser()->tab_count());

  // Setup notification, navigate, and block.
  scoped_ptr<DownloadsObserver> observer(CreateWaiter(1));
  ui_test_utils::NavigateToURL(
      browser(), URLRequestMockHTTPJob::GetMockUrl(file));
  observer->WaitForFinished();

  // Download should not be finished; check state.
  EXPECT_TRUE(observer->select_file_dialog_seen());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());
}

}  // namespace
