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
#include "chrome/browser/chromeos/gdata/gdata_params.h"
#include "googleurl/src/gurl.h"

namespace gdata {

class DocumentsService;
class GDataFileSystem;
struct UploadFileInfo;

class GDataUploader {
 public:
  explicit GDataUploader(GDataFileSystem* file_system);
  virtual ~GDataUploader();

  // Uploads a file specified by |upload_file_info|. Transfers ownership.
  void UploadFile(UploadFileInfo* upload_file_info);

  // Updates size and download_completed of streaming upload.
  void UpdateUpload(const GURL& file_url,
                    int64 file_size,
                    bool download_complete);

 private:
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
  void UploadNextChunk(UploadFileInfo* upload_file_info);

  // net::FileStream::Read completion callback.
  void ReadCompletionCallback(const GURL& file_url,
      int bytes_to_read,
      int bytes_read);

  // DocumentsService callback for ResumeUpload.
  void OnResumeUploadResponseReceived(const GURL& file_url,
                                      const ResumeUploadResponse& response);

  // Private data.

  GDataFileSystem* file_system_;

  typedef std::map<GURL, UploadFileInfo*> UploadFileInfoMap;
  UploadFileInfoMap pending_uploads_;

  // Factory for various callbacks.
  base::WeakPtrFactory<GDataUploader> uploader_factory_;

  DISALLOW_COPY_AND_ASSIGN(GDataUploader);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UPLOADER_H_
