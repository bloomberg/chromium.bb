// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/save_page_type.h"

namespace content {

class BrowserContext;
class DownloadId;
class WebContents;

// Called by SavePackage when it creates a DownloadItem.
typedef base::Callback<void(DownloadItem*)>
    SavePackageDownloadCreatedCallback;

// Will be called asynchronously with the results of the ChooseSavePath
// operation.  If the delegate wants notification of the download item created
// in response to this operation, the SavePackageDownloadCreatedCallback will be
// non-null.
typedef base::Callback<void(const FilePath&,
                            SavePageType,
                            const SavePackageDownloadCreatedCallback&)>
    SavePackagePathPickedCallback;

// Called with the results of DetermineDownloadTarget(). If the delegate decides
// to cancel the download, then |target_path| should be set to an empty path. If
// |target_path| is non-empty, then |intermediate_path| is required to be
// non-empty and specify the path to the intermediate file (which could be the
// same as |target_path|). Both |target_path| and |intermediate_path| are
// expected to in the same directory.
typedef base::Callback<void(
    const FilePath& target_path,
    DownloadItem::TargetDisposition disposition,
    DownloadDangerType danger_type,
    const FilePath& intermediate_path)> DownloadTargetCallback;

// Browser's download manager: manages all downloads and destination view.
class CONTENT_EXPORT DownloadManagerDelegate {
 public:
  // Lets the delegate know that the download manager is shutting down.
  virtual void Shutdown() {}

  // Returns a new DownloadId.
  virtual DownloadId GetNextId();

  // Called to notify the delegate that a new download |item| requires a
  // download target to be determined. The delegate should return |true| if it
  // will determine the target information and will invoke |callback|. The
  // callback may be invoked directly (synchronously). If this function returns
  // |false|, the download manager will continue the download using a default
  // target path.
  //
  // The state of the |item| shouldn't be modified during the process of
  // filename determination save for external data (GetExternalData() /
  // SetExternalData()).
  //
  // If the download should be canceled, |callback| should be invoked with an
  // empty |target_path| argument.
  virtual bool DetermineDownloadTarget(DownloadItem* item,
                                       const DownloadTargetCallback& callback);

  // Called when the download system wants to alert a WebContents that a
  // download has started, but the TabConetnts has gone away. This lets an
  // delegate return an alternative WebContents. The delegate can return NULL.
  virtual WebContents* GetAlternativeWebContentsToNotifyForDownload();

  // Tests if a file type should be opened automatically.
  virtual bool ShouldOpenFileBasedOnExtension(const FilePath& path);

  // Allows the delegate to delay completion of the download.  This function
  // will either return true (in which case the download is ready to complete)
  // or arrange for complete_callback to be called at some point in the future
  // when the download is ready to complete.
  //
  // ShouldCompleteDownload() may be called multiple times; if it is, only the
  // last callback specified (while the delegate is delaying completion) will be
  // run.  Calls made after the callback is run are guaranteed to return true.
  virtual bool ShouldCompleteDownload(
      DownloadItem* item,
      const base::Closure& complete_callback);

  // Allows the delegate to override opening the download. If this function
  // returns false, the delegate needs to call
  // DownloadItem::DelayedDownloadOpened when it's done with the item,
  // and is responsible for opening it.  This function is called
  // after the final rename, but before the download state is set to COMPLETED.
  virtual bool ShouldOpenDownload(DownloadItem* item);

  // Returns true if we need to generate a binary hash for downloads.
  virtual bool GenerateFileHash();

  // Notifies the delegate that a new download item is created. The
  // DownloadManager waits for the delegate to add information about this
  // download to its persistent store. When the delegate is done, it calls
  // DownloadManager::OnDownloadItemAddedToPersistentStore.
  virtual void AddItemToPersistentStore(DownloadItem* item) {}

  // Notifies the delegate that information about the given download has change,
  // so that it can update its persistent store.
  // Does not update |url|, |start_time|, |total_bytes|; uses |db_handle| only
  // to select the row in the database table to update.
  virtual void UpdateItemInPersistentStore(DownloadItem* item) {}

  // Notifies the delegate that path for the download item has changed, so that
  // it can update its persistent store.
  virtual void UpdatePathForItemInPersistentStore(
      DownloadItem* item,
      const FilePath& new_path) {}

  // Notifies the delegate that it should remove the download item from its
  // persistent store.
  virtual void RemoveItemFromPersistentStore(DownloadItem* item) {}

  // Notifies the delegate to remove downloads from the given time range.
  virtual void RemoveItemsFromPersistentStoreBetween(
      base::Time remove_begin,
      base::Time remove_end) {}

  // Retrieve the directories to save html pages and downloads to.
  virtual void GetSaveDir(BrowserContext* browser_context,
                          FilePath* website_save_dir,
                          FilePath* download_save_dir,
                          bool* skip_dir_check) {}

  // Asks the user for the path to save a page. The delegate calls the callback
  // to give the answer.
  virtual void ChooseSavePath(WebContents* web_contents,
                              const FilePath& suggested_path,
                              const FilePath::StringType& default_extension,
                              bool can_save_as_complete,
                              const SavePackagePathPickedCallback& callback) {
  }

 protected:
  virtual ~DownloadManagerDelegate();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
