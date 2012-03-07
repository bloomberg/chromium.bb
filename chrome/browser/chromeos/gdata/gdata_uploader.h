// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UPLOADER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

class Profile;

namespace gdata {

class DocumentsService;
class GDataFileSystem;
struct UploadFileInfo;

class GDataUploader : public content::DownloadManager::Observer,
                      public content::DownloadItem::Observer {
 public:
  explicit GDataUploader(GDataFileSystem* file_system);
  virtual ~GDataUploader();

  // Initializes GDataUploader. Become an observer of |profile|'s
  // DownloadManager.
  void Initialize(Profile* profile);

 private:
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

  // Start the upload of a downloaded/downloading file.
  void UploadDownloadItem(content::DownloadItem* download);

  // Update metadata of ongoing upload if it exists.
  void UpdateUpload(content::DownloadItem* download);

  // Check if this DownloadItem should be uploaded.
  bool ShouldUpload(content::DownloadItem* download);

  // Uploads a file. |file_url| is a file:// url of the downloaded file.
  void UploadFile(const GURL& file_url);

  // Lookup UploadFileInfo* in pending_uploads_.
  UploadFileInfo* GetUploadFileInfo(const GURL& file_url);

  // Destroys |upload_file_info|.
  void RemovePendingUpload(UploadFileInfo* upload_file_info);

  // net::FileStream::Open completion callback. The result of the file
  // open operation is passed as |result|.
  void OpenCompletionCallback(const GURL& file_url, int result);

  // DocumentsService callback for InitiateUpload.
  void OnUploadLocationReceived(const GURL& file_url,
                                GDataErrorCode code,
                                const GURL& upload_location);

  // Uploads the next chunk of data from the file.
  void UploadNextChunk(GDataErrorCode code,
      int64 start_range_received,
      int64 end_range_received,
      UploadFileInfo* upload_file_info);

  // net::FileStream::Read completion callback.
  void ReadCompletionCallback(const GURL& file_url,
      int bytes_to_read,
      int bytes_read);

  // DocumentsService callback for ResumeUpload.
  void OnResumeUploadResponseReceived(const GURL& file_url,
                                      GDataErrorCode code,
                                      int64 start_range_received,
                                      int64 end_range_received);

  // Private data.

  GDataFileSystem* file_system_;

  typedef std::set<content::DownloadItem*> DownloadSet;
  DownloadSet pending_downloads_;

  typedef std::map<GURL, UploadFileInfo*> UploadFileInfoMap;
  UploadFileInfoMap pending_uploads_;

  // We observe the DownloadManager for new downloads.
  content::DownloadManager* download_manager_;

  // Factory for various callbacks.
  base::WeakPtrFactory<GDataUploader> uploader_factory_;

  DISALLOW_COPY_AND_ASSIGN(GDataUploader);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UPLOADER_H_
