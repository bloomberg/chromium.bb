// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_

#include <map>
#include <set>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/lock.h"
#include "content/browser/download/download_item_factory.h"
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_manager.h"

class DownloadFileManager;
class DownloadItemImpl;

class CONTENT_EXPORT DownloadManagerImpl
    : public content::DownloadManager,
      private DownloadItemImplDelegate {
 public:
  // Caller guarantees that |file_manager| and |net_log| will remain valid
  // for the lifetime of DownloadManagerImpl (until Shutdown() is called).
  // |factory| may be a default constructed (null) scoped_ptr; if so,
  // the DownloadManagerImpl creates and takes ownership of the
  // default DownloadItemFactory.
  DownloadManagerImpl(DownloadFileManager* file_manager,
                      scoped_ptr<content::DownloadItemFactory> factory,
                      net::NetLog* net_log);

  // Implementation functions (not part of the DownloadManager interface).

  // Creates a download item for the SavePackage system.
  // Must be called on the UI thread.  Note that the DownloadManager
  // retains ownership.
  virtual DownloadItemImpl* CreateSavePackageDownloadItem(
      const FilePath& main_file_path,
      const GURL& page_url,
      const std::string& mime_type,
      content::DownloadItem::Observer* observer);

  // content::DownloadManager functions.
  virtual void SetDelegate(content::DownloadManagerDelegate* delegate) OVERRIDE;
  virtual content::DownloadManagerDelegate* GetDelegate() const OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual void GetAllDownloads(DownloadVector* result) OVERRIDE;
  virtual void SearchDownloads(const string16& query,
                               DownloadVector* result) OVERRIDE;
  virtual bool Init(content::BrowserContext* browser_context) OVERRIDE;
  virtual content::DownloadId StartDownload(
      scoped_ptr<DownloadCreateInfo> info,
      scoped_ptr<content::ByteStreamReader> stream) OVERRIDE;
  virtual void UpdateDownload(int32 download_id,
                              int64 bytes_so_far,
                              int64 bytes_per_sec,
                              const std::string& hash_state) OVERRIDE;
  virtual void OnResponseCompleted(int32 download_id, int64 size,
                                   const std::string& hash) OVERRIDE;
  virtual void CancelDownload(int32 download_id) OVERRIDE;
  virtual void OnDownloadInterrupted(
      int32 download_id,
      content::DownloadInterruptReason reason) OVERRIDE;
  virtual int RemoveDownloadsBetween(base::Time remove_begin,
                                     base::Time remove_end) OVERRIDE;
  virtual int RemoveDownloads(base::Time remove_begin) OVERRIDE;
  virtual int RemoveAllDownloads() OVERRIDE;
  virtual void DownloadUrl(
      scoped_ptr<content::DownloadUrlParameters> params) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual void OnPersistentStoreQueryComplete(
      std::vector<content::DownloadPersistentStoreInfo>* entries) OVERRIDE;
  virtual void OnItemAddedToPersistentStore(int32 download_id,
                                            int64 db_handle) OVERRIDE;
  virtual int InProgressCount() const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual void CheckForHistoryFilesRemoval() OVERRIDE;
  virtual content::DownloadItem* GetDownloadItem(int id) OVERRIDE;
  virtual content::DownloadItem* GetDownload(int id) OVERRIDE;
  virtual void SavePageDownloadFinished(
      content::DownloadItem* download) OVERRIDE;
  virtual content::DownloadItem* GetActiveDownloadItem(int id) OVERRIDE;

 private:
  typedef std::set<content::DownloadItem*> DownloadSet;
  typedef base::hash_map<int32, DownloadItemImpl*> DownloadMap;
  typedef std::vector<DownloadItemImpl*> DownloadItemImplVector;

  // For testing.
  friend class DownloadManagerTest;
  friend class DownloadTest;

  friend class base::RefCountedThreadSafe<DownloadManagerImpl>;

  virtual ~DownloadManagerImpl();

  // Creates the download item.  Must be called on the UI thread.
  // Returns the |BoundNetLog| used by the |DownloadItem|.
  virtual net::BoundNetLog CreateDownloadItem(DownloadCreateInfo* info);

  // Does nothing if |download_id| is not an active download.
  void MaybeCompleteDownloadById(int download_id);

  // Determine if the download is ready for completion, i.e. has had
  // all data saved, and completed the filename determination and
  // history insertion.
  bool IsDownloadReadyForCompletion(DownloadItemImpl* download);

  // Show the download in the browser.
  void ShowDownloadInBrowser(DownloadItemImpl* download);

  // Get next download id.
  content::DownloadId GetNextId();

  // Called on the FILE thread to check the existence of a downloaded file.
  void CheckForFileRemovalOnFileThread(int32 download_id, const FilePath& path);

  // Called on the UI thread if the FILE thread detects the removal of
  // the downloaded file. The UI thread updates the state of the file
  // and then notifies this update to the file's observer.
  void OnFileRemovalDetected(int32 download_id);

  // Removes |download| from the active and in progress maps.
  // Called when the download is cancelled or has an error.
  // Does nothing if the download is not in the history DB.
  void RemoveFromActiveList(DownloadItemImpl* download);

  // Inform observers that the model has changed.
  void NotifyModelChanged();

  // Debugging routine to confirm relationship between below
  // containers; no-op if NDEBUG.
  void AssertContainersConsistent() const;

  // Add a DownloadItem to history_downloads_.
  void AddDownloadItemToHistory(DownloadItemImpl* item, int64 db_handle);

  // Remove from internal maps.
  int RemoveDownloadItems(const DownloadItemImplVector& pending_deletes);

  // Called in response to our request to the DownloadFileManager to
  // create a DownloadFile.  A |reason| of
  // content::DOWNLOAD_INTERRUPT_REASON_NONE indicates success.
  void OnDownloadFileCreated(
      int32 download_id, content::DownloadInterruptReason reason);

  // Called when the delegate has completed determining the download target.
  // Arguments following |download_id| are as per
  // content::DownloadTargetCallback.
  void OnDownloadTargetDetermined(
      int32 download_id,
      const FilePath& target_path,
      content::DownloadItem::TargetDisposition disposition,
      content::DownloadDangerType danger_type,
      const FilePath& intermediate_path);

  // Called when a download entry is committed to the persistent store.
  void OnDownloadItemAddedToPersistentStore(DownloadItemImpl* item);

  // Called when Save Page As entry is committed to the persistent store.
  void OnSavePageItemAddedToPersistentStore(DownloadItemImpl* item);

  // Overridden from DownloadItemImplDelegate
  // (Note that |GetBrowserContext| are present in both interfaces.)
  virtual DownloadFileManager* GetDownloadFileManager() OVERRIDE;
  virtual bool ShouldOpenDownload(DownloadItemImpl* item) OVERRIDE;
  virtual bool ShouldOpenFileBasedOnExtension(const FilePath& path) OVERRIDE;
  virtual void CheckForFileRemoval(DownloadItemImpl* download_item) OVERRIDE;
  virtual void MaybeCompleteDownload(DownloadItemImpl* download) OVERRIDE;
  virtual void DownloadStopped(DownloadItemImpl* download) OVERRIDE;
  virtual void DownloadCompleted(DownloadItemImpl* download) OVERRIDE;
  virtual void DownloadOpened(DownloadItemImpl* download) OVERRIDE;
  virtual void DownloadRemoved(DownloadItemImpl* download) OVERRIDE;
  virtual void DownloadRenamedToIntermediateName(
      DownloadItemImpl* download) OVERRIDE;
  virtual void DownloadRenamedToFinalName(DownloadItemImpl* download) OVERRIDE;
  virtual void AssertStateConsistent(DownloadItemImpl* download) const OVERRIDE;

  // Factory for creation of downloads items.
  scoped_ptr<content::DownloadItemFactory> factory_;

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
  // |active_downloads_| is a map of all downloads that are currently being
  // processed. The key is the ID assigned by the DownloadFileManager,
  // which is unique for the current session.
  //
  // When a download is created through a user action, the corresponding
  // DownloadItem* is placed in |active_downloads_| and remains there until the
  // download is in a terminal state (COMPLETE or CANCELLED).  Once it has a
  // valid handle, the DownloadItem* is placed in the |history_downloads_| map.
  // Downloads from past sessions read from a persisted state from the history
  // system are placed directly into |history_downloads_| since they have valid
  // handles in the history system.

  DownloadMap downloads_;
  DownloadMap active_downloads_;

  int history_size_;

  // True if the download manager has been initialized and requires a shutdown.
  bool shutdown_needed_;

  // Observers that want to be notified of changes to the set of downloads.
  ObserverList<Observer> observers_;

  // The current active browser context.
  content::BrowserContext* browser_context_;

  // Non-owning pointer for handling file writing on the download_thread_.
  DownloadFileManager* file_manager_;

  // Allows an embedder to control behavior. Guaranteed to outlive this object.
  content::DownloadManagerDelegate* delegate_;

  net::NetLog* net_log_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerImpl);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_
