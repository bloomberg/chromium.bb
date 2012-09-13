// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_UPLOADER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/gdata/drive_upload_mode.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "net/base/file_stream.h"

class GURL;

namespace content {
class DownloadItem;
}

namespace gdata {

class MockDriveUploader;
class DriveServiceInterface;
struct ResumeUploadResponse;

// Callback to be invoked once the upload has completed.
typedef base::Callback<void(DriveFileError error,
    const FilePath& drive_path,
    const FilePath& file_path,
    scoped_ptr<DocumentEntry> document_entry)> UploadCompletionCallback;

class DriveUploaderInterface {
 public:
  virtual ~DriveUploaderInterface() {}

  // Uploads a new file.
  // Returns the upload_id.
  virtual int UploadNewFile(
      const GURL& upload_location,
      const FilePath& drive_file_path,
      const FilePath& local_file_path,
      const std::string& title,
      const std::string& content_type,
      int64 content_length,
      int64 file_size,
      const UploadCompletionCallback& callback) = 0;

  // Stream data to an existing file.
  // Transfers ownership. Returns the upload_id.
  virtual int StreamExistingFile(
      const GURL& upload_location,
      const FilePath& drive_file_path,
      const FilePath& local_file_path,
      const std::string& content_type,
      int64 content_length,
      int64 file_size,
      const UploadCompletionCallback& callback) = 0;

  // Uploads an existing file (a file that already exists on Drive).
  virtual int UploadExistingFile(
      const GURL& upload_location,
      const FilePath& drive_file_path,
      const FilePath& local_file_path,
      const std::string& content_type,
      int64 file_size,
      const UploadCompletionCallback& callback) = 0;

  // Updates attributes of streaming upload.
  virtual void UpdateUpload(int upload_id,
                            content::DownloadItem* download) = 0;

  // Returns the count of bytes confirmed as uploaded so far.
  virtual int64 GetUploadedBytes(int upload_id) const = 0;
};

class DriveUploader : public DriveUploaderInterface {
  friend class MockDriveUploader;
 public:
  explicit DriveUploader(DriveServiceInterface* drive_service);
  virtual ~DriveUploader();

  // DriveUploaderInterface overrides.
  virtual int UploadNewFile(
      const GURL& upload_location,
      const FilePath& drive_file_path,
      const FilePath& local_file_path,
      const std::string& title,
      const std::string& content_type,
      int64 content_length,
      int64 file_size,
      const UploadCompletionCallback& callback) OVERRIDE;
  virtual int StreamExistingFile(
      const GURL& upload_location,
      const FilePath& drive_file_path,
      const FilePath& local_file_path,
      const std::string& content_type,
      int64 content_length,
      int64 file_size,
      const UploadCompletionCallback& callback) OVERRIDE;
  virtual int UploadExistingFile(
      const GURL& upload_location,
      const FilePath& drive_file_path,
      const FilePath& local_file_path,
      const std::string& content_type,
      int64 file_size,
      const UploadCompletionCallback& callback) OVERRIDE;
  virtual void UpdateUpload(
      int upload_id, content::DownloadItem* download) OVERRIDE;
  virtual int64 GetUploadedBytes(int upload_id) const OVERRIDE;

 private:
  // Structure containing current upload information of file, passed between
  // DriveServiceInterface methods and callbacks.
  struct UploadFileInfo {
    // To be able to access UploadFileInfo from tests.
    UploadFileInfo();
    ~UploadFileInfo();

    // Bytes left to upload.
    int64 SizeRemaining() const;

    // Useful for printf debugging.
    std::string DebugString() const;

    int upload_id;  // id of this upload.
    FilePath file_path;  // The path of the file to be uploaded.
    int64 file_size;  // Last known size of the file.

    // TODO(zelirag, achuith): Make this string16.
    std::string title;  // Title to be used for file to be uploaded.
    std::string content_type;  // Content-Type of file.
    int64 content_length;  // Header content-Length.

    UploadMode upload_mode;

    // Location URL used to get |upload_location| with InitiateUpload.
    GURL initial_upload_location;

    // Location URL where file is to be uploaded to, returned from
    // InitiateUpload. Used for the subsequent ResumeUpload requests.
    GURL upload_location;

    // Final path in gdata. Looks like /special/drive/MyFolder/MyFile.
    FilePath drive_path;

    // TODO(achuith): Use generic stream object after FileStream is refactored
    // to extend a generic stream.
    //
    // For opening and reading from physical file.
    net::FileStream* file_stream;
    scoped_refptr<net::IOBuffer> buf;  // Holds current content to be uploaded.
    // Size of |buf|, max is 512KB; Google Docs requires size of each upload
    // chunk to be a multiple of 512KB.
    int64 buf_len;

    // Data to be updated by caller before sending each ResumeUpload request.
    // Note that end_range is inclusive, so start_range=0, end_range=8 is 9
    // bytes.
    //
    // Start of range of contents currently stored in |buf|.
    int64 start_range;
    int64 end_range;  // End of range of contents currently stored in |buf|.

    bool all_bytes_present;  // Whether all bytes of this file are present.
    bool upload_paused;  // Whether this file's upload has been paused.

    bool should_retry_file_open;  // Whether we should retry opening this file.
    int num_file_open_tries;  // Number of times we've tried to open this file.

    // Will be set once the upload is complete.
    scoped_ptr<DocumentEntry> entry;

    UploadCompletionCallback completion_callback;
  };

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
