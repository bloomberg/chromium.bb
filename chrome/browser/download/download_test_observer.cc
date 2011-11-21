// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/task.h"
#include "chrome/browser/download/download_test_observer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// These functions take scoped_refptr's to DownloadManager because they
// are posted to message queues, and hence may execute arbitrarily after
// their actual posting.  Once posted, there is no connection between
// these routines and the DownloadTestObserver class from which they came,
// so the DownloadTestObserver's reference to the DownloadManager cannot
// be counted on to keep the DownloadManager around.

// Fake user click on "Accept".
void AcceptDangerousDownload(scoped_refptr<DownloadManager> download_manager,
                             int32 download_id) {
  DownloadItem* download = download_manager->GetDownloadItem(download_id);
  download->DangerousDownloadValidated();
}

// Fake user click on "Deny".
void DenyDangerousDownload(scoped_refptr<DownloadManager> download_manager,
                           int32 download_id) {
  DownloadItem* download = download_manager->GetDownloadItem(download_id);
  ASSERT_TRUE(download->IsPartialDownload());
  download->Cancel(true);
  download->Delete(DownloadItem::DELETE_DUE_TO_USER_DISCARD);
}

DownloadTestObserver::DownloadTestObserver(
    DownloadManager* download_manager,
    size_t wait_count,
    DownloadItem::DownloadState download_finished_state,
    bool finish_on_select_file,
    DangerousDownloadAction dangerous_download_action)
    : download_manager_(download_manager),
      wait_count_(wait_count),
      finished_downloads_at_construction_(0),
      waiting_(false),
      download_finished_state_(download_finished_state),
      finish_on_select_file_(finish_on_select_file),
      select_file_dialog_seen_(false),
      dangerous_download_action_(dangerous_download_action) {
  download_manager_->AddObserver(this);  // Will call initial ModelChanged().
  finished_downloads_at_construction_ = finished_downloads_.size();
  EXPECT_NE(DownloadItem::REMOVING, download_finished_state)
      << "Waiting for REMOVING is not supported.  Try COMPLETE.";
      }

DownloadTestObserver::~DownloadTestObserver() {
  for (DownloadSet::iterator it = downloads_observed_.begin();
       it != downloads_observed_.end(); ++it)
    (*it)->RemoveObserver(this);

  download_manager_->RemoveObserver(this);
}

void DownloadTestObserver::WaitForFinished() {
  if (!IsFinished()) {
    waiting_ = true;
    ui_test_utils::RunMessageLoop();
    waiting_ = false;
  }
}

bool DownloadTestObserver::IsFinished() const {
  if (finished_downloads_.size() - finished_downloads_at_construction_ >=
      wait_count_)
    return true;
  return (finish_on_select_file_ && select_file_dialog_seen_);
}

void DownloadTestObserver::OnDownloadUpdated(DownloadItem* download) {
  // The REMOVING state indicates that the download is being destroyed.
  // Stop observing.  Do not do anything with it, as it is about to be gone.
  if (download->GetState() == DownloadItem::REMOVING) {
    DownloadSet::iterator it = downloads_observed_.find(download);
    ASSERT_TRUE(it != downloads_observed_.end());
    downloads_observed_.erase(it);
    download->RemoveObserver(this);
    return;
  }

  // Real UI code gets the user's response after returning from the observer.
  if (download->GetSafetyState() == DownloadItem::DANGEROUS &&
      !ContainsKey(dangerous_downloads_seen_, download->GetId())) {
    dangerous_downloads_seen_.insert(download->GetId());

    // Calling DangerousDownloadValidated() at this point will
    // cause the download to be completed twice.  Do what the real UI
    // code does: make the call as a delayed task.
    switch (dangerous_download_action_) {
      case ON_DANGEROUS_DOWNLOAD_ACCEPT:
        // Fake user click on "Accept".  Delay the actual click, as the
        // real UI would.
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            NewRunnableFunction(
                &AcceptDangerousDownload,
                download_manager_,
                download->GetId()));
        break;

      case ON_DANGEROUS_DOWNLOAD_DENY:
        // Fake a user click on "Deny".  Delay the actual click, as the
        // real UI would.
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            NewRunnableFunction(
                &DenyDangerousDownload,
                download_manager_,
                download->GetId()));
        break;

      case ON_DANGEROUS_DOWNLOAD_FAIL:
        ADD_FAILURE() << "Unexpected dangerous download item.";
        break;

      default:
        NOTREACHED();
    }
  }

  if (download->GetState() == download_finished_state_) {
    DownloadInFinalState(download);
  }
}

void DownloadTestObserver::ModelChanged() {
  // Regenerate DownloadItem observers.  If there are any download items
  // in our final state, note them in |finished_downloads_|
  // (done by |OnDownloadUpdated()|).
  std::vector<DownloadItem*> downloads;
  download_manager_->GetAllDownloads(FilePath(), &downloads);

  for (std::vector<DownloadItem*>::iterator it = downloads.begin();
       it != downloads.end(); ++it) {
    OnDownloadUpdated(*it);  // Safe to call multiple times; checks state.

    DownloadSet::const_iterator finished_it(finished_downloads_.find(*it));
    DownloadSet::iterator observed_it(downloads_observed_.find(*it));

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

void DownloadTestObserver::SelectFileDialogDisplayed(int32 /* id */) {
  select_file_dialog_seen_ = true;
  SignalIfFinished();
}

size_t DownloadTestObserver::NumDangerousDownloadsSeen() const {
  return dangerous_downloads_seen_.size();
}

void DownloadTestObserver::DownloadInFinalState(DownloadItem* download) {
  if (finished_downloads_.find(download) != finished_downloads_.end()) {
    // We've already seen terminal state on this download.
    return;
  }

  // Record the transition.
  finished_downloads_.insert(download);

  SignalIfFinished();
}

void DownloadTestObserver::SignalIfFinished() {
  if (waiting_ && IsFinished())
    MessageLoopForUI::current()->Quit();
}

DownloadTestFlushObserver::DownloadTestFlushObserver(
    DownloadManager* download_manager)
    : download_manager_(download_manager),
      waiting_for_zero_inprogress_(true) {}

void DownloadTestFlushObserver::WaitForFlush() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  download_manager_->AddObserver(this);
  ui_test_utils::RunMessageLoop();
}

void DownloadTestFlushObserver::ModelChanged() {
  // Model has changed, so there may be more DownloadItems to observe.
  CheckDownloadsInProgress(true);
}

void DownloadTestFlushObserver::OnDownloadUpdated(DownloadItem* download) {
  // The REMOVING state indicates that the download is being destroyed.
  // Stop observing.  Do not do anything with it, as it is about to be gone.
  if (download->GetState() == DownloadItem::REMOVING) {
    DownloadSet::iterator it = downloads_observed_.find(download);
    ASSERT_TRUE(it != downloads_observed_.end());
    downloads_observed_.erase(it);
    download->RemoveObserver(this);
    return;
  }

  // No change in DownloadItem set on manager.
  CheckDownloadsInProgress(false);
}

DownloadTestFlushObserver::~DownloadTestFlushObserver() {
  download_manager_->RemoveObserver(this);
  for (DownloadSet::iterator it = downloads_observed_.begin();
       it != downloads_observed_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
}

// If we're waiting for that flush point, check the number
// of downloads in the IN_PROGRESS state and take appropriate
// action.  If requested, also observes all downloads while iterating.
void DownloadTestFlushObserver::CheckDownloadsInProgress(
    bool observe_downloads) {
  if (waiting_for_zero_inprogress_) {
    int count = 0;

    std::vector<DownloadItem*> downloads;
    download_manager_->SearchDownloads(string16(), &downloads);
    for (std::vector<DownloadItem*>::iterator it = downloads.begin();
         it != downloads.end(); ++it) {
      if ((*it)->GetState() == DownloadItem::IN_PROGRESS)
        count++;
      if (observe_downloads) {
        if (downloads_observed_.find(*it) == downloads_observed_.end()) {
          (*it)->AddObserver(this);
          downloads_observed_.insert(*it);
        }
        // Download items are forever, and we don't want to make
        // assumptions about future state transitions, so once we
        // start observing them, we don't stop until destruction.
      }
    }

    if (count == 0) {
      waiting_for_zero_inprogress_ = false;
      // Stop observing DownloadItems.  We maintain the observation
      // of DownloadManager so that we don't have to independently track
      // whether we are observing it for conditional destruction.
      for (DownloadSet::iterator it = downloads_observed_.begin();
           it != downloads_observed_.end(); ++it) {
        (*it)->RemoveObserver(this);
      }
      downloads_observed_.clear();

      // Trigger next step.  We need to go past the IO thread twice, as
      // there's a self-task posting in the IO thread cancel path.
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(this,
                            &DownloadTestFlushObserver::PingFileThread, 2));
    }
  }
}

void DownloadTestFlushObserver::PingFileThread(int cycle) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &DownloadTestFlushObserver::PingIOThread,
                        cycle));
}

void DownloadTestFlushObserver::PingIOThread(int cycle) {
  if (--cycle) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &DownloadTestFlushObserver::PingFileThread,
                          cycle));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, new MessageLoop::QuitTask());
  }
}
