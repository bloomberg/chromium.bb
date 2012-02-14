// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_uploader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/gdata/gdata.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

using content::BrowserThread;

namespace {

const BrowserThread::ID kGDataAPICallThread = BrowserThread::UI;

// Deletes a file stream.
// net::FileStream::CloseSync needs to be called on the FILE thread.
void DeleteFileStream(net::FileStream* file_stream) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  file_stream->CloseSync();
  delete file_stream;
}

}  // namespace

namespace gdata {

GDataUploader::GDataUploader(DocumentsService* docs_service)
  : docs_service_(docs_service) {
}

GDataUploader::~GDataUploader() {
}

void GDataUploader::TestUpload() {
  // DocumentsService is a singleton, and the lifetime of GDataUploader
  // is tied to DocumentsService, so it's safe to use base::Unretained here.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&GDataUploader::TestUploadOnFileThread,
                 base::Unretained(this)));
}

void GDataUploader::TestUploadOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath file_path;
  PathService::Get(chrome::DIR_TEST_DATA, &file_path);
  file_path = file_path.AppendASCII("pyauto_private/pdf/TechCrunch.pdf");

  UploadFile("Tech Crunch", "application/pdf", file_path);
}

void GDataUploader::UploadFile(const std::string& title,
                               const std::string& content_type,
                               const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  UploadFileInfo upload_file_info;
  upload_file_info.title = title;
  upload_file_info.content_type = content_type;
  upload_file_info.file_url = net::FilePathToFileURL(file_path);

  // Create a FileStream to make sure the file can be opened successfully.
  scoped_ptr<net::FileStream> file(new net::FileStream(NULL));
  const int rv = file->OpenSync(
      file_path,
      base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_ASYNC);
  if (rv != net::OK) {
    // If the file can't be opened, we'll just upload an empty file.
    NOTREACHED() << "Error opening \"" << file_path.value()
                  << "\" for reading: " << strerror(rv);
    return;
  }
  // Cache the file stream to be used for actual reading of file later.
  upload_file_info.file_stream = file.release();

  // Obtain the file size for content length.
  int64 file_size;
  if (!file_util::GetFileSize(file_path, &file_size)) {
    NOTREACHED() << "Error getting file size for " << file_path.value();
    return;
  }

  upload_file_info.content_length = file_size;

  DVLOG(1) << "Uploading file: title=[" << upload_file_info.title
           << "], file_url=[" << upload_file_info.file_url.spec()
           << "], content_type=" << upload_file_info.content_type
           << ", content_length=" << upload_file_info.content_length;

  // DocumentsService is a singleton, and the lifetime of GDataUploader
  // is tied to DocumentsService, so it's safe to use base::Unretained here.
  BrowserThread::PostTask(kGDataAPICallThread, FROM_HERE,
      base::Bind(&DocumentsService::InitiateUpload,
                 base::Unretained(docs_service_),
                 upload_file_info,
                 base::Bind(&GDataUploader::OnUploadLocationReceived,
                            base::Unretained(this))));
}

void GDataUploader::UploadNextChunk(
    GDataErrorCode code,
    int64 start_range_received,
    int64 end_range_received,
    UploadFileInfo* upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));

  // If code is 308 (RESUME_INCOMPLETE) and range_received is what has been
  // previously uploaded (i.e. = upload_file_info->end_range), proceed to upload
  // the next chunk.
  if (!(code == HTTP_RESUME_INCOMPLETE &&
        start_range_received == 0 &&
        end_range_received == upload_file_info->end_range)) {
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
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&DeleteFileStream, upload_file_info->file_stream));
    return;
  }

  // If end_range_received is 0, we're at start of uploading process.
  bool first_upload = end_range_received == 0;

  // Determine number of bytes to read for this upload iteration, which cannot
  // exceed size of buf i.e. buf_len.
  int64 size_remaining = upload_file_info->content_length;
  if (!first_upload)
    size_remaining -= end_range_received + 1;
  int64 bytes_to_read = std::min(size_remaining,
      static_cast<int64>(upload_file_info->buf_len));

  upload_file_info->start_range = first_upload ? 0 : end_range_received + 1;
  // DocumentsService is a singleton, and the lifetime of GDataUploader
  // is tied to DocumentsService, so it's safe to use base::Unretained here.
  upload_file_info->file_stream->Read(
      upload_file_info->buf->data(),
      bytes_to_read,
      base::Bind(&GDataUploader::ReadCompletionCallback,
                 base::Unretained(this),
                 *upload_file_info,
                 bytes_to_read));
}

void GDataUploader::OnUploadLocationReceived(
    GDataErrorCode code,
    const UploadFileInfo& in_upload_file_info,
    const GURL& upload_location) {
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));

  UploadFileInfo upload_file_info = in_upload_file_info;

  DVLOG(1) << "Got upload location [" << upload_location.spec()
           << "] for [" << in_upload_file_info.title << "] ("
           << in_upload_file_info.file_url.spec() << ")";

  if (code != HTTP_SUCCESS) {
    // TODO(achuith): Handle error codes from Google Docs server.
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&DeleteFileStream, upload_file_info.file_stream));
    return;
  }

  upload_file_info.upload_location = upload_location;

  // Create buffer to hold upload data.
  // Google Documents List API requires uploading in chunks of 512kB.
  const size_t kBufSize = 524288;
  upload_file_info.buf_len = std::min(upload_file_info.content_length,
                                      kBufSize);
  upload_file_info.buf = new net::IOBuffer(upload_file_info.buf_len);

  // Change code to RESUME_INCOMPLETE and set range to 0-0, so that
  // UploadNextChunk will start from beginning.
  UploadNextChunk(HTTP_RESUME_INCOMPLETE, 0, 0, &upload_file_info);
}

void GDataUploader::ReadCompletionCallback(
    const UploadFileInfo& in_upload_file_info,
    int64 bytes_to_read,
    int bytes_read) {
  // The Read is asynchronously executed on the FILE thread, which is
  // also where we receive our callback.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  UploadFileInfo upload_file_info = in_upload_file_info;

  // TODO(achuith): Handle this error better.
  if (bytes_read <= 0) {
    // Zero out the bytes in this buffer and continue.
    memset(upload_file_info.buf->data(), 0, bytes_to_read);
    bytes_read = bytes_to_read;
    NOTREACHED() << "Error reading from file "
             << upload_file_info.file_url.spec();
  }

  upload_file_info.end_range = upload_file_info.start_range +
                               bytes_read - 1;

  // DocumentsService is a singleton, and the lifetime of GDataUploader
  // is tied to DocumentsService, so it's safe to use base::Unretained here.
  BrowserThread::PostTask(kGDataAPICallThread, FROM_HERE,
      base::Bind(&DocumentsService::ResumeUpload,
                 base::Unretained(docs_service_),
                 upload_file_info,
                 base::Bind(&GDataUploader::OnResumeUploadResponseReceived,
                            base::Unretained(this))));
}

void GDataUploader::OnResumeUploadResponseReceived(
    GDataErrorCode code,
    const UploadFileInfo& in_upload_file_info,
    int64 start_range_received,
    int64 end_range_received) {
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));

  UploadFileInfo upload_file_info = in_upload_file_info;

  if (code == HTTP_CREATED) {
    DVLOG(1) << "Successfully created uploaded file=["
             << in_upload_file_info.title << "] ("
             << in_upload_file_info.file_url.spec() << ")";
    // We're done with uploading, so file stream is no longer needed.
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&DeleteFileStream, upload_file_info.file_stream));
    return;
  }

  DVLOG(1) << "Received range " << start_range_received
           << "-" << end_range_received
           << " for [" << in_upload_file_info.title << "] ("
           << in_upload_file_info.file_url.spec() << ")";

  // Continue uploading if necessary.
  UploadNextChunk(code, start_range_received, end_range_received,
                  &upload_file_info);
}

}  // namespace gdata
