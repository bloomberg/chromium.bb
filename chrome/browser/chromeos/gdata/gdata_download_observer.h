// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DOWNLOAD_OBSERVER_H__
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DOWNLOAD_OBSERVER_H__
#pragma once

#include <map>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

namespace gdata {

class DocumentEntry;
class GDataUploader;
struct UploadFileInfo;

// Observes downloads to temporary local gdata folder. Schedules these
// downloads for upload to gdata service.
class GDataDownloadObserver : public content::DownloadManager::Observer,
                              public content::DownloadItem::Observer {
 public:
  GDataDownloadObserver();
  virtual ~GDataDownloadObserver();

  // Become an observer of  DownloadManager.
  void Initialize(GDataUploader* gdata_uploader,
                  content::DownloadManager* download_manager,
                  const FilePath& gdata_tmp_download_path);

  // Sets gdata path, for example, '/special/drive/MyFolder/MyFile',
  // to external data in |download|.
  static void SetGDataPath(content::DownloadItem* download,
                           const FilePath& gdata_path);

  // Checks if there is a GData upload associated with |download|
  static bool IsGDataDownload(content::DownloadItem* download);

  // Checks if |download| is ready to complete. Returns true if |download| has
  // no GData upload associated with it or if the GData upload has already
  // completed. This method is called by the ChromeDownloadManagerDelegate to
  // check if the download is ready to complete.  If the download is not yet
  // ready to complete and |complete_callback| is not null, then
  // |complete_callback| will be called on the UI thread when the download
  // becomes ready to complete.  If this method is called multiple times with
  // the download not ready to complete, only the last |complete_callback|
  // passed to this method for |download| will be called.
  static bool IsReadyToComplete(
      content::DownloadItem* download,
      const base::Closure& complete_callback);

  // Returns the count of bytes confirmed as uploaded so far for |download|.
  static int64 GetUploadedBytes(content::DownloadItem* download);

  // Returns the progress of the upload of |download| as a percentage. If the
  // progress is unknown, returns -1.
  static int PercentComplete(content::DownloadItem* download);

  // Create a temporary file |gdata_tmp_download_path| in
  // |gdata_tmp_download_dir|. Must be called on a thread that allows file
  // operations.
  static void GetGDataTempDownloadPath(const FilePath& gdata_tmp_download_dir,
                                       FilePath* gdata_tmp_download_path);

 private:
  // Gets the gdata_path from external data in |download|.
  // GetGDataPath may return an empty path in case SetGDataPath was not
  // previously called or there was some other internal error
  // (there is a DCHECK for this).
  static FilePath GetGDataPath(content::DownloadItem* download);

  // DownloadManager overrides.
  virtual void ManagerGoingDown(content::DownloadManager* manager) OVERRIDE;
  virtual void ModelChanged(content::DownloadManager* manager) OVERRIDE;

  // DownloadItem overrides.
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE {}

  // Adds/Removes |download| to pending_downloads_.
  // Also start/stop observing |download|.
  void AddPendingDownload(content::DownloadItem* download);
  void RemovePendingDownload(content::DownloadItem* download);

  // Remove our external data and remove our observers from |download|
  void DetachFromDownload(content::DownloadItem* download);

  // Starts the upload of a downloaded/downloading file.
  void UploadDownloadItem(content::DownloadItem* download);

  // Updates metadata of ongoing upload if it exists.
  void UpdateUpload(content::DownloadItem* download);

  // Checks if this DownloadItem should be uploaded.
  bool ShouldUpload(content::DownloadItem* download);

  // Creates UploadFileInfo and initializes it using DownloadItem*.
  scoped_ptr<UploadFileInfo> CreateUploadFileInfo(
      content::DownloadItem* download);

  // Callback invoked by GDataUploader when the upload associated with
  // |download_id| has completed. |error| indicated whether the
  // call was successful. This function invokes the MaybeCompleteDownload()
  // method on the DownloadItem to allow it to complete.
  void OnUploadComplete(int32 download_id,
                        base::PlatformFileError error,
                        UploadFileInfo* upload_file_info);

  // Private data.
  // Use GDataUploader to trigger file uploads.
  GDataUploader* gdata_uploader_;
  // Observe the DownloadManager for new downloads.
  content::DownloadManager* download_manager_;

  // Temporary download location directory.
  FilePath gdata_tmp_download_path_;

  // Map of pending downloads.
  typedef std::map<int32, content::DownloadItem*> DownloadMap;
  DownloadMap pending_downloads_;

  base::WeakPtrFactory<GDataDownloadObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GDataDownloadObserver);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_DOWNLOAD_OBSERVER_H__
