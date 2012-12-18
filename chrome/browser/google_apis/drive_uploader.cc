// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_uploader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/drive_upload_mode.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace {

// Google Documents List API requires uploading in chunks of 512kB.
const int64 kUploadChunkSize = 512 * 1024;

// Opens |path| with |file_stream| and returns the file size.
// If failed, returns an error code in a negative value.
int64 OpenFileStreamAndGetSizeOnBlockingPool(net::FileStream* file_stream,
                                             const FilePath& path) {
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
  UploadFileInfo(scoped_refptr<base::SequencedTaskRunner> task_runner,
                 UploadMode upload_mode,
                 const GURL& initial_upload_location,
                 const FilePath& drive_path,
                 const FilePath& local_path,
                 const std::string& title,
                 const std::string& content_type,
                 const UploadCompletionCallback& callback)
      : upload_mode(upload_mode),
        initial_upload_location(initial_upload_location),
        drive_path(drive_path),
        file_path(local_path),
        title(title),
        content_type(content_type),
        completion_callback(callback),
        content_length(0),
        next_send_position(0),
        file_stream(new net::FileStream(NULL)),
        buf(new net::IOBuffer(kUploadChunkSize)),
        blocking_task_runner(task_runner) {
  }

  ~UploadFileInfo() {
    blocking_task_runner->DeleteSoon(FROM_HERE, file_stream.release());
  }

  // Bytes left to upload.
  int64 SizeRemaining()  const {
    DCHECK(content_length >= next_send_position);
    return content_length - next_send_position;
  }

  // Useful for printf debugging.
  std::string DebugString() const {
    return "title=[" + title +
           "], file_path=[" + file_path.AsUTF8Unsafe() +
           "], content_type=[" + content_type +
           "], content_length=[" + base::UintToString(content_length) +
           "], drive_path=[" + drive_path.AsUTF8Unsafe() +
           "]";
  }

  // Whether this is uploading a new file or updating an existing file.
  const UploadMode upload_mode;

  // Location URL used to get |upload_location| with InitiateUpload.
  const GURL initial_upload_location;

  // Final path in gdata. Looks like /special/drive/MyFolder/MyFile.
  const FilePath drive_path;

  // The local file path of the file to be uploaded.
  const FilePath file_path;

  // Title to be used for file to be uploaded.
  const std::string title;

  // Content-Type of file.
  const std::string content_type;

  // Callback to be invoked once the upload has finished.
  const UploadCompletionCallback completion_callback;

  // Location URL where file is to be uploaded to, returned from
  // InitiateUpload. Used for the subsequent ResumeUpload requests.
  GURL upload_location;

  // Header content-Length.
  int64 content_length;

  // The start position of the contents to be sent as the next upload chunk.
  int64 next_send_position;

  // For opening and reading from physical file.
  //
  // File operations are posted to |blocking_task_runner|, while the ownership
  // of the stream is held in UI thread. At the point when this UploadFileInfo
  // is destroyed, the ownership of the stream is passed to the worker pool.
  // TODO(kinaba): We should switch to async API of FileStream once
  // crbug.com/164312 is fixed.
  scoped_ptr<net::FileStream> file_stream;

  // Holds current content to be uploaded.
  const scoped_refptr<net::IOBuffer> buf;

  // Runner for net::FileStream tasks.
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner;
};

DriveUploader::DriveUploader(DriveServiceInterface* drive_service)
    : drive_service_(drive_service),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  base::SequencedWorkerPool* blocking_pool = BrowserThread::GetBlockingPool();
  blocking_task_runner_ = blocking_pool->GetSequencedTaskRunner(
      blocking_pool->GetSequenceToken());
}

DriveUploader::~DriveUploader() {}

void DriveUploader::UploadNewFile(const GURL& upload_location,
                                  const FilePath& drive_file_path,
                                  const FilePath& local_file_path,
                                  const std::string& title,
                                  const std::string& content_type,
                                  const UploadCompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!upload_location.is_empty());
  DCHECK(!drive_file_path.empty());
  DCHECK(!local_file_path.empty());
  DCHECK(!title.empty());
  DCHECK(!content_type.empty());
  DCHECK(!callback.is_null());

  StartUploadFile(scoped_ptr<UploadFileInfo>(new UploadFileInfo(
      blocking_task_runner_,
      UPLOAD_NEW_FILE,
      upload_location,
      drive_file_path,
      local_file_path,
      title,
      content_type,
      callback
  )));
}

void DriveUploader::UploadExistingFile(
    const GURL& upload_location,
    const FilePath& drive_file_path,
    const FilePath& local_file_path,
    const std::string& content_type,
    const UploadCompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!upload_location.is_empty());
  DCHECK(!drive_file_path.empty());
  DCHECK(!local_file_path.empty());
  DCHECK(!content_type.empty());
  DCHECK(!callback.is_null());

  StartUploadFile(scoped_ptr<UploadFileInfo>(new UploadFileInfo(
      blocking_task_runner_,
      UPLOAD_EXISTING_FILE,
      upload_location,
      drive_file_path,
      local_file_path,
      "",  // title : not necessary for update of an existing file.
      content_type,
      callback
  )));
}

void DriveUploader::StartUploadFile(
    scoped_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "Uploading file: " << upload_file_info->DebugString();

  // Passing a raw net::FileStream* to the blocking pool is safe, because it is
  // owned by |upload_file_info| in the reply callback.
  UploadFileInfo* info_ptr = upload_file_info.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&OpenFileStreamAndGetSizeOnBlockingPool,
                 info_ptr->file_stream.get(),
                 info_ptr->file_path),
      base::Bind(&DriveUploader::OpenCompletionCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&upload_file_info)));
}

void DriveUploader::OpenCompletionCallback(
    scoped_ptr<UploadFileInfo> upload_file_info, int64 file_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (file_size < 0) {
    UploadFailed(upload_file_info.Pass(), DRIVE_UPLOAD_ERROR_NOT_FOUND);
    return;
  }

  upload_file_info->content_length = file_size;

  // Open succeeded, initiate the upload.
  UploadFileInfo* info_ptr = upload_file_info.get();
  drive_service_->InitiateUpload(
      InitiateUploadParams(info_ptr->upload_mode,
                           info_ptr->title,
                           info_ptr->content_type,
                           info_ptr->content_length,
                           info_ptr->initial_upload_location,
                           info_ptr->drive_path),
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
           << "] for [" << upload_file_info->title << "]";

  if (code != HTTP_SUCCESS) {
    // TODO(achuith): Handle error codes from Google Docs server.
    UploadFailed(upload_file_info.Pass(), DRIVE_UPLOAD_ERROR_ABORT);
    return;
  }

  upload_file_info->upload_location = upload_location;

  // Start the upload from the beginning of the file.
  UploadNextChunk(upload_file_info.Pass());
}

void DriveUploader::UploadNextChunk(
    scoped_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Determine number of bytes to read for this upload iteration.
  const int bytes_to_read = std::min(upload_file_info->SizeRemaining(),
                                     kUploadChunkSize);

  if (bytes_to_read == 0) {
    // net::FileStream doesn't allow to read 0 bytes, so directly proceed to the
    // completion callback. PostTask is necessary because we have to finish
    // InitiateUpload's callback before calling ResumeUpload, due to the
    // implementation of OperationRegistry. (http://crbug.com/134814)
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&DriveUploader::ReadCompletionCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&upload_file_info), 0, 0));
    return;
  }

  // Passing a raw |file_stream| and |buf| to the blocking pool is safe, because
  // they are owned by |upload_file_info| in the reply callback.
  UploadFileInfo* info_ptr = upload_file_info.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&net::FileStream::ReadUntilComplete,
                 base::Unretained(info_ptr->file_stream.get()),
                 info_ptr->buf->data(),
                 bytes_to_read),
      base::Bind(&DriveUploader::ReadCompletionCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&upload_file_info),
                 bytes_to_read));
}

void DriveUploader::ReadCompletionCallback(
    scoped_ptr<UploadFileInfo> upload_file_info,
    int bytes_to_read,
    int bytes_read) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(bytes_to_read, bytes_read);
  DVLOG(1) << "ReadCompletionCallback bytes read=" << bytes_read;

  if (bytes_read < 0) {
    LOG(ERROR) << "Error reading from file "
               << upload_file_info->file_path.value();
    UploadFailed(upload_file_info.Pass(), DRIVE_UPLOAD_ERROR_ABORT);
    return;
  }

  int64 start_position = upload_file_info->next_send_position;
  upload_file_info->next_send_position += bytes_read;
  int64 end_position = upload_file_info->next_send_position;

  UploadFileInfo* info_ptr = upload_file_info.get();
  drive_service_->ResumeUpload(
      ResumeUploadParams(info_ptr->upload_mode,
                         start_position,
                         end_position,
                         info_ptr->content_length,
                         info_ptr->content_type,
                         info_ptr->buf,
                         info_ptr->upload_location,
                         info_ptr->drive_path),
      base::Bind(&DriveUploader::OnResumeUploadResponseReceived,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&upload_file_info)));
}

void DriveUploader::OnResumeUploadResponseReceived(
    scoped_ptr<UploadFileInfo> upload_file_info,
    const ResumeUploadResponse& response,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const UploadMode upload_mode = upload_file_info->upload_mode;
  if ((upload_mode == UPLOAD_NEW_FILE && response.code == HTTP_CREATED) ||
      (upload_mode == UPLOAD_EXISTING_FILE && response.code == HTTP_SUCCESS)) {
    DVLOG(1) << "Successfully created uploaded file=["
             << upload_file_info->title << "]";

    // Done uploading.
    upload_file_info->completion_callback.Run(DRIVE_UPLOAD_OK,
                                              upload_file_info->drive_path,
                                              upload_file_info->file_path,
                                              entry.Pass());
    return;
  }

  // If code is 308 (RESUME_INCOMPLETE) and range_received is what has been
  // previously uploaded (i.e. = upload_file_info->end_position), proceed to
  // upload the next chunk.
  if (response.code != HTTP_RESUME_INCOMPLETE ||
      response.start_position_received != 0 ||
      response.end_position_received != upload_file_info->next_send_position) {
    // TODO(achuith): Handle error cases, e.g.
    // - when previously uploaded data wasn't received by Google Docs server,
    //   i.e. when end_position_received < upload_file_info->end_position
    LOG(ERROR) << "UploadNextChunk http code=" << response.code
        << ", start_position_received=" << response.start_position_received
        << ", end_position_received=" << response.end_position_received
        << ", expected end range=" << upload_file_info->next_send_position;
    UploadFailed(upload_file_info.Pass(),
                 response.code == HTTP_FORBIDDEN ?
                     DRIVE_UPLOAD_ERROR_NO_SPACE : DRIVE_UPLOAD_ERROR_ABORT);
    return;
  }

  DVLOG(1) << "Received range " << response.start_position_received
           << "-" << response.end_position_received
           << " for [" << upload_file_info->title << "]";

  // Continue uploading.
  UploadNextChunk(upload_file_info.Pass());
}

void DriveUploader::UploadFailed(scoped_ptr<UploadFileInfo> upload_file_info,
                                 DriveUploadError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  LOG(ERROR) << "Upload failed " << upload_file_info->DebugString();

  upload_file_info->completion_callback.Run(error,
                                            upload_file_info->drive_path,
                                            upload_file_info->file_path,
                                            scoped_ptr<ResourceEntry>());
}

}  // namespace google_apis
