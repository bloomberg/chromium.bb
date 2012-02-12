// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UPLOADER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/gdata/gdata.h"

namespace gdata {

class GDataUploader {
 public:
  explicit GDataUploader(DocumentsService* docs_service);
  ~GDataUploader();

  // Uploads a test file.
  void TestUpload();

 private:
  // DocumentsService callback for InitiateUpload.
  void OnUploadLocationReceived(GDataErrorCode code,
      const UploadFileInfo& in_upload_file_info,
      const GURL& upload_location);

  // Uploads the next chunk of data from the file.
  void UploadNextChunk(GDataErrorCode code,
      int64 start_range_received,
      int64 end_range_received,
      UploadFileInfo* upload_file_info);

  // DocumentsService callback for ResumeUpload.
  void OnResumeUploadResponseReceived(GDataErrorCode code,
      const UploadFileInfo& in_upload_file_info,
      int64 start_range_received,
      int64 end_range_received);

  DocumentsService* docs_service_;

  DISALLOW_COPY_AND_ASSIGN(GDataUploader);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UPLOADER_H_
