// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_uploader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace {

// Google Documents List API requires uploading in chunks of 512kB.
const int64 kUploadChunkSize = 512 * 1024;

// Maximum number of times we try to open a file before giving up.
const int kMaxFileOpenTries = 5;

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

int DriveUploader::UploadNewFile(
    const GURL& upload_location,
    const FilePath& drive_file_path,
    const FilePath& local_file_path,
    const std::string& title,
    const std::string& content_type,
    int64 content_length,
    int64 file_size,
    const UploadCompletionCallback& completion_callback,
    const UploaderReadyCallback& ready_callback) {
   DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
   DCHECK(!upload_location.is_empty());
   DCHECK(!drive_file_path.empty());
   DCHECK(!local_file_path.empty());
   DCHECK(!title.empty());
   DCHECK(!content_type.empty());
   DCHECK(!completion_callback.is_null());
   // ready_callback may be null.

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
  upload_file_info->completion_callback = completion_callback;
  upload_file_info->ready_callback = ready_callback;

  // When uploading a new file, we should retry file open as the file may
  // not yet be ready. See comments in OpenCompletionCallback.
  // TODO(satorux): The retry should be done only when we are uploading
  // while downloading files from web sites (i.e. saving files to Drive).
  upload_file_info->should_retry_file_open = true;
  return StartUploadFile(upload_file_info.Pass());
}

int DriveUploader::StreamExistingFile(
    const GURL& upload_location,
    const FilePath& drive_file_path,
    const FilePath& local_file_path,
    const std::string& content_type,
    int64 content_length,
    int64 file_size,
    const UploadCompletionCallback& completion_callback,
    const UploaderReadyCallback& ready_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!upload_location.is_empty());
  DCHECK(!drive_file_path.empty());
  DCHECK(!local_file_path.empty());
  DCHECK(!content_type.empty());
  DCHECK(!completion_callback.is_null());
  // ready_callback may be null.

  scoped_ptr<UploadFileInfo> upload_file_info(new UploadFileInfo);
  upload_file_info->upload_mode = UPLOAD_EXISTING_FILE;
  upload_file_info->initial_upload_location = upload_location;
  upload_file_info->drive_path = drive_file_path;
  upload_file_info->file_path = local_file_path;
  upload_file_info->content_type = content_type;
  upload_file_info->content_length = content_length;
  upload_file_info->file_size = file_size;
  upload_file_info->all_bytes_present = content_length == file_size;
  upload_file_info->completion_callback = completion_callback;
  upload_file_info->ready_callback = ready_callback;

  // When uploading a new file, we should retry file open as the file may
  // not yet be ready. See comments in OpenCompletionCallback.
  // TODO(satorux): The retry should be done only when we are uploading
  // while downloading files from web sites (i.e. saving files to Drive).
  upload_file_info->should_retry_file_open = true;
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
    const UploadCompletionCallback& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!upload_location.is_empty());
  DCHECK(!drive_file_path.empty());
  DCHECK(!local_file_path.empty());
  DCHECK(!content_type.empty());
  DCHECK(!completion_callback.is_null());

  scoped_ptr<UploadFileInfo> upload_file_info(new UploadFileInfo);
  upload_file_info->upload_mode = UPLOAD_EXISTING_FILE;
  upload_file_info->initial_upload_location = upload_location;
  upload_file_info->drive_path = drive_file_path;
  upload_file_info->file_path = local_file_path;
  upload_file_info->content_type = content_type;
  upload_file_info->content_length = file_size;
  upload_file_info->file_size = file_size;
  upload_file_info->all_bytes_present = true;
  upload_file_info->completion_callback = completion_callback;

  // When uploading an updated file, we should not retry file open as the
  // file should already be present by definition.
  upload_file_info->should_retry_file_open = false;
  return StartUploadFile(upload_file_info.Pass());
}

void DriveUploader::UpdateUpload(int upload_id,
                                 content::DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfo* upload_file_info = GetUploadFileInfo(upload_id);
  if (!upload_file_info)
    return;

  const int64 file_size = download->GetReceivedBytes();

  // Update file_size and all_bytes_present.
  DVLOG(1) << "Updating file size from " << upload_file_info->file_size
           << " to " << file_size
           << (download->AllDataSaved() ? " (AllDataSaved)" : " (In-progress)");
  upload_file_info->file_size = file_size;
  upload_file_info->all_bytes_present = download->AllDataSaved();
  if (upload_file_info->file_path != download->GetFullPath()) {
    // We shouldn't see a rename if should_retry_file_open is true. The only
    // rename we expect (for now) is the final rename that happens after the
    // download transition from IN_PROGRESS -> COMPLETE. This, in turn, only
    // happens after the upload completes. However, since this isn't enforced by
    // the API contract, we reset the retry count so we can retry all over again
    // with the new path.
    // TODO(asanka): Introduce a synchronization point after the initial rename
    //               of the download and get rid of the retry logic.
    upload_file_info->num_file_open_tries = 0;
    upload_file_info->file_path = download->GetFullPath();
  }

  // Resume upload if necessary and possible.
  if (upload_file_info->upload_paused &&
      (upload_file_info->all_bytes_present ||
       upload_file_info->SizeRemaining() > kUploadChunkSize)) {
    DVLOG(1) << "Resuming upload " << upload_file_info->title;
    upload_file_info->upload_paused = false;
    UploadNextChunk(upload_file_info);
  }

  // Retry opening this file if we failed before.  File open can fail because
  // the downloads system sets the full path on the UI thread and schedules a
  // rename on the FILE thread. Thus the new path is visible on the UI thread
  // before the renamed file is available on the file system.
  if (upload_file_info->should_retry_file_open) {
    DCHECK(!download->IsComplete());
    // Disallow further retries.
    upload_file_info->should_retry_file_open = false;
    OpenFile(upload_file_info, FILE_OPEN_UPDATE_UPLOAD);
  }
}

int64 DriveUploader::GetUploadedBytes(int upload_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UploadFileInfo* upload_info = GetUploadFileInfo(upload_id);
  // We return the start_range as the count of uploaded bytes since that is the
  // start of the next or currently uploading chunk.
  // TODO(asanka): Use a finer grained progress value than this. We end up
  //               reporting progress in kUploadChunkSize increments.
  return upload_info ? upload_info->start_range : 0;
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

  // The file may actually not exist yet, as the downloads system downloads
  // to a temp location and then renames the file. If this is the case, we
  // just retry opening the file later.
  if (result != net::OK) {
    DCHECK_EQ(result, net::ERR_FILE_NOT_FOUND);

    if (upload_file_info->should_retry_file_open) {
      // File open failed. Try again later.
      upload_file_info->num_file_open_tries++;

      DVLOG(1) << "Error opening \"" << upload_file_info->file_path.value()
               << "\" for reading: " << net::ErrorToString(result)
               << ", tries=" << upload_file_info->num_file_open_tries;

      // Stop trying to open this file if we exceed kMaxFileOpenTries.
      const bool exceeded_max_attempts =
          upload_file_info->num_file_open_tries >= kMaxFileOpenTries;
      upload_file_info->should_retry_file_open = !exceeded_max_attempts;
    }
    if (!upload_file_info->should_retry_file_open) {
      UploadFailed(upload_file_info, DRIVE_UPLOAD_ERROR_NOT_FOUND);
      return;
    }
  } else {
    // Open succeeded, initiate the upload.
    upload_file_info->should_retry_file_open = false;
    if (upload_file_info->initial_upload_location.is_empty()) {
      UploadFailed(upload_file_info, DRIVE_UPLOAD_ERROR_ABORT);
      return;
    }
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
  // The uploader gets ready after we complete opening the file, called
  // from the StartUploadFile method. We use PostTask on purpose, because
  // this callback is called by FileStream, and we may access FileStream
  // again from the |ready_callback| implementation. FileStream is not
  // reentrant.
  //
  // Note, that we call this callback if we opened the file, or if we
  // failed, but further retries are scheduled. The callback will not be
  // called if the upload has been aborted.
  if (open_type == FILE_OPEN_START_UPLOAD &&
      !upload_file_info->ready_callback.is_null()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(upload_file_info->ready_callback, upload_id));
  }
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

    upload_file_info->start_range = 0;
    upload_file_info->end_range = -1;
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

  upload_file_info->start_range = upload_file_info->end_range + 1;
  upload_file_info->end_range = upload_file_info->start_range +
                                bytes_read - 1;

  ResumeUpload(upload_id);
}

void DriveUploader::ResumeUpload(int upload_id) {
  UploadFileInfo* upload_file_info = GetUploadFileInfo(upload_id);
  if (!upload_file_info)
    return;

  drive_service_->ResumeUpload(
      ResumeUploadParams(upload_file_info->upload_mode,
                         upload_file_info->start_range,
                         upload_file_info->end_range,
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
  // previously uploaded (i.e. = upload_file_info->end_range), proceed to
  // upload the next chunk.
  if (response.code != HTTP_RESUME_INCOMPLETE ||
      response.start_range_received != 0 ||
      response.end_range_received != upload_file_info->end_range) {
    // TODO(achuith): Handle error cases, e.g.
    // - when previously uploaded data wasn't received by Google Docs server,
    //   i.e. when end_range_received < upload_file_info->end_range
    LOG(ERROR) << "UploadNextChunk http code=" << response.code
               << ", start_range_received=" << response.start_range_received
               << ", end_range_received=" << response.end_range_received
               << ", expected end range=" << upload_file_info->end_range;
    UploadFailed(
        upload_file_info,
        response.code == HTTP_FORBIDDEN ?
        DRIVE_UPLOAD_ERROR_NO_SPACE : DRIVE_UPLOAD_ERROR_ABORT);
    return;
  }

  DVLOG(1) << "Received range " << response.start_range_received
           << "-" << response.end_range_received
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
      start_range(0),
      end_range(-1),
      all_bytes_present(false),
      upload_paused(false),
      should_retry_file_open(false),
      num_file_open_tries(0) {
}

DriveUploader::UploadFileInfo::~UploadFileInfo() { }

int64 DriveUploader::UploadFileInfo::SizeRemaining() const {
  DCHECK(file_size > end_range);
  // Note that uploaded_bytes = end_range + 1;
  return file_size - end_range - 1;
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
