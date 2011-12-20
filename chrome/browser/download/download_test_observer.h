// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TEST_OBSERVER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TEST_OBSERVER_H_
#pragma once

#include <set>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

// Construction of this class defines a system state, based on some number
// of downloads being seen in a particular state + other events that
// may occur in the download system.  That state will be recorded if it
// occurs at any point after construction.  When that state occurs, the class
// is considered finished.  Callers may either probe for the finished state, or
// wait on it.
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
  // download items have entered state |download_finished_state|.
  // If |finish_on_select_file| is true, the object will also be
  // considered finished if the DownloadManager raises a
  // SelectFileDialogDisplayed() notification.

  // TODO(rdsmith): Consider rewriting the interface to take a list of events
  // to treat as completion events.
  DownloadTestObserver(
      content::DownloadManager* download_manager,
      size_t wait_count,
      content::DownloadItem::DownloadState download_finished_state,
      bool finish_on_select_file,
      DangerousDownloadAction dangerous_download_action);

  virtual ~DownloadTestObserver();

  // State accessors.
  bool select_file_dialog_seen() const { return select_file_dialog_seen_; }

  // Wait for whatever state was specified in the constructor.
  void WaitForFinished();

  // Return true if everything's happened that we're configured for.
  bool IsFinished() const;

  // content::DownloadItem::Observer
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE {}

  // content::DownloadManager::Observer
  virtual void ModelChanged() OVERRIDE;

  virtual void SelectFileDialogDisplayed(int32 id) OVERRIDE;

  size_t NumDangerousDownloadsSeen() const;

 private:
  typedef std::set<content::DownloadItem*> DownloadSet;

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

  // The state on which to consider the DownloadItem finished.
  content::DownloadItem::DownloadState download_finished_state_;

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
  virtual void ModelChanged() OVERRIDE;

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

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TEST_OBSERVER_H_
