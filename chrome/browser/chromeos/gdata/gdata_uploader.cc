// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_uploader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/gdata/gdata.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_upload_file_info.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace {

// Google Documents List API requires uploading in chunks of 512kB.
const int64 kUploadChunkSize = 512 * 1024;

// Maximum number of times we try to open a file before giving up.
const int kMaxFileOpenTries = 5;

}  // namespace

namespace gdata {

GDataUploader::GDataUploader(GDataFileSystem* file_system)
  : file_system_(file_system),
    ALLOW_THIS_IN_INITIALIZER_LIST(uploader_factory_(this)) {
}

GDataUploader::~GDataUploader() {
}

void GDataUploader::UploadFile(UploadFileInfo* upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(upload_file_info);

  // Add upload_file_info to our internal map and take ownership.
  pending_uploads_[upload_file_info->file_url] = upload_file_info;
  DVLOG(1) << "Uploading file: " << upload_file_info->DebugString();

  // Create a FileStream to make sure the file can be opened successfully.
  upload_file_info->file_stream = new net::FileStream(NULL);

  // Create buffer to hold upload data.
  upload_file_info->buf_len = std::min(upload_file_info->file_size,
                                       kUploadChunkSize);
  upload_file_info->buf = new net::IOBuffer(upload_file_info->buf_len);

  OpenFile(upload_file_info);
}

void GDataUploader::UpdateUpload(const GURL& file_url,
                                 const FilePath& file_path,
                                 int64 file_size,
                                 bool download_complete) {
  UploadFileInfo* upload_file_info = GetUploadFileInfo(file_url);
  if (!upload_file_info)
    return;

  // Update file_size and download_complete.
  DVLOG(1) << "Updating file size from " << upload_file_info->file_size
           << " to " << file_size;
  upload_file_info->file_size = file_size;
  upload_file_info->download_complete = download_complete;

  // Resume upload if necessary and possible.
  if (upload_file_info->upload_paused &&
      (upload_file_info->download_complete ||
      upload_file_info->SizeRemaining() >= kUploadChunkSize)) {
    DVLOG(1) << "Resuming upload " << upload_file_info->title;
    upload_file_info->upload_paused = false;
    UploadNextChunk(upload_file_info);
  }

  // Retry opening this file if we failed before.
  // File open can fail because the downloads system downloads to temporary
  // locations then renames files.
  if (upload_file_info->should_retry_file_open) {
    // Reset number of file open tries if file_path has changed.
    // This can happen when the file gets renamed from the intermediate
    // 'foo.crdownload' to the final 'foo' path.
    if (upload_file_info->file_path != file_path)
      upload_file_info->num_file_open_tries = 0;
    upload_file_info->file_path = file_path;
    // Disallow further retries.
    upload_file_info->should_retry_file_open = false;

    OpenFile(upload_file_info);
  }
}

UploadFileInfo* GDataUploader::GetUploadFileInfo(const GURL& file_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfoMap::iterator it = pending_uploads_.find(file_url);
  return it != pending_uploads_.end() ? it->second : NULL;
}

void GDataUploader::RemovePendingUpload(UploadFileInfo* upload_file_info) {
  pending_uploads_.erase(upload_file_info->file_url);
  // The file stream is closed by the destructor asynchronously.
  delete upload_file_info->file_stream;
  delete upload_file_info;
}

void GDataUploader::OpenFile(UploadFileInfo* upload_file_info) {
  // Open the file asynchronously.
  const int rv = upload_file_info->file_stream->Open(
      upload_file_info->file_path,
      base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_ASYNC,
      base::Bind(&GDataUploader::OpenCompletionCallback,
                 uploader_factory_.GetWeakPtr(),
                 upload_file_info->file_url));
  DCHECK_EQ(net::ERR_IO_PENDING, rv);
}

void GDataUploader::OpenCompletionCallback(const GURL& file_url, int result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfo* upload_file_info = GetUploadFileInfo(file_url);
  if (!upload_file_info)
    return;

  // The file may actually not exist yet, as the downloads system downloads
  // to a temp location and then renames the file. If this is the case, we
  // just retry opening the file later.
  if (result != net::OK) {
    DCHECK_EQ(result, net::ERR_FILE_NOT_FOUND);
    // File open failed. Try again later.
    upload_file_info->num_file_open_tries++;

    DVLOG(1) << "Error opening \"" << upload_file_info->file_path.value()
             << "\" for reading: " << net::ErrorToString(result)
             << ", tries=" << upload_file_info->num_file_open_tries;

    // Stop trying to open this file if we exceed kMaxFileOpenTries.
    const bool exceeded_max_attempts =
        upload_file_info->num_file_open_tries >= kMaxFileOpenTries;
    upload_file_info->should_retry_file_open = !exceeded_max_attempts;
    if (exceeded_max_attempts)
      RemovePendingUpload(upload_file_info);

    return;
  }

  // Open succeeded, initiate the upload.
  upload_file_info->should_retry_file_open = false;
  file_system_->InitiateUpload(
      upload_file_info->title,
      upload_file_info->content_type,
      upload_file_info->content_length,
      upload_file_info->gdata_path.DirName(),
      upload_file_info->gdata_path,
      base::Bind(&GDataUploader::OnUploadLocationReceived,
                 uploader_factory_.GetWeakPtr(),
                 upload_file_info->file_url));
}

void GDataUploader::OnUploadLocationReceived(
    const GURL& file_url,
    GDataErrorCode code,
    const GURL& upload_location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfo* upload_file_info = GetUploadFileInfo(file_url);
  if (!upload_file_info)
    return;

  DVLOG(1) << "Got upload location [" << upload_location.spec()
           << "] for [" << upload_file_info->title << "]";

  if (code != HTTP_SUCCESS) {
    // TODO(achuith): Handle error codes from Google Docs server.
    RemovePendingUpload(upload_file_info);
    NOTREACHED();
    return;
  }

  upload_file_info->upload_location = upload_location;

  // Start the upload from the beginning of the file.
  UploadNextChunk(upload_file_info);
}

void GDataUploader::UploadNextChunk(UploadFileInfo* upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Check that |upload_file_info| is in pending_uploads_.
  DCHECK(upload_file_info == GetUploadFileInfo(upload_file_info->file_url));
  DVLOG(1) << "Number of pending uploads=" << pending_uploads_.size();

  // Determine number of bytes to read for this upload iteration, which cannot
  // exceed size of buf i.e. buf_len.
  const int bytes_to_read = std::min(upload_file_info->SizeRemaining(),
                                     upload_file_info->buf_len);

  // Update the content length if the file_size is known.
  if (upload_file_info->download_complete)
    upload_file_info->content_length = upload_file_info->file_size;
  else if (bytes_to_read < kUploadChunkSize) {
    DVLOG(1) << "Paused upload " << upload_file_info->title;
    upload_file_info->upload_paused = true;
    return;
  }

  upload_file_info->file_stream->Read(
      upload_file_info->buf,
      bytes_to_read,
      base::Bind(&GDataUploader::ReadCompletionCallback,
                 uploader_factory_.GetWeakPtr(),
                 upload_file_info->file_url,
                 bytes_to_read));
}

void GDataUploader::ReadCompletionCallback(
    const GURL& file_url,
    int bytes_to_read,
    int bytes_read) {
  // The Read is asynchronously executed on BrowserThread::UI, where
  // Read() was called.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "ReadCompletionCallback bytes read=" << bytes_read;

  UploadFileInfo* upload_file_info = GetUploadFileInfo(file_url);
  if (!upload_file_info)
    return;

  // TODO(achuith): Handle this error.
  DCHECK_EQ(bytes_to_read, bytes_read);
  DCHECK_GT(bytes_read, 0) << "Error reading from file "
                           << upload_file_info->file_path.value();

  upload_file_info->start_range = upload_file_info->end_range + 1;
  upload_file_info->end_range = upload_file_info->start_range +
                                bytes_read - 1;

  file_system_->ResumeUpload(
      ResumeUploadParams(upload_file_info->title,
                         upload_file_info->start_range,
                         upload_file_info->end_range,
                         upload_file_info->content_length,
                         upload_file_info->content_type,
                         upload_file_info->buf,
                         upload_file_info->upload_location,
                         upload_file_info->gdata_path),
      base::Bind(&GDataUploader::OnResumeUploadResponseReceived,
                 uploader_factory_.GetWeakPtr(),
                 upload_file_info->file_url));
}

void GDataUploader::OnResumeUploadResponseReceived(
    const GURL& file_url,
    const ResumeUploadResponse& response) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfo* upload_file_info = GetUploadFileInfo(file_url);
  if (!upload_file_info)
    return;

  if (response.code == HTTP_CREATED) {
    DVLOG(1) << "Successfully created uploaded file=["
             << upload_file_info->title
             << "], with resourceId=[" << response.resource_id
             << "], and md5Checksum=[" << response.md5_checksum << "]";

    // Done uploading.
    DCHECK(!response.resource_id.empty());
    DCHECK(!response.md5_checksum.empty());
    bool ok = file_system_->StoreToCache(response.resource_id,
        response.md5_checksum,
        upload_file_info->file_path,
        CacheOperationCallback());
    DCHECK(ok);
    RemovePendingUpload(upload_file_info);
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
    // - when quota is exceeded, which is 1GB for files not converted to Google
    //   Docs format; even though the quota-exceeded content length
    //   is specified in the header when posting request to get upload
    //   location, the server allows us to upload all chunks of entire file
    //   successfully, but instead of returning 201 (CREATED) status code after
    //   receiving the last chunk, it returns 403 (FORBIDDEN); response content
    //   then will indicate quote exceeded exception.
    NOTREACHED() << "UploadNextChunk http code=" << response.code
                 << ", start_range_received=" << response.start_range_received
                 << ", end_range_received=" << response.end_range_received
                 << ", expected end range=" << upload_file_info->end_range;

    RemovePendingUpload(upload_file_info);
    return;
  }

  DVLOG(1) << "Received range " << response.start_range_received
           << "-" << response.end_range_received
           << " for [" << upload_file_info->title << "]";

  // Continue uploading.
  UploadNextChunk(upload_file_info);
}

}  // namespace gdata
