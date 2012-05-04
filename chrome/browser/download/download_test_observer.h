// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TEST_OBSERVER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TEST_OBSERVER_H_
#pragma once

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "net/base/net_errors.h"

// Detects changes to the downloads after construction.
// Finishes when one of the following happens:
//   - A specified number of downloads change to a terminal state (defined
//     in derived classes).
//   - Specific events, such as a select file dialog.
// Callers may either probe for the finished state, or wait on it.
//
// TODO(rdsmith): Detect manager going down, remove pointer to
// DownloadManager, transition to finished.  (For right now we
// just use a scoped_refptr<> to keep it around, but that may cause
// timeouts on waiting if a DownloadManager::Shutdown() occurs which
// cancels our in-progress downloads.)
class DownloadTestObserver : public content::DownloadManager::Observer,
                             public content::DownloadItem::Observer {
 public:
  // Action an observer should take if a dangerous download is encountered.
  enum DangerousDownloadAction {
    ON_DANGEROUS_DOWNLOAD_ACCEPT,  // Accept the download
    ON_DANGEROUS_DOWNLOAD_DENY,  // Deny the download
    ON_DANGEROUS_DOWNLOAD_FAIL  // Fail if a dangerous download is seen
  };

  // Create an object that will be considered finished when |wait_count|
  // download items have entered a terminal state.
  // If |finish_on_select_file| is true, the object will also be
  // considered finished if the DownloadManager raises a
  // SelectFileDialogDisplayed() notification.
  DownloadTestObserver(
      content::DownloadManager* download_manager,
      size_t wait_count,
      bool finish_on_select_file,
      DangerousDownloadAction dangerous_download_action);

  virtual ~DownloadTestObserver();

  // State accessors.
  bool select_file_dialog_seen() const { return select_file_dialog_seen_; }

  // Wait for the requested number of downloads to enter a terminal state.
  void WaitForFinished();

  // Return true if everything's happened that we're configured for.
  bool IsFinished() const;

  // content::DownloadItem::Observer
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE {}

  // content::DownloadManager::Observer
  virtual void ModelChanged(content::DownloadManager* manager) OVERRIDE;

  virtual void SelectFileDialogDisplayed(
      content::DownloadManager* manager, int32 id) OVERRIDE;

  size_t NumDangerousDownloadsSeen() const;

  size_t NumDownloadsSeenInState(
      content::DownloadItem::DownloadState state) const;

 protected:
  // Only to be called by derived classes' constructors.
  virtual void Init();

  // Called to see if a download item is in a final state.
  virtual bool IsDownloadInFinalState(content::DownloadItem* download) = 0;

 private:
  typedef std::set<content::DownloadItem*> DownloadSet;

  // Maps states to the number of times they have been encountered
  typedef std::map<content::DownloadItem::DownloadState, size_t> StateMap;

  // Called when we know that a download item is in a final state.
  // Note that this is not the same as it first transitioning in to the
  // final state; multiple notifications may occur once the item is in
  // that state.  So we keep our own track of transitions into final.
  void DownloadInFinalState(content::DownloadItem* download);

  void SignalIfFinished();

  // The observed download manager.
  scoped_refptr<content::DownloadManager> download_manager_;

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

  // The number of downloads entered in final state in initial
  // ModelChanged().  We use |finished_downloads_| to track the incoming
  // transitions to final state we should ignore, and to track the
  // number of final state transitions that occurred between
  // construction and return from wait.  But some downloads may be in our
  // final state (and thus be entered into |finished_downloads_|) when we
  // construct this class.  We don't want to count those in our transition
  // to finished.
  int finished_downloads_at_construction_;

  // Whether an internal message loop has been started and must be quit upon
  // all downloads completing.
  bool waiting_;

  // True if we should transition the DownloadTestObserver to finished if
  // the select file dialog comes up.
  bool finish_on_select_file_;

  // True if we've seen the select file dialog.
  bool select_file_dialog_seen_;

  // Action to take if a dangerous download is encountered.
  DangerousDownloadAction dangerous_download_action_;

  // Holds the download ids which were dangerous.
  std::set<int32> dangerous_downloads_seen_;

  DISALLOW_COPY_AND_ASSIGN(DownloadTestObserver);
};

class DownloadTestObserverTerminal : public DownloadTestObserver {
 public:
  // Create an object that will be considered finished when |wait_count|
  // download items have entered a terminal state (any but IN_PROGRESS).
  // If |finish_on_select_file| is true, the object will also be
  // considered finished if the DownloadManager raises a
  // SelectFileDialogDisplayed() notification.
  DownloadTestObserverTerminal(
      content::DownloadManager* download_manager,
      size_t wait_count,
      bool finish_on_select_file,
      DangerousDownloadAction dangerous_download_action);

  virtual ~DownloadTestObserverTerminal();

 private:
  virtual bool IsDownloadInFinalState(content::DownloadItem* download) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DownloadTestObserverTerminal);
};

// Detects changes to the downloads after construction.
// Finishes when a specified number of downloads change to the
// IN_PROGRESS state, or a Select File Dialog has appeared.
// Dangerous downloads are accepted.
// Callers may either probe for the finished state, or wait on it.
class DownloadTestObserverInProgress : public DownloadTestObserver {
 public:
  // Create an object that will be considered finished when |wait_count|
  // download items have entered state |IN_PROGRESS|.
  // If |finish_on_select_file| is true, the object will also be
  // considered finished if the DownloadManager raises a
  // SelectFileDialogDisplayed() notification.
  DownloadTestObserverInProgress(
      content::DownloadManager* download_manager,
      size_t wait_count,
      bool finish_on_select_file);

  virtual ~DownloadTestObserverInProgress();

 private:
  virtual bool IsDownloadInFinalState(content::DownloadItem* download) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DownloadTestObserverInProgress);
};

// The WaitForFlush() method on this class returns after:
//      * There are no IN_PROGRESS download items remaining on the
//        DownloadManager.
//      * There have been two round trip messages through the file and
//        IO threads.
// This almost certainly means that a Download cancel has propagated through
// the system.
class DownloadTestFlushObserver
    : public content::DownloadManager::Observer,
      public content::DownloadItem::Observer,
      public base::RefCountedThreadSafe<DownloadTestFlushObserver> {
 public:
  explicit DownloadTestFlushObserver(
      content::DownloadManager* download_manager);

  void WaitForFlush();

  // DownloadsManager observer methods.
  virtual void ModelChanged(content::DownloadManager* manager) OVERRIDE;

  // DownloadItem observer methods.
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE {}

 protected:
  friend class base::RefCountedThreadSafe<DownloadTestFlushObserver>;

  virtual ~DownloadTestFlushObserver();

 private:
  typedef std::set<content::DownloadItem*> DownloadSet;

  // If we're waiting for that flush point, check the number
  // of downloads in the IN_PROGRESS state and take appropriate
  // action.  If requested, also observes all downloads while iterating.
  void CheckDownloadsInProgress(bool observe_downloads);

  void PingFileThread(int cycle);

  void PingIOThread(int cycle);

  content::DownloadManager* download_manager_;
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

  content::DownloadId download_id() const { return download_id_; }
  net::Error error() const { return error_; }
  bool started() const { return called_back_count_ > 0; }
  bool succeeded() const { return started() && (error_ == net::OK); }

  const content::DownloadUrlParameters::OnStartedCallback callback();

 private:
  friend class base::RefCountedThreadSafe<DownloadTestItemCreationObserver>;

  ~DownloadTestItemCreationObserver();

  void DownloadItemCreationCallback(content::DownloadId download_id,
                                    net::Error error);

  // The download creation information we received.
  content::DownloadId download_id_;

  net::Error error_;

  // Count of callbacks.
  size_t called_back_count_;

  // We are in the message loop.
  bool waiting_;

  DISALLOW_COPY_AND_ASSIGN(DownloadTestItemCreationObserver);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TEST_OBSERVER_H_
