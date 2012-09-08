// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_MOCK_DRIVE_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_MOCK_DRIVE_UPLOADER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/gdata/drive_uploader.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gdata {

class MockDriveUploader : public DriveUploaderInterface {
 public:
  MockDriveUploader();
  virtual ~MockDriveUploader();
  // This function is not mockable by gmock.
  virtual int UploadNewFile(
      scoped_ptr<UploadFileInfo> upload_file_info) OVERRIDE {
    const int kUploadId = 123;
    return kUploadId;
  }

  // This function is not mockable by gmock.
  virtual int StreamExistingFile(
      scoped_ptr<UploadFileInfo> upload_file_info) OVERRIDE { return 0; }

  MOCK_METHOD6(UploadExistingFile,
               int(const GURL& upload_location,
               const FilePath& drive_file_path,
               const FilePath& local_file_path,
               int64 file_size,
               const std::string& content_type,
               const UploadFileInfo::UploadCompletionCallback& callback));

  MOCK_METHOD2(UpdateUpload, void(int upload_id,
                                  content::DownloadItem* download));
  MOCK_CONST_METHOD1(GetUploadedBytes, int64(int upload_id));
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_MOCK_DRIVE_UPLOADER_H_
