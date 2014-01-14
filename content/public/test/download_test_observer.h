// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_DOWNLOAD_TEST_OBSERVER_H_
#define CONTENT_TEST_DOWNLOAD_TEST_OBSERVER_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"

namespace content {

// Detects an arbitrary change on a download item.
// TODO: Rewrite other observers to use this (or be replaced by it).
class DownloadUpdatedObserver : public DownloadItem::Observer {
 public:
  typedef base::Callback<bool(DownloadItem*)> EventFilter;

  // The filter passed may be called multiple times, even after it
  // returns true.
  DownloadUpdatedObserver(DownloadItem* item, EventFilter filter);
  virtual ~DownloadUpdatedObserver();

  // Returns when either the event has been seen (at least once since
  // object construction) or the item is destroyed.  Return value indicates
  // if the wait ended because the item was seen (true) or the object
  // destroyed (false).
  bool WaitForEvent();

 private:
  // DownloadItem::Observer
  virtual void OnDownloadUpdated(DownloadItem* item) OVERRIDE;
  virtual void OnDownloadDestroyed(DownloadItem* item) OVERRIDE;

  DownloadItem* item_;
  EventFilter filter_;
  bool waiting_;
  bool event_seen_;

  DISALLOW_COPY_AND_ASSIGN(DownloadUpdatedObserver);
};

// Detects changes to the downloads after construction.
//
// Finishes when one of the following happens:
//   - A specified number of downloads change to a terminal state (defined
//     in derived classes).
//   - The download manager was shutdown.
//
// Callers may either probe for the finished state, or wait on it.
class DownloadTestObserver : public DownloadManager::Observer,
                             public DownloadItem::Observer {
 public:
  // Action an observer should take if a dangerous download is encountered.
  enum DangerousDownloadAction {
    ON_DANGEROUS_DOWNLOAD_ACCEPT,  // Accept the download
    ON_DANGEROUS_DOWNLOAD_DENY,    // Deny the download
    ON_DANGEROUS_DOWNLOAD_FAIL,    // Fail if a dangerous download is seen
    ON_DANGEROUS_DOWNLOAD_IGNORE,  // Make it the callers problem.
    ON_DANGEROUS_DOWNLOAD_QUIT     // Will set final state without decision.
  };

  // Create an object that will be considered finished when |wait_count|
  // download items have entered a terminal state.
  DownloadTestObserver(DownloadManager* download_manager,
                       size_t wait_count,
                       DangerousDownloadAction dangerous_download_action);

  virtual ~DownloadTestObserver();

  // Wait for one of the finish conditions.
  void WaitForFinished();

  // Return true if we reached one of the finish conditions.
  bool IsFinished() const;

  // DownloadItem::Observer
  virtual void OnDownloadUpdated(DownloadItem* download) OVERRIDE;
  virtual void OnDownloadDestroyed(DownloadItem* download) OVERRIDE;

  // DownloadManager::Observer
  virtual void OnDownloadCreated(
      DownloadManager* manager, DownloadItem* item) OVERRIDE;
  virtual void ManagerGoingDown(DownloadManager* manager) OVERRIDE;

  size_t NumDangerousDownloadsSeen() const;

  size_t NumDownloadsSeenInState(DownloadItem::DownloadState state) const;

 protected:
  // Only to be called by derived classes' constructors.
  virtual void Init();

  // Called to see if a download item is in a final state.
  virtual bool IsDownloadInFinalState(DownloadItem* download) = 0;

 private:
  typedef std::set<DownloadItem*> DownloadSet;

  // Maps states to the number of times they have been encountered
  typedef std::map<DownloadItem::DownloadState, size_t> StateMap;

  // Called when we know that a download item is in a final state.
  // Note that this is not the same as it first transitioning in to the
  // final state; multiple notifications may occur once the item is in
  // that state.  So we keep our own track of transitions into final.
  void DownloadInFinalState(DownloadItem* download);

  void SignalIfFinished();

  // Fake user click on "Accept".
  void AcceptDangerousDownload(uint32 download_id);

  // Fake user click on "Deny".
  void DenyDangerousDownload(uint32 download_id);

  // The observed download manager.
  DownloadManager* download_manager_;

  // The set of DownloadItem's that have transitioned to their finished state
  // since construction of this object.  When the size of this array
  // reaches wait_count_, we're done.
  DownloadSet finished_downloads_;

  // The set of DownloadItem's we are currently observing.  Generally there
  // won't be any overlap with the above; once we see the final state
  // on a DownloadItem, we'll stop observing it.
  DownloadSet downloads_observed_;

  // The map of states to the number of times they have been observed since
  // we started looking.
  // Recorded at the time downloads_observed_ is recorded, but cleared in the
  // constructor to exclude pre-existing states.
  StateMap states_observed_;

  // The number of downloads to wait on completing.
  size_t wait_count_;

  // The number of downloads entered in final state in Init().  We use
  // |finished_downloads_| to track the incoming transitions to final state we
  // should ignore, and to track the number of final state transitions that
  // occurred between construction and return from wait.  But some downloads may
  // be in our final state (and thus be entered into |finished_downloads_|) when
  // we construct this class.  We don't want to count those in our transition to
  // finished.
  int finished_downloads_at_construction_;

  // Whether an internal message loop has been started and must be quit upon
  // all downloads completing.
  bool waiting_;

  // Action to take if a dangerous download is encountered.
  DangerousDownloadAction dangerous_download_action_;

  // Holds the download ids which were dangerous.
  std::set<uint32> dangerous_downloads_seen_;

  base::WeakPtrFactory<DownloadTestObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadTestObserver);
};

class DownloadTestObserverTerminal : public DownloadTestObserver {
 public:
  // Create an object that will be considered finished when |wait_count|
  // download items have entered a terminal state (DownloadItem::IsDone() is
  // true).
  DownloadTestObserverTerminal(
      DownloadManager* download_manager,
      size_t wait_count,
      DangerousDownloadAction dangerous_download_action);

  virtual ~DownloadTestObserverTerminal();

 private:
  virtual bool IsDownloadInFinalState(DownloadItem* download) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DownloadTestObserverTerminal);
};

// Detects changes to the downloads after construction.
// Finishes when a specified number of downloads change to the
// IN_PROGRESS state, or when the download manager is destroyed.
// Dangerous downloads are accepted.
// Callers may either probe for the finished state, or wait on it.
class DownloadTestObserverInProgress : public DownloadTestObserver {
 public:
  // Create an object that will be considered finished when |wait_count|
  // download items have entered state |IN_PROGRESS|.
  DownloadTestObserverInProgress(
      DownloadManager* download_manager, size_t wait_count);

  virtual ~DownloadTestObserverInProgress();

 private:
  virtual bool IsDownloadInFinalState(DownloadItem* download) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DownloadTestObserverInProgress);
};

class DownloadTestObserverInterrupted : public DownloadTestObserver {
 public:
  // Create an object that will be considered finished when |wait_count|
  // download items are interrupted.
  DownloadTestObserverInterrupted(
      DownloadManager* download_manager,
      size_t wait_count,
      DangerousDownloadAction dangerous_download_action);

  virtual ~DownloadTestObserverInterrupted();

 private:
  virtual bool IsDownloadInFinalState(DownloadItem* download) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DownloadTestObserverInterrupted);
};

// The WaitForFlush() method on this class returns after:
//      * There are no IN_PROGRESS download items remaining on the
//        DownloadManager.
//      * There have been two round trip messages through the file and
//        IO threads.
// This almost certainly means that a Download cancel has propagated through
// the system.
class DownloadTestFlushObserver
    : public DownloadManager::Observer,
      public DownloadItem::Observer,
      public base::RefCountedThreadSafe<DownloadTestFlushObserver> {
 public:
  explicit DownloadTestFlushObserver(DownloadManager* download_manager);

  void WaitForFlush();

  // DownloadsManager observer methods.
  virtual void OnDownloadCreated(
      DownloadManager* manager,
      DownloadItem* item) OVERRIDE;

  // DownloadItem observer methods.
  virtual void OnDownloadUpdated(DownloadItem* download) OVERRIDE;
  virtual void OnDownloadDestroyed(DownloadItem* download) OVERRIDE;

 protected:
  friend class base::RefCountedThreadSafe<DownloadTestFlushObserver>;

  virtual ~DownloadTestFlushObserver();

 private:
  typedef std::set<DownloadItem*> DownloadSet;

  // If we're waiting for that flush point, check the number
  // of downloads in the IN_PROGRESS state and take appropriate
  // action.  If requested, also observes all downloads while iterating.
  void CheckDownloadsInProgress(bool observe_downloads);

  void PingFileThread(int cycle);

  void PingIOThread(int cycle);

  DownloadManager* download_manager_;
  DownloadSet downloads_observed_;
  bool waiting_for_zero_inprogress_;

  DISALLOW_COPY_AND_ASSIGN(DownloadTestFlushObserver);
};

// Waits for a callback indicating that the DownloadItem is about to be created,
// or that an error occurred and it won't be created.
class DownloadTestItemCreationObserver
    : public base::RefCountedThreadSafe<DownloadTestItemCreationObserver> {
 public:
  DownloadTestItemCreationObserver();

  void WaitForDownloadItemCreation();

  uint32 download_id() const { return download_id_; }
  DownloadInterruptReason interrupt_reason() const { return interrupt_reason_; }
  bool started() const { return called_back_count_ > 0; }
  bool succeeded() const {
    return started() && interrupt_reason_ == DOWNLOAD_INTERRUPT_REASON_NONE;
  }

  const DownloadUrlParameters::OnStartedCallback callback();

 private:
  friend class base::RefCountedThreadSafe<DownloadTestItemCreationObserver>;

  ~DownloadTestItemCreationObserver();

  void DownloadItemCreationCallback(DownloadItem* item,
                                    DownloadInterruptReason interrupt_reason);

  // The download creation information we received.
  uint32 download_id_;
  DownloadInterruptReason interrupt_reason_;

  // Count of callbacks.
  size_t called_back_count_;

  // We are in the message loop.
  bool waiting_;

  DISALLOW_COPY_AND_ASSIGN(DownloadTestItemCreationObserver);
};

}  // namespace content`

#endif  // CONTENT_TEST_DOWNLOAD_TEST_OBSERVER_H_
