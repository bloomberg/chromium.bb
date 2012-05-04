// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/download/download_test_observer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_url_parameters.h"

using content::BrowserThread;
using content::DownloadItem;
using content::DownloadManager;

// These functions take scoped_refptr's to DownloadManager because they
// are posted to message queues, and hence may execute arbitrarily after
// their actual posting.  Once posted, there is no connection between
// these routines and the DownloadTestObserver class from which
// they came, so the DownloadTestObserver's reference to the
// DownloadManager cannot be counted on to keep the DownloadManager around.

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
    bool finish_on_select_file,
    DangerousDownloadAction dangerous_download_action)
    : download_manager_(download_manager),
      wait_count_(wait_count),
      finished_downloads_at_construction_(0),
      waiting_(false),
      finish_on_select_file_(finish_on_select_file),
      select_file_dialog_seen_(false),
      dangerous_download_action_(dangerous_download_action) {
}

DownloadTestObserver::~DownloadTestObserver() {
  for (DownloadSet::iterator it = downloads_observed_.begin();
       it != downloads_observed_.end(); ++it)
    (*it)->RemoveObserver(this);

  download_manager_->RemoveObserver(this);
}

void DownloadTestObserver::Init() {
  download_manager_->AddObserver(this);  // Will call initial ModelChanged().
  finished_downloads_at_construction_ = finished_downloads_.size();
  states_observed_.clear();
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
            base::Bind(&AcceptDangerousDownload, download_manager_,
                       download->GetId()));
        break;

      case ON_DANGEROUS_DOWNLOAD_DENY:
        // Fake a user click on "Deny".  Delay the actual click, as the
        // real UI would.
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&DenyDangerousDownload, download_manager_,
                       download->GetId()));
        break;

      case ON_DANGEROUS_DOWNLOAD_FAIL:
        ADD_FAILURE() << "Unexpected dangerous download item.";
        break;

      default:
        NOTREACHED();
    }
  }

  if (IsDownloadInFinalState(download))
    DownloadInFinalState(download);
}

void DownloadTestObserver::ModelChanged(DownloadManager* manager) {
  DCHECK_EQ(manager, download_manager_);

  // Regenerate DownloadItem observers.  If there are any download items
  // in our final state, note them in |finished_downloads_|
  // (done by |OnDownloadUpdated()|).
  std::vector<DownloadItem*> downloads;
  download_manager_->GetAllDownloads(FilePath(), &downloads);
  // As a test class, we're generally interested in whatever's there,
  // so we include temporary downloads.
  download_manager_->GetTemporaryDownloads(FilePath(), &downloads);

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

void DownloadTestObserver::SelectFileDialogDisplayed(
    DownloadManager* manager, int32 /* id */) {
  DCHECK_EQ(manager, download_manager_);
  select_file_dialog_seen_ = true;
  SignalIfFinished();
}

size_t DownloadTestObserver::NumDangerousDownloadsSeen() const {
  return dangerous_downloads_seen_.size();
}

size_t DownloadTestObserver::NumDownloadsSeenInState(
    content::DownloadItem::DownloadState state) const {
  StateMap::const_iterator it = states_observed_.find(state);

  if (it == states_observed_.end())
    return 0;

  return it->second;
}

void DownloadTestObserver::DownloadInFinalState(DownloadItem* download) {
  if (finished_downloads_.find(download) != finished_downloads_.end()) {
    // We've already seen the final state on this download.
    return;
  }

  // Record the transition.
  finished_downloads_.insert(download);

  // Record the state.
  states_observed_[download->GetState()]++;  // Initializes to 0 the first time.

  SignalIfFinished();
}

void DownloadTestObserver::SignalIfFinished() {
  if (waiting_ && IsFinished())
    MessageLoopForUI::current()->Quit();
}

DownloadTestObserverTerminal::DownloadTestObserverTerminal(
    content::DownloadManager* download_manager,
    size_t wait_count,
    bool finish_on_select_file,
    DangerousDownloadAction dangerous_download_action)
        : DownloadTestObserver(download_manager,
                               wait_count,
                               finish_on_select_file,
                               dangerous_download_action) {
  // You can't rely on overriden virtual functions in a base class constructor;
  // the virtual function table hasn't been set up yet.  So, we have to do any
  // work that depends on those functions in the derived class constructor
  // instead.  In this case, it's because of |IsDownloadInFinalState()|.
  Init();
}

DownloadTestObserverTerminal::~DownloadTestObserverTerminal() {
}


bool DownloadTestObserverTerminal::IsDownloadInFinalState(
    content::DownloadItem* download) {
  return (download->GetState() != DownloadItem::IN_PROGRESS);
}

DownloadTestObserverInProgress::DownloadTestObserverInProgress(
    content::DownloadManager* download_manager,
    size_t wait_count,
    bool finish_on_select_file)
        : DownloadTestObserver(download_manager,
                               wait_count,
                               finish_on_select_file,
                               ON_DANGEROUS_DOWNLOAD_ACCEPT) {
  // You can't override virtual functions in a base class constructor; the
  // virtual function table hasn't been set up yet.  So, we have to do any
  // work that depends on those functions in the derived class constructor
  // instead.  In this case, it's because of |IsDownloadInFinalState()|.
  Init();
}

DownloadTestObserverInProgress::~DownloadTestObserverInProgress() {
}


bool DownloadTestObserverInProgress::IsDownloadInFinalState(
    content::DownloadItem* download) {
  return (download->GetState() == DownloadItem::IN_PROGRESS);
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

void DownloadTestFlushObserver::ModelChanged(DownloadManager* manager) {
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
          base::Bind(&DownloadTestFlushObserver::PingFileThread, this, 2));
    }
  }
}

void DownloadTestFlushObserver::PingFileThread(int cycle) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DownloadTestFlushObserver::PingIOThread, this, cycle));
}

void DownloadTestFlushObserver::PingIOThread(int cycle) {
  if (--cycle) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DownloadTestFlushObserver::PingFileThread, this, cycle));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, MessageLoop::QuitClosure());
  }
}

DownloadTestItemCreationObserver::DownloadTestItemCreationObserver()
    : download_id_(content::DownloadId::Invalid()),
      error_(net::OK),
      called_back_count_(0),
      waiting_(false) {
}

DownloadTestItemCreationObserver::~DownloadTestItemCreationObserver() {
}

void DownloadTestItemCreationObserver::WaitForDownloadItemCreation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (called_back_count_ == 0) {
    waiting_ = true;
    ui_test_utils::RunMessageLoop();
    waiting_ = false;
  }
}

void DownloadTestItemCreationObserver::DownloadItemCreationCallback(
    content::DownloadId download_id, net::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  download_id_ = download_id;
  error_ = error;
  ++called_back_count_;
  DCHECK_EQ(1u, called_back_count_);

  if (waiting_)
    MessageLoopForUI::current()->Quit();
}

const content::DownloadUrlParameters::OnStartedCallback
    DownloadTestItemCreationObserver::callback() {
  return base::Bind(
      &DownloadTestItemCreationObserver::DownloadItemCreationCallback, this);
}

