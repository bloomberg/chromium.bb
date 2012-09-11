// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_UPLOADER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/gdata/drive_upload_file_info.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class GURL;

namespace content {
class DownloadItem;
}

namespace gdata {

class DriveServiceInterface;
struct ResumeUploadResponse;

class DriveUploaderInterface {
 public:
  virtual ~DriveUploaderInterface() {}

  // Uploads a new file specified by |upload_file_info|. Transfers ownership.
  // Returns the upload_id.
  //
  // WARNING: This is not mockable by gmock because it takes scoped_ptr<>.
  // See "Announcing scoped_ptr<>::Pass(). The latest in pointer ownership
  // technology!" thread on chromium-dev.
  virtual int UploadNewFile(scoped_ptr<UploadFileInfo> upload_file_info) = 0;

  // Stream data to an existing file from data specified by |upload_file_info|.
  // Transfers ownership. Returns the upload_id.
  // TODO(zork): Do not use UploadFileInfo to pass data.  See: crbug.com/134819
  virtual int StreamExistingFile(
      scoped_ptr<UploadFileInfo> upload_file_info) = 0;

  // Uploads an existing file (a file that already exists on Drive)
  // specified by |local_file_path|. The existing file on Drive will be
  // updated. Returns the upload_id. |callback| is run upon completion or
  // failure.
  virtual int UploadExistingFile(
      const GURL& upload_location,
      const FilePath& drive_file_path,
      const FilePath& local_file_path,
      int64 file_size,
      const std::string& content_type,
      const UploadFileInfo::UploadCompletionCallback& callback) = 0;

  // Updates attributes of streaming upload.
  virtual void UpdateUpload(int upload_id,
                            content::DownloadItem* download) = 0;

  // Returns the count of bytes confirmed as uploaded so far.
  virtual int64 GetUploadedBytes(int upload_id) const = 0;
};

class DriveUploader : public DriveUploaderInterface {
 public:
  explicit DriveUploader(DriveServiceInterface* drive_service);
  virtual ~DriveUploader();

  // DriveUploaderInterface overrides.
  virtual int UploadNewFile(
      scoped_ptr<UploadFileInfo> upload_file_info) OVERRIDE;
  virtual int StreamExistingFile(
      scoped_ptr<UploadFileInfo> upload_file_info) OVERRIDE;
  virtual int UploadExistingFile(
      const GURL& upload_location,
      const FilePath& drive_file_path,
      const FilePath& local_file_path,
      int64 file_size,
      const std::string& content_type,
      const UploadFileInfo::UploadCompletionCallback& callback) OVERRIDE;
  virtual void UpdateUpload(
      int upload_id, content::DownloadItem* download) OVERRIDE;
  virtual int64 GetUploadedBytes(int upload_id) const OVERRIDE;

 private:
  // Lookup UploadFileInfo* in pending_uploads_.
  UploadFileInfo* GetUploadFileInfo(int upload_id) const;

  // Open the file.
  void OpenFile(UploadFileInfo* upload_file_info);

  // net::FileStream::Open completion callback. The result of the file
  // open operation is passed as |result|.
  void OpenCompletionCallback(int upload_id, int result);

  // DriveService callback for InitiateUpload.
  void OnUploadLocationReceived(int upload_id,
                                GDataErrorCode code,
                                const GURL& upload_location);

  // Uploads the next chunk of data from the file.
  void UploadNextChunk(UploadFileInfo* upload_file_info);

  // net::FileStream::Read completion callback.
  void ReadCompletionCallback(int upload_id,
      int bytes_to_read,
      int bytes_read);

  // Calls DriveService's ResumeUpload with the current upload info.
  void ResumeUpload(int upload_id);

  // DriveService callback for ResumeUpload.
  void OnResumeUploadResponseReceived(int upload_id,
                                      const ResumeUploadResponse& response,
                                      scoped_ptr<DocumentEntry> entry);

  // Initiate the upload.
  void InitiateUpload(UploadFileInfo* uploader_file_info);

  // Handle failed uploads.
  void UploadFailed(scoped_ptr<UploadFileInfo> upload_file_info,
                    DriveFileError error);

  // Removes |upload_id| from UploadFileInfoMap |pending_uploads_|.
  // Note that this does not delete the UploadFileInfo object itself,
  // because it may still be in use by an asynchronous function.
  void RemoveUpload(int upload_id);

  // Starts uploading a file with |upload_file_info|. Returns a new upload
  // ID assigned to |upload_file_info|.
  int StartUploadFile(scoped_ptr<UploadFileInfo> upload_file_info);

  // Pointers to DriveServiceInterface object owned by DriveSystemService.
  // The lifetime of this object is guaranteed to exceed that of the
  // DriveUploader instance.
  DriveServiceInterface* drive_service_;

  int next_upload_id_;  // id counter.

  typedef std::map<int, UploadFileInfo*> UploadFileInfoMap;
  UploadFileInfoMap pending_uploads_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveUploader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveUploader);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_UPLOADER_H_
