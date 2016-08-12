// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/download_test_observer.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

DownloadUpdatedObserver::DownloadUpdatedObserver(
    DownloadItem* item, DownloadUpdatedObserver::EventFilter filter)
    : item_(item),
      filter_(filter),
      waiting_(false),
      event_seen_(false) {
  item->AddObserver(this);
}

DownloadUpdatedObserver::~DownloadUpdatedObserver() {
  if (item_)
    item_->RemoveObserver(this);
}

bool DownloadUpdatedObserver::WaitForEvent() {
  if (item_ && filter_.Run(item_))
    event_seen_ = true;
  if (event_seen_)
    return true;

  waiting_ = true;
  RunMessageLoop();
  waiting_ = false;
  return event_seen_;
}

void DownloadUpdatedObserver::OnDownloadUpdated(DownloadItem* item) {
  DCHECK_EQ(item_, item);
  if (filter_.Run(item_))
    event_seen_ = true;
  if (waiting_ && event_seen_)
    base::MessageLoopForUI::current()->QuitWhenIdle();
}

void DownloadUpdatedObserver::OnDownloadDestroyed(DownloadItem* item) {
  DCHECK_EQ(item_, item);
  item_->RemoveObserver(this);
  item_ = NULL;
  if (waiting_)
    base::MessageLoopForUI::current()->QuitWhenIdle();
}

DownloadTestObserver::DownloadTestObserver(
    DownloadManager* download_manager,
    size_t wait_count,
    DangerousDownloadAction dangerous_download_action)
    : download_manager_(download_manager),
      wait_count_(wait_count),
      finished_downloads_at_construction_(0),
      waiting_(false),
      dangerous_download_action_(dangerous_download_action),
      weak_factory_(this) {
}

DownloadTestObserver::~DownloadTestObserver() {
  for (DownloadSet::iterator it = downloads_observed_.begin();
       it != downloads_observed_.end(); ++it)
    (*it)->RemoveObserver(this);

  if (download_manager_)
    download_manager_->RemoveObserver(this);
}

void DownloadTestObserver::Init() {
  download_manager_->AddObserver(this);
  std::vector<DownloadItem*> downloads;
  download_manager_->GetAllDownloads(&downloads);
  for (std::vector<DownloadItem*>::iterator it = downloads.begin();
       it != downloads.end(); ++it) {
    OnDownloadCreated(download_manager_, *it);
  }
  finished_downloads_at_construction_ = finished_downloads_.size();
  states_observed_.clear();
}

void DownloadTestObserver::ManagerGoingDown(DownloadManager* manager) {
  CHECK_EQ(manager, download_manager_);
  download_manager_ = NULL;
  SignalIfFinished();
}

void DownloadTestObserver::WaitForFinished() {
  if (!IsFinished()) {
    waiting_ = true;
    RunMessageLoop();
    waiting_ = false;
  }
}

bool DownloadTestObserver::IsFinished() const {
  return (finished_downloads_.size() - finished_downloads_at_construction_ >=
          wait_count_) || (download_manager_ == NULL);
}

void DownloadTestObserver::OnDownloadCreated(
    DownloadManager* manager,
    DownloadItem* item) {
  // NOTE: This method is called both by DownloadManager when a download is
  // created as well as in DownloadTestObserver::Init() for downloads that
  // existed before |this| was created.
  OnDownloadUpdated(item);
  DownloadSet::const_iterator finished_it(finished_downloads_.find(item));
  // If it isn't finished, start observing it.
  if (finished_it == finished_downloads_.end()) {
    item->AddObserver(this);
    downloads_observed_.insert(item);
  }
}

void DownloadTestObserver::OnDownloadDestroyed(DownloadItem* download) {
  // Stop observing.  Do not do anything with it, as it is about to be gone.
  DownloadSet::iterator it = downloads_observed_.find(download);
  ASSERT_TRUE(it != downloads_observed_.end());
  downloads_observed_.erase(it);
  download->RemoveObserver(this);
}

void DownloadTestObserver::OnDownloadUpdated(DownloadItem* download) {
  // Real UI code gets the user's response after returning from the observer.
  if (download->IsDangerous() &&
      !base::ContainsKey(dangerous_downloads_seen_, download->GetId())) {
    dangerous_downloads_seen_.insert(download->GetId());

    // Calling ValidateDangerousDownload() at this point will
    // cause the download to be completed twice.  Do what the real UI
    // code does: make the call as a delayed task.
    switch (dangerous_download_action_) {
      case ON_DANGEROUS_DOWNLOAD_ACCEPT:
        // Fake user click on "Accept".  Delay the actual click, as the
        // real UI would.
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&DownloadTestObserver::AcceptDangerousDownload,
                       weak_factory_.GetWeakPtr(),
                       download->GetId()));
        break;

      case ON_DANGEROUS_DOWNLOAD_DENY:
        // Fake a user click on "Deny".  Delay the actual click, as the
        // real UI would.
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&DownloadTestObserver::DenyDangerousDownload,
                       weak_factory_.GetWeakPtr(),
                       download->GetId()));
        break;

      case ON_DANGEROUS_DOWNLOAD_FAIL:
        ADD_FAILURE() << "Unexpected dangerous download item.";
        break;

      case ON_DANGEROUS_DOWNLOAD_IGNORE:
        break;

      case ON_DANGEROUS_DOWNLOAD_QUIT:
        DownloadInFinalState(download);
        break;

      default:
        NOTREACHED();
    }
  }

  if (IsDownloadInFinalState(download))
    DownloadInFinalState(download);
}

size_t DownloadTestObserver::NumDangerousDownloadsSeen() const {
  return dangerous_downloads_seen_.size();
}

size_t DownloadTestObserver::NumDownloadsSeenInState(
    DownloadItem::DownloadState state) const {
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
    base::MessageLoopForUI::current()->QuitWhenIdle();
}

void DownloadTestObserver::AcceptDangerousDownload(uint32_t download_id) {
  // Download manager was shutdown before the UI thread could accept the
  // download.
  if (!download_manager_)
    return;
  DownloadItem* download = download_manager_->GetDownload(download_id);
  if (download && !download->IsDone())
    download->ValidateDangerousDownload();
}

void DownloadTestObserver::DenyDangerousDownload(uint32_t download_id) {
  // Download manager was shutdown before the UI thread could deny the
  // download.
  if (!download_manager_)
    return;
  DownloadItem* download = download_manager_->GetDownload(download_id);
  if (download && !download->IsDone())
    download->Remove();
}

DownloadTestObserverTerminal::DownloadTestObserverTerminal(
    DownloadManager* download_manager,
    size_t wait_count,
    DangerousDownloadAction dangerous_download_action)
        : DownloadTestObserver(download_manager,
                               wait_count,
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
    DownloadItem* download) {
  return download->IsDone();
}

DownloadTestObserverInProgress::DownloadTestObserverInProgress(
    DownloadManager* download_manager,
    size_t wait_count)
        : DownloadTestObserver(download_manager,
                               wait_count,
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
    DownloadItem* download) {
  return (download->GetState() == DownloadItem::IN_PROGRESS) &&
      !download->GetTargetFilePath().empty();
}

DownloadTestObserverInterrupted::DownloadTestObserverInterrupted(
    DownloadManager* download_manager,
    size_t wait_count,
    DangerousDownloadAction dangerous_download_action)
        : DownloadTestObserver(download_manager,
                               wait_count,
                               dangerous_download_action) {
  // You can't rely on overriden virtual functions in a base class constructor;
  // the virtual function table hasn't been set up yet.  So, we have to do any
  // work that depends on those functions in the derived class constructor
  // instead.  In this case, it's because of |IsDownloadInFinalState()|.
  Init();
}

DownloadTestObserverInterrupted::~DownloadTestObserverInterrupted() {
}


bool DownloadTestObserverInterrupted::IsDownloadInFinalState(
    DownloadItem* download) {
  return download->GetState() == DownloadItem::INTERRUPTED;
}

DownloadTestFlushObserver::DownloadTestFlushObserver(
    DownloadManager* download_manager)
    : download_manager_(download_manager),
      waiting_for_zero_inprogress_(true) {}

void DownloadTestFlushObserver::WaitForFlush() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  download_manager_->AddObserver(this);
  // The wait condition may have been met before WaitForFlush() was called.
  CheckDownloadsInProgress(true);
  BrowserThread::GetBlockingPool()->FlushForTesting();
  RunMessageLoop();
}

void DownloadTestFlushObserver::OnDownloadCreated(
    DownloadManager* manager,
    DownloadItem* item) {
  CheckDownloadsInProgress(true);
}

void DownloadTestFlushObserver::OnDownloadDestroyed(DownloadItem* download) {
  // Stop observing.  Do not do anything with it, as it is about to be gone.
  DownloadSet::iterator it = downloads_observed_.find(download);
  ASSERT_TRUE(it != downloads_observed_.end());
  downloads_observed_.erase(it);
  download->RemoveObserver(this);
}

void DownloadTestFlushObserver::OnDownloadUpdated(DownloadItem* download) {
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
    download_manager_->GetAllDownloads(&downloads);
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
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::MessageLoop::QuitWhenIdleClosure());
  }
}

DownloadTestItemCreationObserver::DownloadTestItemCreationObserver()
    : download_id_(DownloadItem::kInvalidId),
      interrupt_reason_(DOWNLOAD_INTERRUPT_REASON_NONE),
      called_back_count_(0),
      waiting_(false) {
}

DownloadTestItemCreationObserver::~DownloadTestItemCreationObserver() {
}

void DownloadTestItemCreationObserver::WaitForDownloadItemCreation() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (called_back_count_ == 0) {
    waiting_ = true;
    RunMessageLoop();
    waiting_ = false;
  }
}

void DownloadTestItemCreationObserver::DownloadItemCreationCallback(
    DownloadItem* item,
    DownloadInterruptReason interrupt_reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (item)
    download_id_ = item->GetId();
  interrupt_reason_ = interrupt_reason;
  ++called_back_count_;
  DCHECK_EQ(1u, called_back_count_);

  if (waiting_)
    base::MessageLoopForUI::current()->QuitWhenIdle();
}

const DownloadUrlParameters::OnStartedCallback
    DownloadTestItemCreationObserver::callback() {
  return base::Bind(
      &DownloadTestItemCreationObserver::DownloadItemCreationCallback, this);
}

}  // namespace content
