// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_uploader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "chrome/browser/chromeos/gdata/gdata.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

using content::BrowserThread;
using content::DownloadManager;
using content::DownloadItem;

namespace {

const BrowserThread::ID kGDataAPICallThread = BrowserThread::UI;

}  // namespace

namespace gdata {

GDataUploader::GDataUploader(DocumentsService* docs_service)
  : docs_service_(docs_service) {
}

GDataUploader::~GDataUploader() {
  DCHECK(pending_downloads_.empty());
}

void GDataUploader::Initialize(Profile* profile) {
  DVLOG(1) << "GDataUploader::Initialize";
  DownloadServiceFactory::GetForProfile(profile)
      ->GetDownloadManager()->AddObserver(this);
}

void GDataUploader::UploadFile(const FilePath& file_path,
                               const std::string& content_type,
                               int64 file_size) {
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));

  // Do nothing if --test-gdata is not present in cmd line flags.
  if (!CommandLine::ForCurrentProcess()->HasSwitch("test-gdata"))
    return;

  UploadFileInfo upload_file_info;
  // Use the file name as the title.
  upload_file_info.title = file_path.BaseName().value();
  upload_file_info.content_type = content_type;
  upload_file_info.file_url = net::FilePathToFileURL(file_path);
  upload_file_info.content_length = file_size;

  // Create a FileStream to make sure the file can be opened successfully.
  upload_file_info.file_stream = new net::FileStream(NULL);

  // Open the file asynchronously.
  //
  // DocumentsService is a singleton, and the lifetime of GDataUploader
  // is tied to DocumentsService, so it's safe to use base::Unretained here.
  const int rv = upload_file_info.file_stream->Open(
      file_path,
      base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_ASYNC,
      base::Bind(&GDataUploader::OpenCompletionCallback,
                 base::Unretained(this),
                 file_path,
                 upload_file_info));
  DCHECK_EQ(net::ERR_IO_PENDING, rv);
}

void GDataUploader::OpenCompletionCallback(
    const FilePath& file_path,
    const UploadFileInfo& upload_file_info,
    int result) {
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));

  if (result != net::OK) {
    // If the file can't be opened, we'll do nothing.
    // TODO(achuith): Handle errors properly.
    NOTREACHED() << "Error opening \"" << file_path.value()
                  << "\" for reading: " << strerror(result);
    return;
  }

  DVLOG(1) << "Uploading file: title=[" << upload_file_info.title
           << "], file_url=[" << upload_file_info.file_url.spec()
           << "], content_type=" << upload_file_info.content_type
           << ", content_length=" << upload_file_info.content_length;

  // DocumentsService is a singleton, and the lifetime of GDataUploader
  // is tied to DocumentsService, so it's safe to use base::Unretained here.
  docs_service_->InitiateUpload(
      upload_file_info,
      base::Bind(&GDataUploader::OnUploadLocationReceived,
                 base::Unretained(this)));
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

    // The file stream is closed by the destructor asynchronously.
    delete upload_file_info->file_stream;
    upload_file_info->file_stream = NULL;
    return;
  }

  // If end_range_received is 0, we're at start of uploading process.
  const bool first_upload = end_range_received == 0;

  // Determine number of bytes to read for this upload iteration, which cannot
  // exceed size of buf i.e. buf_len.
  int64 size_remaining = upload_file_info->content_length;
  if (!first_upload)
    size_remaining -= end_range_received + 1;
  const int64 bytes_to_read = std::min(size_remaining,
      static_cast<int64>(upload_file_info->buf_len));

  upload_file_info->start_range = first_upload ? 0 : end_range_received + 1;
  // DocumentsService is a singleton, and the lifetime of GDataUploader
  // is tied to DocumentsService, so it's safe to use base::Unretained here.
  upload_file_info->file_stream->Read(
      upload_file_info->buf,
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
    // The file stream is closed by the destructor asynchronously.
    delete upload_file_info.file_stream;
    upload_file_info.file_stream = NULL;
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
  // The Read is asynchronously executed on kGDataAPICallThread, where
  // Read() was called.
  DCHECK(BrowserThread::CurrentlyOn(kGDataAPICallThread));

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
  docs_service_->ResumeUpload(
      upload_file_info,
      base::Bind(&GDataUploader::OnResumeUploadResponseReceived,
                 base::Unretained(this)));
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
    // The file stream is closed by the destructor asynchronously.
    delete upload_file_info.file_stream;
    upload_file_info.file_stream = NULL;
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

void GDataUploader::ModelChanged(DownloadManager* download_manager) {
  DownloadManager::DownloadVector downloads;
  download_manager->GetAllDownloads(FilePath(), &downloads);
  for (size_t i = 0; i < downloads.size(); ++i) {
    OnDownloadUpdated(downloads[i]);
  }
}

void GDataUploader::OnDownloadUpdated(DownloadItem* download) {
  const DownloadItem::DownloadState state = download->GetState();
  switch (state) {
    case DownloadItem::IN_PROGRESS:
      AddPendingDownload(download);
      break;

    case DownloadItem::COMPLETE:
      if (pending_downloads_.find(download) != pending_downloads_.end()) {
        DVLOG(1) << "finished download received bytes="
                 << download->GetReceivedBytes()
                 << ", full path=" << download->GetFullPath().value()
                 << ", mime type=" << download->GetMimeType();
        DCHECK(download->GetReceivedBytes());
        UploadFile(download->GetFullPath(),
                   download->GetMimeType(),
                   download->GetReceivedBytes());
        RemovePendingDownload(download);
      }
      break;

    case DownloadItem::CANCELLED:
    case DownloadItem::REMOVING:
    case DownloadItem::INTERRUPTED:
      RemovePendingDownload(download);
      break;

    default:
      NOTREACHED();
  }
  DVLOG(1) << "Number of pending downloads=" << pending_downloads_.size();
}

void GDataUploader::AddPendingDownload(DownloadItem* download) {
  // Add ourself as an observer of this download if we've never seen it before.
  if (pending_downloads_.find(download) == pending_downloads_.end()) {
    pending_downloads_.insert(download);
    download->AddObserver(this);
    DVLOG(1) << "new download total bytes=" << download->GetTotalBytes()
             << ", full path=" << download->GetFullPath().value()
             << ", mime type=" << download->GetMimeType();
  }
}

void GDataUploader::RemovePendingDownload(DownloadItem* download) {
  DownloadSet::iterator it = pending_downloads_.find(download);
  if (it != pending_downloads_.end()) {
    pending_downloads_.erase(it);
    download->RemoveObserver(this);
  }
}

}  // namespace gdata
