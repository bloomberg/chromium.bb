// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UPLOADER_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/gdata/gdata.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

namespace gdata {

class GDataUploader : public content::DownloadManager::Observer,
                      public content::DownloadItem::Observer {
 public:
  explicit GDataUploader(DocumentsService* docs_service);
  virtual ~GDataUploader();

  // Initializes GDataUploader. Become an observer of |profile|'s
  // DownloadManager.
  void Initialize(Profile* profile);

 private:
  // Uploads a file with given |title|, |content_type|
  // and |file_size| from |file_path|.
  void UploadFile(const FilePath& file_path,
                  const std::string& content_type,
                  int64 file_size);

  // net::FileStream::Open completion callback.
  //
  // Called when an file to be uploaded is opened. The result of the file
  // open operation is passed as |result|.
  void OpenCompletionCallback(const FilePath& file_path,
                              const UploadFileInfo& upload_file_info,
                              int result);

  // DocumentsService callback for InitiateUpload.
  void OnUploadLocationReceived(GDataErrorCode code,
      const UploadFileInfo& upload_file_info,
      const GURL& upload_location);

  // Uploads the next chunk of data from the file.
  void UploadNextChunk(GDataErrorCode code,
      int64 start_range_received,
      int64 end_range_received,
      UploadFileInfo* upload_file_info);

  // net::FileStream::Read completion callback.
  void ReadCompletionCallback(const UploadFileInfo& upload_file_info,
      int64 bytes_to_read,
      int bytes_read);

  // DocumentsService callback for ResumeUpload.
  void OnResumeUploadResponseReceived(GDataErrorCode code,
      const UploadFileInfo& upload_file_info,
      int64 start_range_received,
      int64 end_range_received);

  // DownloadManager overrides.
  virtual void ModelChanged(content::DownloadManager* manager) OVERRIDE;

  // DownloadItem overrides.
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE {}

  // Adds/Removes |download| to pending_downloads_. Also start/stop
  // observing |download|.
  void AddPendingDownload(content::DownloadItem* download);
  void RemovePendingDownload(content::DownloadItem* download);

  DocumentsService* docs_service_;

  typedef std::set<content::DownloadItem*> DownloadSet;
  DownloadSet pending_downloads_;

  DISALLOW_COPY_AND_ASSIGN(GDataUploader);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UPLOADER_H_
