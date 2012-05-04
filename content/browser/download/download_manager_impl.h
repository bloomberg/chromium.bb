// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_
#pragma once

#include <map>
#include <set>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_helpers.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "content/browser/download/download_item_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_manager.h"

class CONTENT_EXPORT DownloadManagerImpl
    : public content::DownloadManager,
      public DownloadItemImpl::Delegate {
 public:
  DownloadManagerImpl(content::DownloadManagerDelegate* delegate,
                      net::NetLog* net_log);

  // content::DownloadManager functions.
  virtual void Shutdown() OVERRIDE;
  virtual void GetTemporaryDownloads(const FilePath& dir_path,
                                     DownloadVector* result) OVERRIDE;
  virtual void GetAllDownloads(const FilePath& dir_path,
                               DownloadVector* result) OVERRIDE;
  virtual void SearchDownloads(const string16& query,
                               DownloadVector* result) OVERRIDE;
  virtual bool Init(content::BrowserContext* browser_context) OVERRIDE;
  virtual void StartDownload(int32 id) OVERRIDE;
  virtual void UpdateDownload(int32 download_id,
                              int64 bytes_so_far,
                              int64 bytes_per_sec,
                              const std::string& hash_state) OVERRIDE;
  virtual void OnResponseCompleted(int32 download_id, int64 size,
                                   const std::string& hash) OVERRIDE;
  virtual void CancelDownload(int32 download_id) OVERRIDE;
  virtual void OnDownloadInterrupted(
      int32 download_id,
      int64 size,
      const std::string& hash_state,
      content::DownloadInterruptReason reason) OVERRIDE;
  virtual void OnDownloadRenamedToFinalName(int download_id,
                                            const FilePath& full_path,
                                            int uniquifier) OVERRIDE;
  virtual int RemoveDownloadsBetween(base::Time remove_begin,
                                     base::Time remove_end) OVERRIDE;
  virtual int RemoveDownloads(base::Time remove_begin) OVERRIDE;
  virtual int RemoveAllDownloads() OVERRIDE;
  virtual void DownloadUrl(const GURL& url,
                           const GURL& referrer,
                           const std::string& referrer_encoding,
                           bool prefer_cache,
                           int64 post_id,
                           const content::DownloadSaveInfo& save_info,
                           content::WebContents* web_contents,
                           const OnStartedCallback& callback) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual void OnPersistentStoreQueryComplete(
      std::vector<content::DownloadPersistentStoreInfo>* entries) OVERRIDE;
  virtual void OnItemAddedToPersistentStore(int32 download_id,
                                            int64 db_handle) OVERRIDE;
  virtual int InProgressCount() const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual FilePath LastDownloadPath() OVERRIDE;
  virtual net::BoundNetLog CreateDownloadItem(
      DownloadCreateInfo* info,
      const DownloadRequestHandle& request_handle) OVERRIDE;
  virtual content::DownloadItem* CreateSavePackageDownloadItem(
      const FilePath& main_file_path,
      const GURL& page_url,
      bool is_otr,
      const std::string& mime_type,
      content::DownloadItem::Observer* observer) OVERRIDE;
  virtual void ClearLastDownloadPath() OVERRIDE;
  virtual void FileSelected(const FilePath& path, int32 download_id) OVERRIDE;
  virtual void FileSelectionCanceled(int32 download_id) OVERRIDE;
  virtual void RestartDownload(int32 download_id) OVERRIDE;
  virtual void CheckForHistoryFilesRemoval() OVERRIDE;
  virtual content::DownloadItem* GetDownloadItem(int id) OVERRIDE;
  virtual void SavePageDownloadFinished(
      content::DownloadItem* download) OVERRIDE;
  virtual content::DownloadItem* GetActiveDownloadItem(int id) OVERRIDE;
  virtual bool GenerateFileHash() OVERRIDE;
  virtual content::DownloadManagerDelegate* delegate() const OVERRIDE;
  virtual void SetDownloadManagerDelegate(
      content::DownloadManagerDelegate* delegate) OVERRIDE;

  // Overridden from DownloadItemImpl::Delegate
  // (Note that |GetBrowserContext| are present in both interfaces.)
  virtual bool ShouldOpenDownload(content::DownloadItem* item) OVERRIDE;
  virtual bool ShouldOpenFileBasedOnExtension(
      const FilePath& path) OVERRIDE;
  virtual void CheckForFileRemoval(
      content::DownloadItem* download_item) OVERRIDE;
  virtual void MaybeCompleteDownload(
      content::DownloadItem* download) OVERRIDE;
  virtual void DownloadCancelled(
      content::DownloadItem* download) OVERRIDE;
  virtual void DownloadCompleted(
      content::DownloadItem* download) OVERRIDE;
  virtual void DownloadOpened(
      content::DownloadItem* download) OVERRIDE;
  virtual void DownloadRemoved(content::DownloadItem* download) OVERRIDE;
  virtual void AssertStateConsistent(
      content::DownloadItem* download) const OVERRIDE;

  // For unit tests only.
  void SetFileManagerForTesting(DownloadFileManager* file_manager);

 private:
  typedef std::set<content::DownloadItem*> DownloadSet;
  typedef base::hash_map<int64, content::DownloadItem*> DownloadMap;

  // For testing.
  friend class DownloadManagerTest;
  friend class DownloadTest;

  friend class base::RefCountedThreadSafe<DownloadManagerImpl>;

  virtual ~DownloadManagerImpl();

  // Determine if the download is ready for completion, i.e. has had
  // all data saved, and completed the filename determination and
  // history insertion.
  bool IsDownloadReadyForCompletion(content::DownloadItem* download);

  // Show the download in the browser.
  void ShowDownloadInBrowser(content::DownloadItem* download);

  // Get next download id.
  content::DownloadId GetNextId();

  // Called on the FILE thread to check the existence of a downloaded file.
  void CheckForFileRemovalOnFileThread(int64 db_handle, const FilePath& path);

  // Called on the UI thread if the FILE thread detects the removal of
  // the downloaded file. The UI thread updates the state of the file
  // and then notifies this update to the file's observer.
  void OnFileRemovalDetected(int64 db_handle);

  // Called back after a target path for the file to be downloaded to has been
  // determined, either automatically based on the suggested file name, or by
  // the user in a Save As dialog box.
  void ContinueDownloadWithPath(content::DownloadItem* download,
                                const FilePath& chosen_file);

  // Retrieves the download from the |download_id|.
  // Returns NULL if the download is not active.
  content::DownloadItem* GetActiveDownload(int32 download_id);

  // Removes |download| from the active and in progress maps.
  // Called when the download is cancelled or has an error.
  // Does nothing if the download is not in the history DB.
  void RemoveFromActiveList(content::DownloadItem* download);

  // Inform observers that the model has changed.
  void NotifyModelChanged();

  // Debugging routine to confirm relationship between below
  // containers; no-op if NDEBUG.
  void AssertContainersConsistent() const;

  // Add a DownloadItem to history_downloads_.
  void AddDownloadItemToHistory(content::DownloadItem* item, int64 db_handle);

  // Remove from internal maps.
  int RemoveDownloadItems(const DownloadVector& pending_deletes);

  // Called when a download entry is committed to the persistent store.
  void OnDownloadItemAddedToPersistentStore(int32 download_id, int64 db_handle);

  // Called when Save Page As entry is committed to the persistent store.
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

  // The user's last choice for download directory. This is only used when the
  // user wants us to prompt for a save location for each download.
  FilePath last_download_path_;

  // Allows an embedder to control behavior. Guaranteed to outlive this object.
  content::DownloadManagerDelegate* delegate_;

  net::NetLog* net_log_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerImpl);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_
