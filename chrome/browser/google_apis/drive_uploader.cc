// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_uploader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace {

// Google Documents List API requires uploading in chunks of 512kB.
const int64 kUploadChunkSize = 512 * 1024;

// Reads |bytes_to_read| bytes from |file_stream| to |buf|.
int ReadFileStreamOnBlockingPool(net::FileStream* file_stream,
                                 scoped_refptr<net::IOBuffer> buf,
                                 int bytes_to_read) {
  return file_stream->ReadSync(buf->data(), bytes_to_read);
}

}  // namespace

namespace google_apis {

DriveUploader::DriveUploader(DriveServiceInterface* drive_service)
  : drive_service_(drive_service),
    next_upload_id_(0),
    ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  base::SequencedWorkerPool* blocking_pool = BrowserThread::GetBlockingPool();
  blocking_task_runner_ = blocking_pool->GetSequencedTaskRunner(
      blocking_pool->GetSequenceToken());
}

DriveUploader::~DriveUploader() {
  for (UploadFileInfoMap::iterator iter = pending_uploads_.begin();
       iter != pending_uploads_.end();
       ++iter) {
    // FileStream must be closed on a file-access-allowed thread.
    blocking_task_runner_->DeleteSoon(
        FROM_HERE, iter->second->file_stream.release());
    delete iter->second;
  }
}

int DriveUploader::UploadNewFile(const GURL& upload_location,
                                 const FilePath& drive_file_path,
                                 const FilePath& local_file_path,
                                 const std::string& title,
                                 const std::string& content_type,
                                 int64 content_length,
                                 int64 file_size,
                                 const UploadCompletionCallback& callback) {
   DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
   DCHECK(!upload_location.is_empty());
   DCHECK(!drive_file_path.empty());
   DCHECK(!local_file_path.empty());
   DCHECK(!title.empty());
   DCHECK(!content_type.empty());
   DCHECK(!callback.is_null());

  scoped_ptr<UploadFileInfo> upload_file_info(new UploadFileInfo);
  upload_file_info->upload_mode = UPLOAD_NEW_FILE;
  upload_file_info->initial_upload_location = upload_location;
  upload_file_info->drive_path = drive_file_path;
  upload_file_info->file_path = local_file_path;
  upload_file_info->title = title;
  upload_file_info->content_type = content_type;
  upload_file_info->content_length = content_length;
  upload_file_info->file_size = file_size;
  upload_file_info->all_bytes_present = content_length == file_size;
  upload_file_info->completion_callback = callback;
  return StartUploadFile(upload_file_info.Pass());
}

int DriveUploader::StartUploadFile(
    scoped_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(upload_file_info.get());
  DCHECK_EQ(upload_file_info->upload_id, -1);
  DCHECK_NE(UPLOAD_INVALID, upload_file_info->upload_mode);

  const int upload_id = next_upload_id_++;
  upload_file_info->upload_id = upload_id;

  // Add upload_file_info to our internal map and take ownership.
  pending_uploads_[upload_id] = upload_file_info.release();
  UploadFileInfo* info = GetUploadFileInfo(upload_id);
  DVLOG(1) << "Uploading file: " << info->DebugString();

  // Create a FileStream to make sure the file can be opened successfully.
  info->file_stream.reset(new net::FileStream(NULL));

  // Create buffer to hold upload data. The full file size may not be known at
  // this point, so it may not be appropriate to use info->file_size.
  info->buf_len = kUploadChunkSize;
  info->buf = new net::IOBuffer(info->buf_len);

  OpenFile(info, FILE_OPEN_START_UPLOAD);
  return upload_id;
}

int DriveUploader::UploadExistingFile(
    const GURL& upload_location,
    const FilePath& drive_file_path,
    const FilePath& local_file_path,
    const std::string& content_type,
    int64 file_size,
    const UploadCompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!upload_location.is_empty());
  DCHECK(!drive_file_path.empty());
  DCHECK(!local_file_path.empty());
  DCHECK(!content_type.empty());
  DCHECK(!callback.is_null());

  scoped_ptr<UploadFileInfo> upload_file_info(new UploadFileInfo);
  upload_file_info->upload_mode = UPLOAD_EXISTING_FILE;
  upload_file_info->initial_upload_location = upload_location;
  upload_file_info->drive_path = drive_file_path;
  upload_file_info->file_path = local_file_path;
  upload_file_info->content_type = content_type;
  upload_file_info->content_length = file_size;
  upload_file_info->file_size = file_size;
  upload_file_info->all_bytes_present = true;
  upload_file_info->completion_callback = callback;
  return StartUploadFile(upload_file_info.Pass());
}

DriveUploader::UploadFileInfo* DriveUploader::GetUploadFileInfo(
    int upload_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfoMap::const_iterator it = pending_uploads_.find(upload_id);
  DVLOG_IF(1, it == pending_uploads_.end()) << "No upload found for id "
                                            << upload_id;
  return it != pending_uploads_.end() ? it->second : NULL;
}

void DriveUploader::OpenFile(UploadFileInfo* upload_file_info,
                             FileOpenType open_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Passing a raw net::FileStream* from upload_file_info->file_stream.get() is
  // safe, because the FileStream is always deleted by posting the task to the
  // same sequenced task runner. (See RemoveUpload() and ~DriveUploader()).
  // That is, deletion won't happen until this task completes.
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&net::FileStream::OpenSync,
                 base::Unretained(upload_file_info->file_stream.get()),
                 upload_file_info->file_path,
                 base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ),
      base::Bind(&DriveUploader::OpenCompletionCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 open_type,
                 upload_file_info->upload_id));
}

void DriveUploader::OpenCompletionCallback(FileOpenType open_type,
                                           int upload_id,
                                           int result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfo* upload_file_info = GetUploadFileInfo(upload_id);
  if (!upload_file_info)
    return;

  if (result != net::OK) {
    DCHECK_EQ(result, net::ERR_FILE_NOT_FOUND);
    UploadFailed(upload_file_info, DRIVE_UPLOAD_ERROR_NOT_FOUND);
    return;
  }

  if (upload_file_info->initial_upload_location.is_empty()) {
    UploadFailed(upload_file_info, DRIVE_UPLOAD_ERROR_ABORT);
    return;
  }

  // Open succeeded, initiate the upload.
  drive_service_->InitiateUpload(
      InitiateUploadParams(upload_file_info->upload_mode,
                           upload_file_info->title,
                           upload_file_info->content_type,
                           upload_file_info->content_length,
                           upload_file_info->initial_upload_location,
                           upload_file_info->drive_path),
      base::Bind(&DriveUploader::OnUploadLocationReceived,
                 weak_ptr_factory_.GetWeakPtr(),
                 upload_file_info->upload_id));
}

void DriveUploader::OnUploadLocationReceived(int upload_id,
                                             GDataErrorCode code,
                                             const GURL& upload_location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfo* upload_file_info = GetUploadFileInfo(upload_id);
  if (!upload_file_info)
    return;

  DVLOG(1) << "Got upload location [" << upload_location.spec()
           << "] for [" << upload_file_info->title << "]";

  if (code != HTTP_SUCCESS) {
    // TODO(achuith): Handle error codes from Google Docs server.
    UploadFailed(upload_file_info, DRIVE_UPLOAD_ERROR_ABORT);
    return;
  }

  upload_file_info->upload_location = upload_location;

  // Start the upload from the beginning of the file.
  UploadNextChunk(upload_file_info);
}

void DriveUploader::UploadNextChunk(UploadFileInfo* upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Check that |upload_file_info| is in pending_uploads_.
  DCHECK(upload_file_info == GetUploadFileInfo(upload_file_info->upload_id));
  DVLOG(1) << "Number of pending uploads=" << pending_uploads_.size();

  // Determine number of bytes to read for this upload iteration, which cannot
  // exceed size of buf i.e. buf_len.
  const int64 bytes_remaining = upload_file_info->SizeRemaining();
  const int bytes_to_read = std::min(upload_file_info->SizeRemaining(),
                                     upload_file_info->buf_len);

  // Update the content length if the file_size is known.
  if (upload_file_info->all_bytes_present)
    upload_file_info->content_length = upload_file_info->file_size;
  else if (bytes_remaining == bytes_to_read) {
    // Wait for more data if this is the last chunk we have and we don't know
    // whether we've reached the end of the file. We won't know how much data to
    // expect until the transfer is complete (the Content-Length might be
    // incorrect or absent). If we've sent the last chunk out already when we
    // find out there's no more data, we won't be able to complete the upload.
    DVLOG(1) << "Paused upload " << upload_file_info->title;
    upload_file_info->upload_paused = true;
    return;
  }

  if (bytes_to_read == 0) {
    // This should only happen when the actual file size is 0.
    DCHECK(upload_file_info->all_bytes_present &&
           upload_file_info->content_length == 0);

    upload_file_info->start_position = 0;
    upload_file_info->end_position = 0;
    // Skips file_stream->Read and error checks for 0-byte case. Immediately
    // proceeds to ResumeUpload.
    // TODO(kinaba): http://crbug.com/134814
    // Replace the following PostTask() to an direct method call. This is needed
    // because we have to ResumeUpload after the previous InitiateUpload or
    // ResumeUpload is completely finished; at this point, we are inside the
    // callback function from the previous operation, which is not treated as
    // finished yet.
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(&DriveUploader::ResumeUpload,
                   weak_ptr_factory_.GetWeakPtr(),
                   upload_file_info->upload_id));
    return;
  }

  // Passing a raw net::FileStream* from upload_file_info->file_stream.get() is
  // safe. See the comment in OpenFile().
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&ReadFileStreamOnBlockingPool,
                 upload_file_info->file_stream.get(),
                 upload_file_info->buf,
                 bytes_to_read),
      base::Bind(&DriveUploader::ReadCompletionCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 upload_file_info->upload_id,
                 bytes_to_read));
}

void DriveUploader::ReadCompletionCallback(int upload_id,
                                           int bytes_to_read,
                                           int bytes_read) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Should be non-zero. FileStream::Read() returns 0 only at end-of-file,
  // but we don't try to read from end-of-file.
  DCHECK_NE(0, bytes_read);
  DVLOG(1) << "ReadCompletionCallback bytes read=" << bytes_read;

  UploadFileInfo* upload_file_info = GetUploadFileInfo(upload_id);
  if (!upload_file_info)
    return;

  if (bytes_read < 0) {
    LOG(ERROR) << "Error reading from file "
               << upload_file_info->file_path.value();
    UploadFailed(upload_file_info, DRIVE_UPLOAD_ERROR_ABORT);
    return;
  }

  upload_file_info->start_position = upload_file_info->end_position;
  upload_file_info->end_position =
      upload_file_info->start_position + bytes_read;

  ResumeUpload(upload_id);
}

void DriveUploader::ResumeUpload(int upload_id) {
  UploadFileInfo* upload_file_info = GetUploadFileInfo(upload_id);
  if (!upload_file_info)
    return;

  drive_service_->ResumeUpload(
      ResumeUploadParams(upload_file_info->upload_mode,
                         upload_file_info->start_position,
                         upload_file_info->end_position,
                         upload_file_info->content_length,
                         upload_file_info->content_type,
                         upload_file_info->buf,
                         upload_file_info->upload_location,
                         upload_file_info->drive_path),
      base::Bind(&DriveUploader::OnResumeUploadResponseReceived,
                 weak_ptr_factory_.GetWeakPtr(),
                 upload_file_info->upload_id));
}

void DriveUploader::OnResumeUploadResponseReceived(
    int upload_id,
    const ResumeUploadResponse& response,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfo* upload_file_info = GetUploadFileInfo(upload_id);
  if (!upload_file_info)
    return;

  const UploadMode upload_mode = upload_file_info->upload_mode;
  if ((upload_mode == UPLOAD_NEW_FILE && response.code == HTTP_CREATED) ||
      (upload_mode == UPLOAD_EXISTING_FILE && response.code == HTTP_SUCCESS)) {
    DVLOG(1) << "Successfully created uploaded file=["
             << upload_file_info->title << "]";

    // Done uploading.
    upload_file_info->entry = entry.Pass();
    upload_file_info->completion_callback.Run(
        DRIVE_UPLOAD_OK,
        upload_file_info->drive_path,
        upload_file_info->file_path,
        upload_file_info->entry.Pass());

    // This will delete |upload_file_info|.
    RemoveUpload(scoped_ptr<UploadFileInfo>(upload_file_info));
    return;
  }

  // If code is 308 (RESUME_INCOMPLETE) and range_received is what has been
  // previously uploaded (i.e. = upload_file_info->end_position), proceed to
  // upload the next chunk.
  if (response.code != HTTP_RESUME_INCOMPLETE ||
      response.start_position_received != 0 ||
      response.end_position_received != upload_file_info->end_position) {
    // TODO(achuith): Handle error cases, e.g.
    // - when previously uploaded data wasn't received by Google Docs server,
    //   i.e. when end_position_received < upload_file_info->end_position
    LOG(ERROR) << "UploadNextChunk http code=" << response.code
        << ", start_position_received=" << response.start_position_received
        << ", end_position_received=" << response.end_position_received
        << ", expected end range=" << upload_file_info->end_position;
    UploadFailed(
        upload_file_info,
        response.code == HTTP_FORBIDDEN ?
        DRIVE_UPLOAD_ERROR_NO_SPACE : DRIVE_UPLOAD_ERROR_ABORT);
    return;
  }

  DVLOG(1) << "Received range " << response.start_position_received
           << "-" << response.end_position_received
           << " for [" << upload_file_info->title << "]";

  // Continue uploading.
  UploadNextChunk(upload_file_info);
}

void DriveUploader::UploadFailed(UploadFileInfo* upload_file_info,
                                 DriveUploadError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  LOG(ERROR) << "Upload failed " << upload_file_info->DebugString();

  upload_file_info->completion_callback.Run(
      error,
      upload_file_info->drive_path,
      upload_file_info->file_path,
      upload_file_info->entry.Pass());

  // This will delete |upload_file_info|.
  RemoveUpload(scoped_ptr<UploadFileInfo>(upload_file_info));
}

void DriveUploader::RemoveUpload(scoped_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // FileStream must be closed on a file-access-allowed thread.
  blocking_task_runner_->DeleteSoon(
      FROM_HERE, upload_file_info->file_stream.release());
  pending_uploads_.erase(upload_file_info->upload_id);
}

DriveUploader::UploadFileInfo::UploadFileInfo()
    : upload_id(-1),
      file_size(0),
      content_length(0),
      upload_mode(UPLOAD_INVALID),
      file_stream(NULL),
      buf_len(0),
      start_position(0),
      end_position(0),
      all_bytes_present(false),
      upload_paused(false) {
}

DriveUploader::UploadFileInfo::~UploadFileInfo() { }

int64 DriveUploader::UploadFileInfo::SizeRemaining() const {
  DCHECK(file_size >= end_position);
  return file_size - end_position;
}

std::string DriveUploader::UploadFileInfo::DebugString() const {
  return "title=[" + title +
         "], file_path=[" + file_path.AsUTF8Unsafe() +
         "], content_type=[" + content_type +
         "], content_length=[" + base::UintToString(content_length) +
         "], file_size=[" + base::UintToString(file_size) +
         "], drive_path=[" + drive_path.AsUTF8Unsafe() +
         "]";
}

}  // namespace google_apis
