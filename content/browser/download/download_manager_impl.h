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
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_manager.h"

namespace net {
class BoundNetLog;
}

namespace content {
class DownloadFileFactory;
class DownloadItemFactory;
class DownloadItemImpl;

class CONTENT_EXPORT DownloadManagerImpl : public DownloadManager,
                                           private DownloadItemImplDelegate {
 public:
  // Caller guarantees that |net_log| will remain valid
  // for the lifetime of DownloadManagerImpl (until Shutdown() is called).
  DownloadManagerImpl(net::NetLog* net_log);

  // Implementation functions (not part of the DownloadManager interface).

  // Creates a download item for the SavePackage system.
  // Must be called on the UI thread.  Note that the DownloadManager
  // retains ownership.
  virtual DownloadItemImpl* CreateSavePackageDownloadItem(
      const FilePath& main_file_path,
      const GURL& page_url,
      const std::string& mime_type,
      DownloadItem::Observer* observer);

  // DownloadManager functions.
  virtual void SetDelegate(DownloadManagerDelegate* delegate) OVERRIDE;
  virtual DownloadManagerDelegate* GetDelegate() const OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual void GetAllDownloads(DownloadVector* result) OVERRIDE;
  virtual bool Init(BrowserContext* browser_context) OVERRIDE;
  virtual DownloadItem* StartDownload(
      scoped_ptr<DownloadCreateInfo> info,
      scoped_ptr<ByteStreamReader> stream) OVERRIDE;
  virtual void CancelDownload(int32 download_id) OVERRIDE;
  virtual int RemoveDownloadsBetween(base::Time remove_begin,
                                     base::Time remove_end) OVERRIDE;
  virtual int RemoveDownloads(base::Time remove_begin) OVERRIDE;
  virtual int RemoveAllDownloads() OVERRIDE;
  virtual void DownloadUrl(scoped_ptr<DownloadUrlParameters> params) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual content::DownloadItem* CreateDownloadItem(
      const FilePath& path,
      const GURL& url,
      const GURL& referrer_url,
      const base::Time& start_time,
      const base::Time& end_time,
      int64 received_bytes,
      int64 total_bytes,
      content::DownloadItem::DownloadState state,
      bool opened) OVERRIDE;
  virtual int InProgressCount() const OVERRIDE;
  virtual BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual void CheckForHistoryFilesRemoval() OVERRIDE;
  virtual DownloadItem* GetDownload(int id) OVERRIDE;

  // For testing; specifically, accessed from TestFileErrorInjector.
  void SetDownloadItemFactoryForTesting(
      scoped_ptr<DownloadItemFactory> item_factory);
  void SetDownloadFileFactoryForTesting(
      scoped_ptr<DownloadFileFactory> file_factory);
  virtual DownloadFileFactory* GetDownloadFileFactoryForTesting();

 private:
  typedef std::set<DownloadItem*> DownloadSet;
  typedef base::hash_map<int32, DownloadItemImpl*> DownloadMap;
  typedef std::vector<DownloadItemImpl*> DownloadItemImplVector;

  // For testing.
  friend class DownloadManagerTest;
  friend class DownloadTest;

  friend class base::RefCountedThreadSafe<DownloadManagerImpl>;

  virtual ~DownloadManagerImpl();

  // Creates the download item.  Must be called on the UI thread.
  virtual DownloadItemImpl* CreateDownloadItem(
      DownloadCreateInfo* info, const net::BoundNetLog& bound_net_log);

  // Get next download id.
  DownloadId GetNextId();

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

  // Debugging routine to confirm relationship between below
  // containers; no-op if NDEBUG.
  void AssertContainersConsistent() const;

  // Remove from internal maps.
  int RemoveDownloadItems(const DownloadItemImplVector& pending_deletes);

  // Overridden from DownloadItemImplDelegate
  // (Note that |GetBrowserContext| are present in both interfaces.)
  virtual void DetermineDownloadTarget(
      DownloadItemImpl* item, const DownloadTargetCallback& callback) OVERRIDE;
  virtual void ReadyForDownloadCompletion(
      DownloadItemImpl* item, const base::Closure& complete_callback) OVERRIDE;
  virtual bool ShouldOpenFileBasedOnExtension(const FilePath& path) OVERRIDE;
  virtual bool ShouldOpenDownload(
      DownloadItemImpl* item,
      const ShouldOpenDownloadCallback& callback) OVERRIDE;
  virtual void CheckForFileRemoval(DownloadItemImpl* download_item) OVERRIDE;
  virtual void DownloadStopped(DownloadItemImpl* download) OVERRIDE;
  virtual void DownloadCompleted(DownloadItemImpl* download) OVERRIDE;
  virtual void DownloadOpened(DownloadItemImpl* download) OVERRIDE;
  virtual void DownloadRemoved(DownloadItemImpl* download) OVERRIDE;
  virtual void ShowDownloadInBrowser(DownloadItemImpl* download) OVERRIDE;
  virtual void AssertStateConsistent(DownloadItemImpl* download) const OVERRIDE;

  // Factory for creation of downloads items.
  scoped_ptr<DownloadItemFactory> item_factory_;

  // Factory for the creation of download files.
  scoped_ptr<DownloadFileFactory> file_factory_;

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
  // processed.
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
  BrowserContext* browser_context_;

  // Allows an embedder to control behavior. Guaranteed to outlive this object.
  DownloadManagerDelegate* delegate_;

  net::NetLog* net_log_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_
