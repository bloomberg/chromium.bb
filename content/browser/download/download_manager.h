// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DownloadManager object manages the process of downloading, including
// updates to the history system and providing the information for displaying
// the downloads view in the Destinations tab. There is one DownloadManager per
// active browser context in Chrome.
//
// Download observers:
// Objects that are interested in notifications about new downloads, or progress
// updates for a given download must implement one of the download observer
// interfaces:
//   DownloadManager::Observer:
//     - allows observers, primarily views, to be notified when changes to the
//       set of all downloads (such as new downloads, or deletes) occur
// Use AddObserver() / RemoveObserver() on the appropriate download object to
// receive state updates.
//
// Download state persistence:
// The DownloadManager uses the history service for storing persistent
// information about the state of all downloads. The history system maintains a
// separate table for this called 'downloads'. At the point that the
// DownloadManager is constructed, we query the history service for the state of
// all persisted downloads.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "content/browser/browser_thread.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_status_updater_delegate.h"
#include "content/common/content_export.h"
#include "net/base/net_errors.h"

class DownloadFileManager;
class DownloadManagerDelegate;
class DownloadStatusUpdater;
class GURL;
class ResourceDispatcherHost;
class TabContents;
struct DownloadCreateInfo;
struct DownloadSaveInfo;

namespace content {
class BrowserContext;
}

// Browser's download manager: manages all downloads and destination view.
class CONTENT_EXPORT DownloadManager
    : public base::RefCountedThreadSafe<DownloadManager,
                                        BrowserThread::DeleteOnUIThread>,
      public DownloadStatusUpdaterDelegate {
 public:
  DownloadManager(DownloadManagerDelegate* delegate,
                  DownloadStatusUpdater* status_updater);

  // Shutdown the download manager. Must be called before destruction.
  void Shutdown();

  // Interface to implement for observers that wish to be informed of changes
  // to the DownloadManager's collection of downloads.
  class CONTENT_EXPORT Observer {
   public:
    // New or deleted download, observers should query us for the current set
    // of downloads.
    virtual void ModelChanged() = 0;

    // Called when the DownloadManager is being destroyed to prevent Observers
    // from calling back to a stale pointer.
    virtual void ManagerGoingDown() {}

    // Called immediately after the DownloadManager puts up a select file
    // dialog.
    // |id| indicates which download opened the dialog.
    virtual void SelectFileDialogDisplayed(int32 id) {}

   protected:
    virtual ~Observer() {}
  };

  typedef std::vector<DownloadItem*> DownloadVector;

  // Return all temporary downloads that reside in the specified directory.
  void GetTemporaryDownloads(const FilePath& dir_path, DownloadVector* result);

  // Return all non-temporary downloads in the specified directory that are
  // are in progress or have completed.
  void GetAllDownloads(const FilePath& dir_path, DownloadVector* result);

  // Returns all non-temporary downloads matching |query|. Empty query matches
  // everything.
  void SearchDownloads(const string16& query, DownloadVector* result);

  // Returns true if initialized properly.
  bool Init(content::BrowserContext* browser_context);

  // Notifications sent from the download thread to the UI thread
  void StartDownload(int32 id);
  void UpdateDownload(int32 download_id, int64 size);

  // |download_id| is the ID of the download.
  // |size| is the number of bytes that have been downloaded.
  // |hash| is sha256 hash for the downloaded file. It is empty when the hash
  // is not available.
  void OnResponseCompleted(int32 download_id, int64 size,
                           const std::string& hash);

  // Called when there is an error in the download.
  // |download_id| is the ID of the download.
  // |size| is the number of bytes that are currently downloaded.
  // |error| is a download error code.  Indicates what caused the interruption.
  void OnDownloadError(int32 download_id, int64 size, net::Error error);

  // This routine is called from the DownloadItem when a
  // request is cancelled or interrupted.  It removes the download
  // from all internal queues holding in-progress work, and takes care
  // of the off-thread aspects of the cancel (stopping the request,
  // cancelling the download on the file thread).
  void DownloadStopped(DownloadItem* download);

  // Called from DownloadItem when the download is being removed.
  // Takes care of all history operations, modifying internal queues,
  // and notifying DownloadManager observers, and actually deletes
  // the DownloadItem.
  void RemoveDownload(DownloadItem* download);

  // Determine if the download is ready for completion, i.e. has had
  // all data saved, and completed the filename determination and
  // history insertion.
  bool IsDownloadReadyForCompletion(DownloadItem* download);

  // If all pre-requisites have been met, complete download processing, i.e.
  // do internal cleanup, file rename, and potentially auto-open.
  // (Dangerous downloads still may block on user acceptance after this
  // point.)
  void MaybeCompleteDownload(DownloadItem* download);

  // Called when the download is renamed to its final name.
  // |uniquifier| is a number used to make unique names for the file.  It is
  // only valid for the DANGEROUS_BUT_VALIDATED state of the download item.
  void OnDownloadRenamedToFinalName(int download_id,
                                    const FilePath& full_path,
                                    int uniquifier);

  // Remove downloads after remove_begin (inclusive) and before remove_end
  // (exclusive). You may pass in null Time values to do an unbounded delete
  // in either direction.
  int RemoveDownloadsBetween(const base::Time remove_begin,
                             const base::Time remove_end);

  // Remove downloads will delete all downloads that have a timestamp that is
  // the same or more recent than |remove_begin|. The number of downloads
  // deleted is returned back to the caller.
  int RemoveDownloads(const base::Time remove_begin);

  // Remove all downloads will delete all downloads. The number of downloads
  // deleted is returned back to the caller.
  int RemoveAllDownloads();

  // Final download manager transition for download: Update the download
  // history and remove the download from |active_downloads_|.
  void DownloadCompleted(int32 download_id);

  // Download the object at the URL. Used in cases such as "Save Link As..."
  void DownloadUrl(const GURL& url,
                   const GURL& referrer,
                   const std::string& referrer_encoding,
                   TabContents* tab_contents);

  // Download the object at the URL and save it to the specified path. The
  // download is treated as the temporary download and thus will not appear
  // in the download history. Used in cases such as drag and drop.
  void DownloadUrlToFile(const GURL& url,
                         const GURL& referrer,
                         const std::string& referrer_encoding,
                         const DownloadSaveInfo& save_info,
                         TabContents* tab_contents);

  // Allow objects to observe the download creation process.
  void AddObserver(Observer* observer);

  // Remove a download observer from ourself.
  void RemoveObserver(Observer* observer);

  // Called by the embedder, after creating the download manager, to let it know
  // about downloads from previous runs of the browser.
  void OnPersistentStoreQueryComplete(
      std::vector<DownloadPersistentStoreInfo>* entries);

  // Called by the embedder, in response to
  // DownloadManagerDelegate::AddItemToPersistentStore.
  void OnItemAddedToPersistentStore(int32 download_id, int64 db_handle);

  // Display a new download in the appropriate browser UI.
  void ShowDownloadInBrowser(DownloadItem* download);

  // The number of in progress (including paused) downloads.
  int in_progress_count() const {
    return static_cast<int>(in_progress_.size());
  }

  content::BrowserContext* browser_context() { return browser_context_; }

  FilePath last_download_path() { return last_download_path_; }

  // Creates the download item.  Must be called on the UI thread.
  void CreateDownloadItem(DownloadCreateInfo* info);

  // Clears the last download path, used to initialize "save as" dialogs.
  void ClearLastDownloadPath();

  // Overridden from DownloadStatusUpdaterDelegate:
  virtual bool IsDownloadProgressKnown();
  virtual int64 GetInProgressDownloadCount();
  virtual int64 GetReceivedDownloadBytes();
  virtual int64 GetTotalDownloadBytes();

  // Called by the delegate after the save as dialog is closed.
  void FileSelected(const FilePath& path, void* params);
  void FileSelectionCanceled(void* params);

  // Called by the delegate if it delayed the download in
  // DownloadManagerDelegate::ShouldStartDownload and now is ready.
  void RestartDownload(int32 download_id);

  // Checks whether downloaded files still exist. Updates state of downloads
  // that refer to removed files. The check runs in the background and may
  // finish asynchronously after this method returns.
  void CheckForHistoryFilesRemoval();

  // Checks whether a downloaded file still exists and updates the file's state
  // if the file is already removed. The check runs in the background and may
  // finish asynchronously after this method returns.
  void CheckForFileRemoval(DownloadItem* download_item);

  // Assert the named download item is on the correct queues
  // in the DownloadManager.  For debugging.
  void AssertQueueStateConsistent(DownloadItem* download);

  // Get the download item from the history map.  Useful after the item has
  // been removed from the active map, or was retrieved from the history DB.
  DownloadItem* GetDownloadItem(int id);

  // Called when Save Page download starts. Transfers ownership of |download|
  // to the DownloadManager.
  void SavePageDownloadStarted(DownloadItem* download);

  // Called when Save Page download is done.
  void SavePageDownloadFinished(DownloadItem* download);

  // Get the download item from the active map.  Useful when the item is not
  // yet in the history map.  Returns NULL if no such active download.
  DownloadItem* GetActiveDownloadItem(int id);

  DownloadManagerDelegate* delegate() const { return delegate_; }

 private:
  typedef std::set<DownloadItem*> DownloadSet;
  typedef base::hash_map<int64, DownloadItem*> DownloadMap;

  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class DeleteTask<DownloadManager>;
  friend class base::RefCountedThreadSafe<DownloadManager,
                                          BrowserThread::DeleteOnUIThread>;

  // For testing.
  friend class DownloadManagerTest;
  friend class MockDownloadManager;
  friend class DownloadTest;

  void set_delegate(DownloadManagerDelegate* delegate) { delegate_ = delegate; }

  virtual ~DownloadManager();

  // Called on the FILE thread to check the existence of a downloaded file.
  void CheckForFileRemovalOnFileThread(int64 db_handle, const FilePath& path);

  // Called on the UI thread if the FILE thread detects the removal of
  // the downloaded file. The UI thread updates the state of the file
  // and then notifies this update to the file's observer.
  void OnFileRemovalDetected(int64 db_handle);

  // Called back after a target path for the file to be downloaded to has been
  // determined, either automatically based on the suggested file name, or by
  // the user in a Save As dialog box.
  void ContinueDownloadWithPath(DownloadItem* download,
                                const FilePath& chosen_file);

  // Retrieves the download from the |download_id|.
  // Returns NULL if the download is not active.
  DownloadItem* GetActiveDownload(int32 download_id);

  // Updates the delegate about the overall download progress.
  void UpdateDownloadProgress();

  // Inform observers that the model has changed.
  void NotifyModelChanged();

  // Return all in progress downloads.  This includes downloads that
  // have not yet been entered into the history (all other accessors
  // only return downloads that have been entered into the history).
  // This is intended to be used for testing only.
  void GetInProgressDownloads(std::vector<DownloadItem*>* result);

  // Debugging routine to confirm relationship between below
  // containers; no-op if NDEBUG.
  void AssertContainersConsistent() const;

  // Add a DownloadItem to history_downloads_.
  void AddDownloadItemToHistory(DownloadItem* item, int64 db_handle);

  // Remove from internal maps.
  int RemoveDownloadItems(const DownloadVector& pending_deletes);

  // Called when a download entry is committed to the persistent store.
  void OnDownloadItemAddedToPersistentStore(int32 download_id, int64 db_handle);

  // Called when Save Page As entry is commited to the persistent store.
  void OnSavePageItemAddedToPersistentStore(int32 download_id, int64 db_handle);

  // |downloads_| is the owning set for all downloads known to the
  // DownloadManager.  This includes downloads started by the user in
  // this session, downloads initialized from the history system, and
  // "save page as" downloads.  All other DownloadItem containers in
  // the DownloadManager are maps; they do not own the DownloadItems.
  // Note that this is the only place (with any functional implications;
  // see save_page_downloads_ below) that "save page as" downloads are
  // kept, as the DownloadManager's only job is to hold onto those
  // until destruction.
  //
  // |history_downloads_| is map of all downloads in this browser context. The
  // key is the handle returned by the history system, which is unique across
  // sessions.
  //
  // |active_downloads_| is a map of all downloads that are currently being
  // processed. The key is the ID assigned by the DownloadFileManager,
  // which is unique for the current session.
  //
  // |in_progress_| is a map of all downloads that are in progress and that have
  // not yet received a valid history handle. The key is the ID assigned by the
  // DownloadFileManager, which is unique for the current session.
  //
  // |save_page_downloads_| (if defined) is a collection of all the
  // downloads the "save page as" system has given to us to hold onto
  // until we are destroyed. They key is DownloadFileManager, so it is unique
  // compared to download item. It is only used for debugging.
  //
  // When a download is created through a user action, the corresponding
  // DownloadItem* is placed in |active_downloads_| and remains there until the
  // download is in a terminal state (COMPLETE or CANCELLED).  It is also
  // placed in |in_progress_| and remains there until it has received a
  // valid handle from the history system. Once it has a valid handle, the
  // DownloadItem* is placed in the |history_downloads_| map.  When the
  // download reaches a terminal state, it is removed from |in_progress_|.
  // Downloads from past sessions read from a persisted state from the
  // history system are placed directly into |history_downloads_| since
  // they have valid handles in the history system.

  DownloadSet downloads_;
  DownloadMap history_downloads_;
  DownloadMap in_progress_;
  DownloadMap active_downloads_;
  DownloadMap save_page_downloads_;

  // True if the download manager has been initialized and requires a shutdown.
  bool shutdown_needed_;

  // Observers that want to be notified of changes to the set of downloads.
  ObserverList<Observer> observers_;

  // The current active browser context.
  content::BrowserContext* browser_context_;

  // Non-owning pointer for handling file writing on the download_thread_.
  DownloadFileManager* file_manager_;

  // Non-owning pointer for updating the download status.
  base::WeakPtr<DownloadStatusUpdater> status_updater_;

  // The user's last choice for download directory. This is only used when the
  // user wants us to prompt for a save location for each download.
  FilePath last_download_path_;

  // Allows an embedder to control behavior. Guaranteed to outlive this object.
  DownloadManagerDelegate* delegate_;

  // TODO(rdsmith): Remove when http://crbug.com/84508 is fixed.
  // For debugging only.
  int64 largest_db_handle_in_history_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManager);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_H_
