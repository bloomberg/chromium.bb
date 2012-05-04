// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/message_loop_helpers.h"
#include "base/time.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_log.h"
#include "net/base/net_errors.h"

class DownloadRequestHandle;
class GURL;
struct DownloadCreateInfo;
struct DownloadRetrieveInfo;

namespace content {
class BrowserContext;
class DownloadManagerDelegate;
class DownloadQuery;
class WebContents;
struct DownloadSaveInfo;

// Browser's download manager: manages all downloads and destination view.
class CONTENT_EXPORT DownloadManager
    : public base::RefCountedThreadSafe<DownloadManager> {
 public:
  // NOTE: If there is an error, the DownloadId will be invalid.
  typedef base::Callback<void(DownloadId, net::Error)> OnStartedCallback;

  virtual ~DownloadManager() {}

  static DownloadManager* Create(
      DownloadManagerDelegate* delegate,
      net::NetLog* net_log);

  // A method that can be used in tests to ensure that all the internal download
  // classes have no pending downloads.
  static bool EnsureNoPendingDownloadsForTesting();

  // Shutdown the download manager. Must be called before destruction.
  virtual void Shutdown() = 0;

  // Interface to implement for observers that wish to be informed of changes
  // to the DownloadManager's collection of downloads.
  class CONTENT_EXPORT Observer {
   public:
    // New or deleted download, observers should query us for the current set
    // of downloads.
    virtual void ModelChanged(DownloadManager* manager) = 0;

    // Called when the DownloadManager is being destroyed to prevent Observers
    // from calling back to a stale pointer.
    virtual void ManagerGoingDown(DownloadManager* manager) {}

    // Called immediately after the DownloadManager puts up a select file
    // dialog.
    // |id| indicates which download opened the dialog.
    virtual void SelectFileDialogDisplayed(
        DownloadManager* manager, int32 id) {}

   protected:
    virtual ~Observer() {}
  };

  typedef std::vector<DownloadItem*> DownloadVector;

  // If |dir_path| is empty, appends all temporary downloads to |*result|.
  // Otherwise, appends all temporary downloads that reside in |dir_path| to
  // |*result|.
  virtual void GetTemporaryDownloads(const FilePath& dir_path,
                                     DownloadVector* result) = 0;

  // If |dir_path| is empty, appends all non-temporary downloads to |*result|.
  // Otherwise, appends all non-temporary downloads that reside in |dir_path|
  // to |*result|.
  virtual void GetAllDownloads(const FilePath& dir_path,
                               DownloadVector* result) = 0;

  // Returns all non-temporary downloads matching |query|. Empty query matches
  // everything.
  virtual void SearchDownloads(const string16& query,
                               DownloadVector* result) = 0;

  // Returns true if initialized properly.
  virtual bool Init(BrowserContext* browser_context) = 0;

  // Notifications sent from the download thread to the UI thread
  virtual void StartDownload(int32 id) = 0;
  virtual void UpdateDownload(int32 download_id,
                              int64 bytes_so_far,
                              int64 bytes_per_sec,
                              const std::string& hash_state) = 0;

  // |download_id| is the ID of the download.
  // |size| is the number of bytes that have been downloaded.
  // |hash| is sha256 hash for the downloaded file. It is empty when the hash
  // is not available.
  virtual void OnResponseCompleted(int32 download_id, int64 size,
                           const std::string& hash) = 0;

  // Offthread target for cancelling a particular download.  Will be a no-op
  // if the download has already been cancelled.
  virtual void CancelDownload(int32 download_id) = 0;

  // Called when there is an error in the download.
  // |download_id| is the ID of the download.
  // |size| is the number of bytes that are currently downloaded.
  // |hash_state| is the current state of the hash of the data that has been
  // downloaded.
  // |reason| is a download interrupt reason code.
  virtual void OnDownloadInterrupted(
      int32 download_id,
      int64 size,
      const std::string& hash_state,
      DownloadInterruptReason reason) = 0;

  // Called when the download is renamed to its final name.
  // |uniquifier| is a number used to make unique names for the file.  It is
  // only valid for the DANGEROUS_BUT_VALIDATED state of the download item.
  virtual void OnDownloadRenamedToFinalName(int download_id,
                                    const FilePath& full_path,
                                    int uniquifier) = 0;

  // Remove downloads after remove_begin (inclusive) and before remove_end
  // (exclusive). You may pass in null Time values to do an unbounded delete
  // in either direction.
  virtual int RemoveDownloadsBetween(base::Time remove_begin,
                                     base::Time remove_end) = 0;

  // Remove downloads will delete all downloads that have a timestamp that is
  // the same or more recent than |remove_begin|. The number of downloads
  // deleted is returned back to the caller.
  virtual int RemoveDownloads(base::Time remove_begin) = 0;

  // Remove all downloads will delete all downloads. The number of downloads
  // deleted is returned back to the caller.
  virtual int RemoveAllDownloads() = 0;

  // Downloads the content at |url|. |referrer| and |referrer_encoding| are the
  // referrer for the download, and may be empty. If |prefer_cache| is true,
  // then if the response to |url| is in the HTTP cache it will be used without
  // revalidation. If |post_id| is non-negative, then it identifies the post
  // transaction used to originally retrieve the |url| resource - it also
  // requires |prefer_cache| to be |true| since re-post'ing is not done.
  // |save_info| specifies where the downloaded file should be
  // saved, and whether the user should be prompted about the download.
  // |web_contents| is the web page that the download is done in context of,
  // and must be non-NULL.
  // |callback| will be called when the download starts, or if an error
  // occurs that prevents a download item from being created.
  virtual void DownloadUrl(const GURL& url,
                           const GURL& referrer,
                           const std::string& referrer_encoding,
                           bool prefer_cache,
                           int64 post_id,
                           const DownloadSaveInfo& save_info,
                           WebContents* web_contents,
                           const OnStartedCallback& callback) = 0;

  // Allow objects to observe the download creation process.
  virtual void AddObserver(Observer* observer) = 0;

  // Remove a download observer from ourself.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Called by the embedder, after creating the download manager, to let it know
  // about downloads from previous runs of the browser.
  virtual void OnPersistentStoreQueryComplete(
      std::vector<DownloadPersistentStoreInfo>* entries) = 0;

  // Called by the embedder, in response to
  // DownloadManagerDelegate::AddItemToPersistentStore.
  virtual void OnItemAddedToPersistentStore(int32 download_id,
                                            int64 db_handle) = 0;

  // The number of in progress (including paused) downloads.
  virtual int InProgressCount() const = 0;

  virtual BrowserContext* GetBrowserContext() const = 0;

  virtual FilePath LastDownloadPath() = 0;

  // Creates the download item.  Must be called on the UI thread.
  // Returns the |BoundNetLog| used by the |DownloadItem|.
  virtual net::BoundNetLog CreateDownloadItem(
      DownloadCreateInfo* info,
      const DownloadRequestHandle& request_handle) = 0;

  // Creates a download item for the SavePackage system.
  // Must be called on the UI thread.  Note that the DownloadManager
  // retains ownership.
  virtual DownloadItem* CreateSavePackageDownloadItem(
      const FilePath& main_file_path,
      const GURL& page_url,
      bool is_otr,
      const std::string& mime_type,
      DownloadItem::Observer* observer) = 0;

  // Clears the last download path, used to initialize "save as" dialogs.
  virtual void ClearLastDownloadPath() = 0;

  // Called by the delegate after the save as dialog is closed.
  virtual void FileSelected(const FilePath& path, int32 download_id) = 0;
  virtual void FileSelectionCanceled(int32 download_id) = 0;

  // Called by the delegate if it delayed the download in
  // DownloadManagerDelegate::ShouldStartDownload and now is ready.
  virtual void RestartDownload(int32 download_id) = 0;

  // Checks whether downloaded files still exist. Updates state of downloads
  // that refer to removed files. The check runs in the background and may
  // finish asynchronously after this method returns.
  virtual void CheckForHistoryFilesRemoval() = 0;

  // Get the download item from the history map.  Useful after the item has
  // been removed from the active map, or was retrieved from the history DB.
  virtual DownloadItem* GetDownloadItem(int id) = 0;

  // Called when Save Page download is done.
  virtual void SavePageDownloadFinished(DownloadItem* download) = 0;

  // Get the download item from the active map.  Useful when the item is not
  // yet in the history map.
  virtual DownloadItem* GetActiveDownloadItem(int id) = 0;

  virtual bool GenerateFileHash() = 0;

  virtual DownloadManagerDelegate* delegate() const = 0;

  // For testing only.  May be called from tests indirectly (through
  // other for testing only methods).
  virtual void SetDownloadManagerDelegate(
      DownloadManagerDelegate* delegate) = 0;

 private:
  friend class base::RefCountedThreadSafe<
      DownloadManager, BrowserThread::DeleteOnUIThread>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<DownloadManager>;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_H_
