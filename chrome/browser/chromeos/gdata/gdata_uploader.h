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
#include "base/platform_file.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"
#include "chrome/browser/chromeos/gdata/gdata_params.h"
#include "googleurl/src/gurl.h"

namespace content {
class DownloadItem;
}

namespace gdata {

class GDataFileSystem;
struct UploadFileInfo;

class GDataUploader {
 public:
  explicit GDataUploader(GDataFileSystem* file_system);
  virtual ~GDataUploader();

  // Uploads a file specified by |upload_file_info|. Transfers ownership.
  // Returns the upload_id.
  int UploadFile(scoped_ptr<UploadFileInfo> upload_file_info);

  // Updates attributes of streaming upload.
  void UpdateUpload(int upload_id, content::DownloadItem* download);

  // Returns the count of bytes confirmed as uploaded so far.
  int64 GetUploadedBytes(int upload_id) const;

  // TODO(achuith): Make this private.
  // Destroys |upload_file_info|.
  void DeleteUpload(UploadFileInfo* upload_file_info);

 private:
  // Lookup UploadFileInfo* in pending_uploads_.
  UploadFileInfo* GetUploadFileInfo(int upload_id) const;

  // Open the file.
  void OpenFile(UploadFileInfo* upload_file_info);

  // net::FileStream::Open completion callback. The result of the file
  // open operation is passed as |result|.
  void OpenCompletionCallback(int upload_id, int result);

  // DocumentsService callback for InitiateUpload.
  void OnUploadLocationReceived(int upload_id,
                                GDataErrorCode code,
                                const GURL& upload_location);

  // Uploads the next chunk of data from the file.
  void UploadNextChunk(UploadFileInfo* upload_file_info);

  // net::FileStream::Read completion callback.
  void ReadCompletionCallback(int upload_id,
      int bytes_to_read,
      int bytes_read);

  // DocumentsService callback for ResumeUpload.
  void OnResumeUploadResponseReceived(int upload_id,
                                      const ResumeUploadResponse& response,
                                      scoped_ptr<DocumentEntry> entry);

  // When upload completes, move the file into the gdata cache.
  void MoveFileToCache(UploadFileInfo* upload_file_info);

  // Handle failed uploads.
  void UploadFailed(UploadFileInfo* upload_file_info,
                    base::PlatformFileError error);

  // Private data.
  GDataFileSystem* file_system_;

  int next_upload_id_;  // id counter.

  typedef std::map<int, UploadFileInfo*> UploadFileInfoMap;
  UploadFileInfoMap pending_uploads_;

  // Factory for various callbacks.
  base::WeakPtrFactory<GDataUploader> uploader_factory_;

  DISALLOW_COPY_AND_ASSIGN(GDataUploader);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UPLOADER_H_
