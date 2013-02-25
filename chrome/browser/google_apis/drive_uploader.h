// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_UPLOADER_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_UPLOADER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/google_apis/drive_upload_error.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"

class GURL;

namespace base {
class FilePath;
}

namespace google_apis {
class DriveServiceInterface;
struct UploadRangeResponse;

// Callback to be invoked once the upload has completed.
typedef base::Callback<void(DriveUploadError error,
                            const base::FilePath& drive_path,
                            const base::FilePath& file_path,
                            scoped_ptr<ResourceEntry> resource_entry)>
    UploadCompletionCallback;

class DriveUploaderInterface {
 public:
  virtual ~DriveUploaderInterface() {}

  // Uploads a new file to a directory specified by |upload_location|.
  // Returns the upload_id.
  //
  // parent_resource_id:
  //   resource id of the destination directory.
  //
  // drive_file_path:
  //   The destination path like "drive/foo/bar.txt".
  //
  // local_file_path:
  //   The path to the local file to be uploaded.
  //
  // title:
  //   The title (file name) of the file to be uploaded.
  //
  // content_type:
  //   The content type of the file to be uploaded.
  //
  // callback:
  //   Called when an upload is done regardless of it was successful or not.
  //   Must not be null.
  virtual void UploadNewFile(const std::string& parent_resource_id,
                             const base::FilePath& drive_file_path,
                             const base::FilePath& local_file_path,
                             const std::string& title,
                             const std::string& content_type,
                             const UploadCompletionCallback& callback) = 0;

  // Uploads an existing file (a file that already exists on Drive).
  //
  // See comments at UploadNewFile() about common parameters.
  //
  // resource_id:
  //   resource id of the existing file to be overwritten.
  //
  // etag:
  //   Expected ETag for the destination file. If it does not match, the upload
  //   fails with UPLOAD_ERROR_CONFLICT.
  //   If |etag| is empty, the test is skipped.
  virtual void UploadExistingFile(const std::string& resource_id,
                                  const base::FilePath& drive_file_path,
                                  const base::FilePath& local_file_path,
                                  const std::string& content_type,
                                  const std::string& etag,
                                  const UploadCompletionCallback& callback) = 0;
};

class DriveUploader : public DriveUploaderInterface {
 public:
  explicit DriveUploader(DriveServiceInterface* drive_service);
  virtual ~DriveUploader();

  // DriveUploaderInterface overrides.
  virtual void UploadNewFile(const std::string& parent_resource_id,
                             const base::FilePath& drive_file_path,
                             const base::FilePath& local_file_path,
                             const std::string& title,
                             const std::string& content_type,
                             const UploadCompletionCallback& callback) OVERRIDE;
  virtual void UploadExistingFile(
      const std::string& resource_id,
      const base::FilePath& drive_file_path,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const std::string& etag,
      const UploadCompletionCallback& callback) OVERRIDE;

 private:
  struct UploadFileInfo;
  typedef base::Callback<void(scoped_ptr<UploadFileInfo> upload_file_info)>
      StartInitiateUploadCallback;

  // Starts uploading a file with |upload_file_info|.
  void StartUploadFile(
      scoped_ptr<UploadFileInfo> upload_file_info,
      const StartInitiateUploadCallback& start_initiate_upload_callback);

  // net::FileStream::Open completion callback. The result of the file open
  // operation is passed as |result|, and the size is stored in |file_size|.
  void OpenCompletionCallback(
      scoped_ptr<UploadFileInfo> upload_file_info,
      const StartInitiateUploadCallback& start_initiate_upload_callback,
      int64 file_size);

  // Starts to initate the new file uploading.
  // Upon completion, OnUploadLocationReceived should be called.
  void StartInitiateUploadNewFile(
      const std::string& parent_resource_id,
      const std::string& title,
      scoped_ptr<UploadFileInfo> upload_file_info);

  // Starts to initate the existing file uploading.
  // Upon completion, OnUploadLocationReceived should be called.
  void StartInitiateUploadExistingFile(
      const std::string& resource_id,
      const std::string& etag,
      scoped_ptr<UploadFileInfo> upload_file_info);

  // DriveService callback for InitiateUpload.
  void OnUploadLocationReceived(scoped_ptr<UploadFileInfo> upload_file_info,
                                GDataErrorCode code,
                                const GURL& upload_location);

  // Uploads the next chunk of data from the file.
  void UploadNextChunk(scoped_ptr<UploadFileInfo> upload_file_info);

  // net::FileStream::Read completion callback.
  void ReadCompletionCallback(scoped_ptr<UploadFileInfo> upload_file_info,
                              int bytes_to_read,
                              int bytes_read);

  // Calls DriveService's ResumeUpload with the current upload info.
  void ResumeUpload(scoped_ptr<UploadFileInfo> upload_file_info,
                    int bytes_to_send);

  // DriveService callback for ResumeUpload.
  void OnUploadRangeResponseReceived(
      scoped_ptr<UploadFileInfo> upload_file_info,
      const UploadRangeResponse& response,
      scoped_ptr<ResourceEntry> entry);

  // Handle failed uploads.
  void UploadFailed(scoped_ptr<UploadFileInfo> upload_file_info,
                    DriveUploadError error);

  // Pointers to DriveServiceInterface object owned by DriveSystemService.
  // The lifetime of this object is guaranteed to exceed that of the
  // DriveUploader instance.
  DriveServiceInterface* drive_service_;

  // TaskRunner for opening, reading, and closing files.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveUploader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveUploader);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_UPLOADER_H_
