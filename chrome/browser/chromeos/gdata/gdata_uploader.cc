// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_uploader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/gdata/gdata.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

namespace gdata {

GDataUploader::GDataUploader(DocumentsService* docs_service)
  : docs_service_(docs_service) {
}

GDataUploader::~GDataUploader() {
}

void GDataUploader::TestUpload() {
  DVLOG(1) << "Testing uploading of file to Google Docs";

  UploadFileInfo upload_file_info;

  // TODO(kuan): File operations may block; real app should call them on the
  // file thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  // TODO(YOU_testing_upload): Modify the filename, title and content_type of
  // the physical file to upload.
  upload_file_info.title = "Tech Crunch";
  upload_file_info.content_type = "application/pdf";
  FilePath file_path;
  PathService::Get(chrome::DIR_TEST_DATA, &file_path);
  file_path = file_path.AppendASCII("pyauto_private/pdf/TechCrunch.pdf");
  upload_file_info.file_url = GURL(std::string("file:") + file_path.value());

  // Create a FileStream to make sure the file can be opened successfully.
  scoped_ptr<net::FileStream> file(new net::FileStream(NULL));
  int rv = file->OpenSync(
      file_path,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ);
  if (rv != net::OK) {
    // If the file can't be opened, we'll just upload an empty file.
    DLOG(WARNING) << "Error opening \"" << file_path.value()
                  << "\" for reading: " << strerror(rv);
    return;
  }
  // Cache the file stream to be used for actual reading of file later.
  upload_file_info.file_stream = file.release();

  // Obtain the file size for content length.
  int64 file_size;
  if (!file_util::GetFileSize(file_path, &file_size)) {
    DLOG(WARNING) << "Error getting file size for " << file_path.value();
    return;
  }

  upload_file_info.content_length = file_size;

  VLOG(1) << "Uploading file: title=[" << upload_file_info.title
          << "], file_url=[" << upload_file_info.file_url.spec()
          << "], content_type=" << upload_file_info.content_type
          << ", content_length=" << upload_file_info.content_length;

  // GDataUploader's lifetime is tied to the DocumentsService singleton's
  // lifetime, so it's safe to use base::Unretained.
  docs_service_->InitiateUpload(upload_file_info,
      base::Bind(&GDataUploader::OnUploadLocationReceived,
                 base::Unretained(this)));
}

void GDataUploader::UploadNextChunk(
    GDataErrorCode code,
    int64 start_range_received,
    int64 end_range_received,
    UploadFileInfo* upload_file_info) {
  // If code is 308 (RESUME_INCOMPLETE) and range_received is what has been
  // previously uploaded (i.e. = upload_file_info->end_range), proceed to upload
  // the next chunk.
  if (!(code == HTTP_RESUME_INCOMPLETE &&
        start_range_received == 0 &&
        end_range_received == upload_file_info->end_range)) {
    // TODO(kuan): Real app needs to handle error cases, e.g.
    // - when previously uploaded data wasn't received by Google Docs server,
    //   i.e. when end_range_received < upload_file_info->end_range
    // - when quota is exceeded, which is 1GB for files not converted to Google
    //   Docs format; even though the quota-exceeded content length (I tried
    //   1.91GB) is specified in the header when posting request to get upload
    //   location, the server allows us to upload all chunks of entire file
    //   successfully, but instead of returning 201 (CREATED) status code after
    //   receiving the last chunk, it returns 403 (FORBIDDEN); response content
    //   then will indicate quote exceeded exception.
    // File operations may block; real app should call them on the file thread.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    delete upload_file_info->file_stream;
    return;
  }

  // If end_range_received is 0, we're at start of uploading process.
  bool first_upload = end_range_received == 0;

  // Determine number of bytes to read for this upload iteration, which cannot
  // exceed size of buf i.e. buf_len.
  int64 size_remaining = upload_file_info->content_length;
  if (!first_upload)
    size_remaining -= end_range_received + 1;
  int bytes_to_read = std::min(size_remaining,
                               static_cast<int64>(upload_file_info->buf_len));

  // TODO(kuan): Read file operation could be blocking, real app should do it
  // on the file thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  int bytes_read = upload_file_info->file_stream->Read(
      upload_file_info->buf->data(),
      bytes_to_read,
      net::CompletionCallback());
  if (bytes_read <= 0) {
    DVLOG(1) << "Error reading from file "
             << upload_file_info->file_url.spec();
    memset(upload_file_info->buf->data(), 0, bytes_to_read);
    bytes_read = bytes_to_read;
  }

  // Update |upload_file_info| with ranges to upload.
  upload_file_info->start_range = first_upload ? 0 : end_range_received + 1;
  upload_file_info->end_range = upload_file_info->start_range +
                                bytes_read - 1;

  // GDataUploader's lifetime is tied to the DocumentsService singleton's
  // lifetime, so it's safe to use base::Unretained.
  docs_service_->ResumeUpload(*upload_file_info,
               base::Bind(&GDataUploader::OnResumeUploadResponseReceived,
                          base::Unretained(this)));
}

void GDataUploader::OnUploadLocationReceived(
    GDataErrorCode code,
    const UploadFileInfo& in_upload_file_info,
    const GURL& upload_location) {
  DVLOG(1) << "Got upload location [" << upload_location.spec()
           << "] for [" << in_upload_file_info.title << "] ("
           << in_upload_file_info.file_url.spec() << ")";

  UploadFileInfo upload_file_info = in_upload_file_info;

  if (code != HTTP_SUCCESS) {
    // TODO(kuan): Real app should handle error codes from Google Docs server.
    // File operations may block; real app should call them on the file thread.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    delete upload_file_info.file_stream;
    return;
  }

  upload_file_info.upload_location = upload_location;

  // Create buffer to hold upload data.
  // Google Documents List API requires uploading chunk size of 512 kilobytes.
  const size_t kBufSize = 524288;
  upload_file_info.buf_len = std::min(upload_file_info.content_length,
                                      kBufSize);
  upload_file_info.buf = new net::IOBuffer(upload_file_info.buf_len);

  // Change code to RESUME_INCOMPLETE and set range to 0-0, so that
  // UploadNextChunk will start from beginning.
  UploadNextChunk(HTTP_RESUME_INCOMPLETE, 0, 0, &upload_file_info);
}

void GDataUploader::OnResumeUploadResponseReceived(
    GDataErrorCode code,
    const UploadFileInfo& in_upload_file_info,
    int64 start_range_received,
    int64 end_range_received) {
  UploadFileInfo upload_file_info = in_upload_file_info;

  if (code == HTTP_CREATED) {
    DVLOG(1) << "Successfully created uploaded file=["
             << in_upload_file_info.title << "] ("
             << in_upload_file_info.file_url.spec() << ")";
    // We're done with uploading, so file stream is no longer needed.
    // TODO(kuan): File operations may block; real app should call them on the
    // file thread.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    delete upload_file_info.file_stream;
    return;
  }

  DVLOG(1) << "Got received range " << start_range_received
           << "-" << end_range_received
           << " for [" << in_upload_file_info.title << "] ("
           << in_upload_file_info.file_url.spec() << ")";

  // Continue uploading if necessary.
  UploadNextChunk(code, start_range_received, end_range_received,
                  &upload_file_info);
}

}  // namespace gdata
