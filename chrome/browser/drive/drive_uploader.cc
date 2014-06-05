// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/drive_uploader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/power_save_blocker.h"
#include "google_apis/drive/drive_api_parser.h"

using content::BrowserThread;
using google_apis::CancelCallback;
using google_apis::FileResource;
using google_apis::GDATA_CANCELLED;
using google_apis::GDataErrorCode;
using google_apis::GDATA_NO_SPACE;
using google_apis::HTTP_CONFLICT;
using google_apis::HTTP_CREATED;
using google_apis::HTTP_FORBIDDEN;
using google_apis::HTTP_NOT_FOUND;
using google_apis::HTTP_PRECONDITION;
using google_apis::HTTP_RESUME_INCOMPLETE;
using google_apis::HTTP_SUCCESS;
using google_apis::ProgressCallback;
using google_apis::UploadRangeResponse;

namespace drive {

namespace {
// Upload data is split to multiple HTTP request each conveying kUploadChunkSize
// bytes (except the request for uploading the last chunk of data).
// The value must be a multiple of 512KB according to the spec of GData WAPI and
// Drive API v2. It is set to a smaller value than 2^31 for working around
// server side error (crbug.com/264089).
const int64 kUploadChunkSize = (1LL << 30);  // 1GB
}  // namespace

// Structure containing current upload information of file, passed between
// DriveServiceInterface methods and callbacks.
struct DriveUploader::UploadFileInfo {
  UploadFileInfo(const base::FilePath& local_path,
                 const std::string& content_type,
                 const UploadCompletionCallback& callback,
                 const ProgressCallback& progress_callback)
      : file_path(local_path),
        content_type(content_type),
        completion_callback(callback),
        progress_callback(progress_callback),
        content_length(0),
        next_start_position(-1),
        power_save_blocker(content::PowerSaveBlocker::Create(
            content::PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
            "Upload in progress")),
        cancelled(false),
        weak_ptr_factory_(this) {
  }

  ~UploadFileInfo() {
  }

  // Useful for printf debugging.
  std::string DebugString() const {
    return "file_path=[" + file_path.AsUTF8Unsafe() +
           "], content_type=[" + content_type +
           "], content_length=[" + base::UintToString(content_length) +
           "]";
  }

  // Returns the callback to cancel the upload represented by this struct.
  CancelCallback GetCancelCallback() {
    return base::Bind(&UploadFileInfo::Cancel, weak_ptr_factory_.GetWeakPtr());
  }

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

  int64 next_start_position;

  // Blocks system suspend while upload is in progress.
  scoped_ptr<content::PowerSaveBlocker> power_save_blocker;

  // Fields for implementing cancellation. |cancel_callback| is non-null if
  // there is an in-flight HTTP request. In that case, |cancell_callback| will
  // cancel the operation. |cancelled| is initially false and turns to true
  // once Cancel() is called. DriveUploader will check this field before after
  // an async task other than HTTP requests and cancels the subsequent requests
  // if this is flagged to true.
  CancelCallback cancel_callback;
  bool cancelled;

 private:
  // Cancels the upload represented by this struct.
  void Cancel() {
    cancelled = true;
    if (!cancel_callback.is_null())
      cancel_callback.Run();
  }

  base::WeakPtrFactory<UploadFileInfo> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(UploadFileInfo);
};

DriveUploader::DriveUploader(DriveServiceInterface* drive_service,
                             base::TaskRunner* blocking_task_runner)
    : drive_service_(drive_service),
      blocking_task_runner_(blocking_task_runner),
      weak_ptr_factory_(this) {
}

DriveUploader::~DriveUploader() {}

CancelCallback DriveUploader::UploadNewFile(
    const std::string& parent_resource_id,
    const base::FilePath& local_file_path,
    const std::string& title,
    const std::string& content_type,
    const UploadNewFileOptions& options,
    const UploadCompletionCallback& callback,
    const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!parent_resource_id.empty());
  DCHECK(!local_file_path.empty());
  DCHECK(!title.empty());
  DCHECK(!content_type.empty());
  DCHECK(!callback.is_null());

  return StartUploadFile(
      scoped_ptr<UploadFileInfo>(new UploadFileInfo(local_file_path,
                                                    content_type,
                                                    callback,
                                                    progress_callback)),
      base::Bind(&DriveUploader::StartInitiateUploadNewFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 parent_resource_id,
                 title,
                 options));
}

CancelCallback DriveUploader::UploadExistingFile(
    const std::string& resource_id,
    const base::FilePath& local_file_path,
    const std::string& content_type,
    const UploadExistingFileOptions& options,
    const UploadCompletionCallback& callback,
    const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!resource_id.empty());
  DCHECK(!local_file_path.empty());
  DCHECK(!content_type.empty());
  DCHECK(!callback.is_null());

  return StartUploadFile(
      scoped_ptr<UploadFileInfo>(new UploadFileInfo(local_file_path,
                                                    content_type,
                                                    callback,
                                                    progress_callback)),
      base::Bind(&DriveUploader::StartInitiateUploadExistingFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 resource_id,
                 options));
}

CancelCallback DriveUploader::ResumeUploadFile(
    const GURL& upload_location,
    const base::FilePath& local_file_path,
    const std::string& content_type,
    const UploadCompletionCallback& callback,
    const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!local_file_path.empty());
  DCHECK(!content_type.empty());
  DCHECK(!callback.is_null());

  scoped_ptr<UploadFileInfo> upload_file_info(new UploadFileInfo(
      local_file_path, content_type,
      callback, progress_callback));
  upload_file_info->upload_location = upload_location;

  return StartUploadFile(
      upload_file_info.Pass(),
      base::Bind(&DriveUploader::StartGetUploadStatus,
                 weak_ptr_factory_.GetWeakPtr()));
}

CancelCallback DriveUploader::StartUploadFile(
    scoped_ptr<UploadFileInfo> upload_file_info,
    const StartInitiateUploadCallback& start_initiate_upload_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "Uploading file: " << upload_file_info->DebugString();

  UploadFileInfo* info_ptr = upload_file_info.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&base::GetFileSize,
                 info_ptr->file_path,
                 &info_ptr->content_length),
      base::Bind(&DriveUploader::StartUploadFileAfterGetFileSize,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&upload_file_info),
                 start_initiate_upload_callback));
  return info_ptr->GetCancelCallback();
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

  if (upload_file_info->cancelled) {
    UploadFailed(upload_file_info.Pass(), GDATA_CANCELLED);
    return;
  }
  start_initiate_upload_callback.Run(upload_file_info.Pass());
}

void DriveUploader::StartInitiateUploadNewFile(
    const std::string& parent_resource_id,
    const std::string& title,
    const UploadNewFileOptions& options,
    scoped_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfo* info_ptr = upload_file_info.get();
  info_ptr->cancel_callback = drive_service_->InitiateUploadNewFile(
      info_ptr->content_type,
      info_ptr->content_length,
      parent_resource_id,
      title,
      options,
      base::Bind(&DriveUploader::OnUploadLocationReceived,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&upload_file_info)));
}

void DriveUploader::StartInitiateUploadExistingFile(
    const std::string& resource_id,
    const UploadExistingFileOptions& options,
    scoped_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfo* info_ptr = upload_file_info.get();
  info_ptr->cancel_callback = drive_service_->InitiateUploadExistingFile(
      info_ptr->content_type,
      info_ptr->content_length,
      resource_id,
      options,
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
           << "] for [" << upload_file_info->file_path.value() << "]";

  if (code != HTTP_SUCCESS) {
    if (code == HTTP_PRECONDITION)
      code = HTTP_CONFLICT;  // ETag mismatch.
    UploadFailed(upload_file_info.Pass(), code);
    return;
  }

  upload_file_info->upload_location = upload_location;
  upload_file_info->next_start_position = 0;
  UploadNextChunk(upload_file_info.Pass());
}

void DriveUploader::StartGetUploadStatus(
    scoped_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(upload_file_info);

  UploadFileInfo* info_ptr = upload_file_info.get();
  info_ptr->cancel_callback = drive_service_->GetUploadStatus(
      info_ptr->upload_location,
      info_ptr->content_length,
      base::Bind(&DriveUploader::OnUploadRangeResponseReceived,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&upload_file_info)));
}

void DriveUploader::UploadNextChunk(
    scoped_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(upload_file_info);
  DCHECK_GE(upload_file_info->next_start_position, 0);
  DCHECK_LE(upload_file_info->next_start_position,
            upload_file_info->content_length);

  if (upload_file_info->cancelled) {
    UploadFailed(upload_file_info.Pass(), GDATA_CANCELLED);
    return;
  }

  // Limit the size of data uploaded per each request by kUploadChunkSize.
  const int64 end_position = std::min(
      upload_file_info->content_length,
      upload_file_info->next_start_position + kUploadChunkSize);

  UploadFileInfo* info_ptr = upload_file_info.get();
  info_ptr->cancel_callback = drive_service_->ResumeUpload(
      info_ptr->upload_location,
      info_ptr->next_start_position,
      end_position,
      info_ptr->content_length,
      info_ptr->content_type,
      info_ptr->file_path,
      base::Bind(&DriveUploader::OnUploadRangeResponseReceived,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&upload_file_info)),
      base::Bind(&DriveUploader::OnUploadProgress,
                 weak_ptr_factory_.GetWeakPtr(),
                 info_ptr->progress_callback,
                 info_ptr->next_start_position,
                 info_ptr->content_length));
}

void DriveUploader::OnUploadRangeResponseReceived(
    scoped_ptr<UploadFileInfo> upload_file_info,
    const UploadRangeResponse& response,
    scoped_ptr<FileResource> entry) {
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
             << upload_file_info->file_path.value() << "]";

    // Done uploading.
    upload_file_info->completion_callback.Run(
        HTTP_SUCCESS, GURL(), entry.Pass());
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
    DVLOG(1)
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
           << " for [" << upload_file_info->file_path.value() << "]";

  upload_file_info->next_start_position = response.end_position_received;
  UploadNextChunk(upload_file_info.Pass());
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

  DVLOG(1) << "Upload failed " << upload_file_info->DebugString();

  if (upload_file_info->next_start_position < 0) {
    // Discard the upload location because no request could succeed with it.
    // Maybe it's obsolete.
    upload_file_info->upload_location = GURL();
  }

  upload_file_info->completion_callback.Run(
      error, upload_file_info->upload_location, scoped_ptr<FileResource>());
}

}  // namespace drive
