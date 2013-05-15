// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_uploader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/power_save_blocker.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace {

// Google Documents List API requires uploading in chunks of 512kB.
const int64 kUploadChunkSize = 512 * 1024;

// Opens |path| with |file_stream| and returns the file size.
// If failed, returns an error code in a negative value.
int64 OpenFileStreamAndGetSizeOnBlockingPool(net::FileStream* file_stream,
                                             const base::FilePath& path) {
  int result = file_stream->OpenSync(
      path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ);
  if (result != net::OK)
    return result;
  return file_stream->Available();
}

}  // namespace

namespace google_apis {

// Structure containing current upload information of file, passed between
// DriveServiceInterface methods and callbacks.
struct DriveUploader::UploadFileInfo {
  UploadFileInfo(const base::FilePath& drive_path,
                 const base::FilePath& local_path,
                 const std::string& content_type,
                 const UploadCompletionCallback& callback,
                 const ProgressCallback& progress_callback)
      : drive_path(drive_path),
        file_path(local_path),
        content_type(content_type),
        completion_callback(callback),
        progress_callback(progress_callback),
        content_length(0),
        power_save_blocker(content::PowerSaveBlocker::Create(
            content::PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
            "Upload in progress")) {
  }

  ~UploadFileInfo() {
  }

  // Useful for printf debugging.
  std::string DebugString() const {
    return "file_path=[" + file_path.AsUTF8Unsafe() +
           "], content_type=[" + content_type +
           "], content_length=[" + base::UintToString(content_length) +
           "], drive_path=[" + drive_path.AsUTF8Unsafe() +
           "]";
  }

  // Final path in gdata. Looks like /special/drive/MyFolder/MyFile.
  const base::FilePath drive_path;

  // The local file path of the file to be uploaded.
  const base::FilePath file_path;

  // Content-Type of file.
  const std::string content_type;

  // Callback to be invoked once the upload has finished.
  const UploadCompletionCallback completion_callback;

  // Callback to periodically notify the upload progress.
  const ProgressCallback progress_callback;

  // Location URL where file is to be uploaded to, returned from
  // InitiateUpload. Used for the subsequent ResumeUpload requests.
  GURL upload_location;

  // Header content-Length.
  int64 content_length;

  // Blocks system suspend while upload is in progress.
  scoped_ptr<content::PowerSaveBlocker> power_save_blocker;
};

DriveUploader::DriveUploader(DriveServiceInterface* drive_service)
    : drive_service_(drive_service),
      weak_ptr_factory_(this) {
}

DriveUploader::~DriveUploader() {}

void DriveUploader::UploadNewFile(const std::string& parent_resource_id,
                                  const base::FilePath& drive_file_path,
                                  const base::FilePath& local_file_path,
                                  const std::string& title,
                                  const std::string& content_type,
                                  const UploadCompletionCallback& callback,
                                  const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!parent_resource_id.empty());
  DCHECK(!drive_file_path.empty());
  DCHECK(!local_file_path.empty());
  DCHECK(!title.empty());
  DCHECK(!content_type.empty());
  DCHECK(!callback.is_null());

  StartUploadFile(
      scoped_ptr<UploadFileInfo>(new UploadFileInfo(drive_file_path,
                                                    local_file_path,
                                                    content_type,
                                                    callback,
                                                    progress_callback)),
      base::Bind(&DriveUploader::StartInitiateUploadNewFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 parent_resource_id,
                 title));
}

void DriveUploader::UploadExistingFile(
    const std::string& resource_id,
    const base::FilePath& drive_file_path,
    const base::FilePath& local_file_path,
    const std::string& content_type,
    const std::string& etag,
    const UploadCompletionCallback& callback,
    const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!resource_id.empty());
  DCHECK(!drive_file_path.empty());
  DCHECK(!local_file_path.empty());
  DCHECK(!content_type.empty());
  DCHECK(!callback.is_null());

  StartUploadFile(
      scoped_ptr<UploadFileInfo>(new UploadFileInfo(drive_file_path,
                                                    local_file_path,
                                                    content_type,
                                                    callback,
                                                    progress_callback)),
      base::Bind(&DriveUploader::StartInitiateUploadExistingFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 resource_id,
                 etag));
}

void DriveUploader::StartUploadFile(
    scoped_ptr<UploadFileInfo> upload_file_info,
    const StartInitiateUploadCallback& start_initiate_upload_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "Uploading file: " << upload_file_info->DebugString();

  // Passing a raw net::FileStream* to the blocking pool is safe, because it is
  // owned by |upload_file_info| in the reply callback.
  UploadFileInfo* info_ptr = upload_file_info.get();
  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&file_util::GetFileSize, info_ptr->file_path,
                 &info_ptr->content_length),
      base::Bind(&DriveUploader::StartUploadFileAfterGetFileSize,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&upload_file_info),
                 start_initiate_upload_callback));
}

void DriveUploader::StartUploadFileAfterGetFileSize(
    scoped_ptr<UploadFileInfo> upload_file_info,
    const StartInitiateUploadCallback& start_initiate_upload_callback,
    bool get_file_size_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!get_file_size_result) {
    UploadFailed(upload_file_info.Pass(), HTTP_NOT_FOUND);
    return;
  }
  DCHECK_GE(upload_file_info->content_length, 0);

  start_initiate_upload_callback.Run(upload_file_info.Pass());
}

void DriveUploader::StartInitiateUploadNewFile(
    const std::string& parent_resource_id,
    const std::string& title,
    scoped_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfo* info_ptr = upload_file_info.get();
  drive_service_->InitiateUploadNewFile(
      info_ptr->drive_path,
      info_ptr->content_type,
      info_ptr->content_length,
      parent_resource_id,
      title,
      base::Bind(&DriveUploader::OnUploadLocationReceived,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&upload_file_info)));
}

void DriveUploader::StartInitiateUploadExistingFile(
    const std::string& resource_id,
    const std::string& etag,
    scoped_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfo* info_ptr = upload_file_info.get();
  drive_service_->InitiateUploadExistingFile(
      info_ptr->drive_path,
      info_ptr->content_type,
      info_ptr->content_length,
      resource_id,
      etag,
      base::Bind(&DriveUploader::OnUploadLocationReceived,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&upload_file_info)));
}

void DriveUploader::OnUploadLocationReceived(
    scoped_ptr<UploadFileInfo> upload_file_info,
    GDataErrorCode code,
    const GURL& upload_location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "Got upload location [" << upload_location.spec()
           << "] for [" << upload_file_info->drive_path.value() << "]";

  if (code != HTTP_SUCCESS) {
    // TODO(achuith): Handle error codes from Google Docs server.
    if (code == HTTP_PRECONDITION) {
      // ETag mismatch.
      UploadFailed(upload_file_info.Pass(), HTTP_CONFLICT);
      return;
    }
    UploadFailed(upload_file_info.Pass(), code);
    return;
  }

  upload_file_info->upload_location = upload_location;

  // Start the upload from the beginning of the file.
  // PostTask is necessary because we have to finish
  // InitiateUpload's callback before calling ResumeUpload, due to the
  // implementation of OperationRegistry. (http://crbug.com/134814)
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DriveUploader::UploadNextChunk,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&upload_file_info),
                 0));  // Upload from the beginning of the file.
}

void DriveUploader::UploadNextChunk(
    scoped_ptr<UploadFileInfo> upload_file_info,
    int64 start_position) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(upload_file_info);
  DCHECK_GE(start_position, 0);
  DCHECK_LE(start_position, upload_file_info->content_length);

  UploadFileInfo* info_ptr = upload_file_info.get();
  drive_service_->ResumeUpload(
      info_ptr->drive_path,
      info_ptr->upload_location,
      start_position,
      info_ptr->content_length,
      info_ptr->content_length,
      info_ptr->content_type,
      info_ptr->file_path,
      base::Bind(&DriveUploader::OnUploadRangeResponseReceived,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&upload_file_info)),
      base::Bind(&DriveUploader::OnUploadProgress,
                 weak_ptr_factory_.GetWeakPtr(),
                 info_ptr->progress_callback,
                 start_position,
                 info_ptr->content_length));
}

void DriveUploader::OnUploadRangeResponseReceived(
    scoped_ptr<UploadFileInfo> upload_file_info,
    const UploadRangeResponse& response,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (response.code == HTTP_CREATED || response.code == HTTP_SUCCESS) {
    // When uploading a new file, we expect HTTP_CREATED, and when uploading
    // an existing file (to overwrite), we expect HTTP_SUCCESS.
    // There is an exception: if we uploading an empty file, uploading a new
    // file also returns HTTP_SUCCESS on Drive API v2. The correct way of the
    // fix should be uploading the metadata only. However, to keep the
    // compatibility with GData WAPI during the migration period, we just
    // relax the condition here.
    // TODO(hidehiko): Upload metadata only for empty files, after GData WAPI
    // code is gone.
    DVLOG(1) << "Successfully created uploaded file=["
             << upload_file_info->drive_path.value() << "]";

    // Done uploading.
    upload_file_info->completion_callback.Run(HTTP_SUCCESS,
                                              upload_file_info->drive_path,
                                              upload_file_info->file_path,
                                              entry.Pass());
    return;
  }

  // ETag mismatch.
  if (response.code == HTTP_PRECONDITION) {
    UploadFailed(upload_file_info.Pass(), HTTP_CONFLICT);
    return;
  }

  // If code is 308 (RESUME_INCOMPLETE) and |range_received| starts with 0
  // (meaning that the data is uploaded from the beginning of the file),
  // proceed to upload the next chunk.
  if (response.code != HTTP_RESUME_INCOMPLETE ||
      response.start_position_received != 0) {
    LOG(ERROR)
        << "UploadNextChunk http code=" << response.code
        << ", start_position_received=" << response.start_position_received
        << ", end_position_received=" << response.end_position_received;
    UploadFailed(
        upload_file_info.Pass(),
        response.code == HTTP_FORBIDDEN ? GDATA_NO_SPACE : response.code);
    return;
  }

  DVLOG(1) << "Received range " << response.start_position_received
           << "-" << response.end_position_received
           << " for [" << upload_file_info->drive_path.value() << "]";

  // Continue uploading the remaining chunk. The start position of the
  // remaining data is |response.end_position_received|.
  // PostTask is necessary because we have to finish previous ResumeUpload's
  // callback before calling ResumeUpload again, due to the implementation of
  // OperationRegistry. (http://crbug.com/134814)
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DriveUploader::UploadNextChunk,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&upload_file_info),
                 response.end_position_received));
}

void DriveUploader::OnUploadProgress(const ProgressCallback& callback,
                                     int64 start_position,
                                     int64 total_size,
                                     int64 progress_of_chunk,
                                     int64 total_of_chunk) {
  if (!callback.is_null())
    callback.Run(start_position + progress_of_chunk, total_size);
}

void DriveUploader::UploadFailed(scoped_ptr<UploadFileInfo> upload_file_info,
                                 GDataErrorCode error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  LOG(ERROR) << "Upload failed " << upload_file_info->DebugString();

  upload_file_info->completion_callback.Run(error,
                                            upload_file_info->drive_path,
                                            upload_file_info->file_path,
                                            scoped_ptr<ResourceEntry>());
}

}  // namespace google_apis
