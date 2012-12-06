// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_UPLOADER_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_UPLOADER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/google_apis/drive_upload_error.h"
#include "chrome/browser/google_apis/drive_upload_mode.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "net/base/file_stream.h"
#include "net/base/completion_callback.h"
#include "net/base/file_stream.h"

class GURL;

namespace content {
class DownloadItem;
}

namespace google_apis {
class DriveServiceInterface;
struct ResumeUploadResponse;

// Callback to be invoked once the upload has completed.
typedef base::Callback<void(
    DriveUploadError error,
    const FilePath& drive_path,
    const FilePath& file_path,
    scoped_ptr<DocumentEntry> document_entry)>
    UploadCompletionCallback;

// Callback to be invoked once the uploader is ready to upload.
typedef base::Callback<void(int32 upload_id)> UploaderReadyCallback;

class DriveUploaderInterface {
 public:
  virtual ~DriveUploaderInterface() {}

  // Uploads a new file to a directory specified by |upload_location|.
  // Returns the upload_id.
  //
  // upload_location:
  //   the "resumable-create-media" URL of the destination directory.
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
  // content_length:
  //   The content length of the file to be uploaded.
  //   If the length is unknown beforehand, -1 should be passed.
  //
  // file_size:
  //  The current size of the file to be uploaded. This can be smaller than
  //  |content_length|, if the source file is still being written (i.e. being
  //  downdloaded from some web site). The client should keep providing the
  //  current status with the  UpdateUpload() function.
  //
  // completion_callback
  //   Called when an upload is done regardless of it was successful or not.
  //   Must not be null.
  //
  // |ready_callback| is called when the uploader is ready to upload data
  // (i.e. when the file is opened). May be null.
  virtual int UploadNewFile(
      const GURL& upload_location,
      const FilePath& drive_file_path,
      const FilePath& local_file_path,
      const std::string& title,
      const std::string& content_type,
      int64 content_length,
      int64 file_size,
      const UploadCompletionCallback& completion_callback,
      const UploaderReadyCallback& ready_callback) = 0;

  // Stream data to an existing file.
  // Returns the upload_id.
  //
  // See comments at UploadNewFile() about common parameters.
  virtual int StreamExistingFile(
      const GURL& upload_location,
      const FilePath& drive_file_path,
      const FilePath& local_file_path,
      const std::string& content_type,
      int64 content_length,
      int64 file_size,
      const UploadCompletionCallback& completion_callback,
      const UploaderReadyCallback& ready_callback) = 0;

  // Uploads an existing file (a file that already exists on Drive).
  //
  // See comments at UploadNewFile() about common parameters.
  virtual int UploadExistingFile(
      const GURL& upload_location,
      const FilePath& drive_file_path,
      const FilePath& local_file_path,
      const std::string& content_type,
      int64 file_size,
      const UploadCompletionCallback& completion_callback) = 0;

  // Updates attributes of streaming upload from |download|. This function
  // should be used when downloading from some web site and uploading to
  // Drive are done in parallel.
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
      const GURL& upload_location,
      const FilePath& drive_file_path,
      const FilePath& local_file_path,
      const std::string& title,
      const std::string& content_type,
      int64 content_length,
      int64 file_size,
      const UploadCompletionCallback& completion_callback,
      const UploaderReadyCallback& ready_callback) OVERRIDE;
  virtual int StreamExistingFile(
      const GURL& upload_location,
      const FilePath& drive_file_path,
      const FilePath& local_file_path,
      const std::string& content_type,
      int64 content_length,
      int64 file_size,
      const UploadCompletionCallback& completion_callback,
      const UploaderReadyCallback& ready_callback) OVERRIDE;
  virtual int UploadExistingFile(
      const GURL& upload_location,
      const FilePath& drive_file_path,
      const FilePath& local_file_path,
      const std::string& content_type,
      int64 file_size,
      const UploadCompletionCallback& completion_callback) OVERRIDE;
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
    // TODO(kinaba): We should switch to async API of FileStream once
    // crbug.com/164312 is fixed.
    //
    // For opening and reading from physical file.
    // Every file operation is posted to the sequenced worker pool, while the
    // ownership of |file_stream| is held by DriveUploader in UI thread. At the
    // point when DriveUploader deletes UploadFileInfo, it passes the ownership
    // of the stream to sequenced worker pool.
    scoped_ptr<net::FileStream> file_stream;
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

    // Callback to be invoked once the uploader is ready to upload.
    UploaderReadyCallback ready_callback;

    // Callback to be invoked once the upload has finished.
    UploadCompletionCallback completion_callback;
  };

  // Indicates step in which we try to open a file.
  // Retrying happens in FILE_OPEN_UPDATE_UPLOAD step.
  enum FileOpenType {
    FILE_OPEN_START_UPLOAD,
    FILE_OPEN_UPDATE_UPLOAD
  };

  // Lookup UploadFileInfo* in pending_uploads_.
  UploadFileInfo* GetUploadFileInfo(int upload_id) const;

  // Open the file.
  void OpenFile(UploadFileInfo* upload_file_info, FileOpenType open_type);

  // net::FileStream::Open completion callback. The result of the file
  // open operation is passed as |result|.
  void OpenCompletionCallback(FileOpenType open_type,
                              int upload_id,
                              int result);

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
  void OnResumeUploadResponseReceived(
      int upload_id,
      const ResumeUploadResponse& response,
      scoped_ptr<DocumentEntry> entry);

  // Initiate the upload.
  void InitiateUpload(UploadFileInfo* uploader_file_info);

  // Handle failed uploads.
  void UploadFailed(UploadFileInfo* upload_file_info,
                    DriveUploadError error);

  // Removes |upload_file_info| from UploadFileInfoMap |pending_uploads_|.
  // After its removal from the map, |upload_file_info| is deleted.
  void RemoveUpload(scoped_ptr<UploadFileInfo> upload_file_info);

  // Starts uploading a file with |upload_file_info|. Returns a new upload
  // ID assigned to |upload_file_info|. |upload_file_info| is added to
  // |pending_uploads_map_|.
  int StartUploadFile(scoped_ptr<UploadFileInfo> upload_file_info);

  // Pointers to DriveServiceInterface object owned by DriveSystemService.
  // The lifetime of this object is guaranteed to exceed that of the
  // DriveUploader instance.
  DriveServiceInterface* drive_service_;

  int next_upload_id_;  // id counter.

  typedef std::map<int, UploadFileInfo*> UploadFileInfoMap;
  // Upload file infos added to the map are deleted either in |RemoveUpload| or
  // in DriveUploader dtor (i.e. we can assume |this| takes their ownership).
  UploadFileInfoMap pending_uploads_;

  // TaskRunner for opening, reading, and closing files.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveUploader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveUploader);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_UPLOADER_H_
