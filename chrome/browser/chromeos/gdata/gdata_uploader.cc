// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_uploader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/gdata/gdata.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_upload_file_info.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

using content::BrowserThread;
using content::DownloadManager;
using content::DownloadItem;

namespace gdata {

namespace {

// Threshold file size after which we stream the file.
const int64 kStreamingFileSize = 1 << 20;  // 1MB
// Google Documents List API requires uploading in chunks of 512kB.
const size_t kUploadChunkSize = 512 * 1024;

// Key for DownloadItem::ExternalData.
const char kUploadingKey[] = "Uploading";

// External Data stored in DownloadItem for ongoing uploads.
class UploadingExternalData : public DownloadItem::ExternalData {
 public:
  explicit UploadingExternalData(const GURL& url) : file_url_(url) {}
  virtual ~UploadingExternalData() {}

  const GURL& file_url() const { return file_url_; }

 private:
  GURL file_url_;
};

// Extracts UploadingExternalData* from |download|.
// static
UploadingExternalData* GetUploadingExternalData(DownloadItem* download) {
  return static_cast<UploadingExternalData*>(download
      ->GetExternalData(&kUploadingKey));
}

}  // namespace

GDataUploader::GDataUploader(GDataFileSystem* file_system)
  : file_system_(file_system),
    download_manager_(NULL),
    ALLOW_THIS_IN_INITIALIZER_LIST(uploader_factory_(this)) {
}

GDataUploader::~GDataUploader() {
  if (download_manager_)
    download_manager_->RemoveObserver(this);

  for (DownloadSet::iterator it = pending_downloads_.begin();
      it != pending_downloads_.end();
      ++it) {
    (*it)->RemoveObserver(this);
  }
}

void GDataUploader::Initialize(Profile* profile) {
  if (!g_browser_process->download_status_updater())
    return;

  download_manager_ =
      DownloadServiceFactory::GetForProfile(profile)->GetDownloadManager();
  download_manager_->AddObserver(this);
}

void GDataUploader::ManagerGoingDown(DownloadManager* download_manager) {
  download_manager->RemoveObserver(this);
  download_manager_ = NULL;
}

void GDataUploader::ModelChanged(DownloadManager* download_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadManager::DownloadVector downloads;
  download_manager->GetAllDownloads(FilePath(), &downloads);
  for (size_t i = 0; i < downloads.size(); ++i) {
    OnDownloadUpdated(downloads[i]);
  }
}

void GDataUploader::OnDownloadUpdated(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const DownloadItem::DownloadState state = download->GetState();
  switch (state) {
    case DownloadItem::IN_PROGRESS:
      AddPendingDownload(download);
      UploadDownloadItem(download);
      break;

    case DownloadItem::COMPLETE:
      UploadDownloadItem(download);
      RemovePendingDownload(download);
      break;

    // TODO(achuith): Stop the pending upload and delete the file.
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download->IsInProgress());

  DownloadSet::iterator it = pending_downloads_.find(download);
  if (it != pending_downloads_.end()) {
    pending_downloads_.erase(it);
    download->RemoveObserver(this);
  }
}

void GDataUploader::UploadDownloadItem(DownloadItem* download) {
  // Update metadata of ongoing upload.
  UpdateUpload(download);

  if (!ShouldUpload(download))
    return;

  DVLOG(1) << "Starting upload"
           << ", full path=" << download->GetFullPath().value()
           << ", received bytes=" << download->GetReceivedBytes()
           << ", total bytes=" << download->GetTotalBytes()
           << ", mime type=" << download->GetMimeType();

  UploadFileInfo* upload_file_info = new UploadFileInfo();
  upload_file_info->Init(download);
  const GURL& file_url = upload_file_info->file_url;
  pending_uploads_.insert(std::make_pair(file_url, upload_file_info));
  // Set the UploadingKey in |download|.
  download->SetExternalData(&kUploadingKey,
                            new UploadingExternalData(file_url));

  UploadFile(file_url);
}

void GDataUploader::UpdateUpload(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadingExternalData* external_data = GetUploadingExternalData(download);
  if (!external_data)
    return;

  UploadFileInfo* upload_file_info =
      GetUploadFileInfo(external_data->file_url());
  if (!upload_file_info)
    return;

  // Update file_size and download_complete.
  DVLOG(1) << "Updating file size from " << upload_file_info->file_size
           << " to " << download->GetReceivedBytes();
  upload_file_info->file_size = download->GetReceivedBytes();
  upload_file_info->download_complete = download->IsComplete();
}

bool GDataUploader::ShouldUpload(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Upload if the item is in pending_downloads_,
  // is complete or large enough to stream, and,
  // is not already being uploaded.
  return pending_downloads_.find(download) != pending_downloads_.end() &&
         (download->IsComplete() ||
             download->GetReceivedBytes() > kStreamingFileSize) &&
         GetUploadingExternalData(download) == NULL;
}

void GDataUploader::UploadFile(const GURL& file_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfo* upload_file_info = GetUploadFileInfo(file_url);
  if (!upload_file_info)
    return;

  DVLOG(1) << "Uploading file: title=[" << upload_file_info->title
           << "], file_path=[" << upload_file_info->file_path.value()
           << "], content_type=" << upload_file_info->content_type
           << ", file_size=" << upload_file_info->file_size;

  // Do nothing if --test-gdata is not present in cmd line flags.
  if (!CommandLine::ForCurrentProcess()->HasSwitch("test-gdata"))
    return;

  // Create a FileStream to make sure the file can be opened successfully.
  upload_file_info->file_stream = new net::FileStream(NULL);

  // Create buffer to hold upload data.
  upload_file_info->buf_len = std::min(upload_file_info->file_size,
                                       kUploadChunkSize);
  upload_file_info->buf = new net::IOBuffer(upload_file_info->buf_len);

  // Open the file asynchronously.
  const int rv = upload_file_info->file_stream->Open(
      upload_file_info->file_path,
      base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_ASYNC,
      base::Bind(&GDataUploader::OpenCompletionCallback,
                 uploader_factory_.GetWeakPtr(),
                 file_url));
  DCHECK_EQ(net::ERR_IO_PENDING, rv);
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

void GDataUploader::OpenCompletionCallback(const GURL& file_url, int result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfo* upload_file_info = GetUploadFileInfo(file_url);
  if (!upload_file_info)
    return;

  if (result != net::OK) {
    // If the file can't be opened, we'll do nothing.
    // TODO(achuith): Handle errors properly.
    NOTREACHED() << "Error opening \"" << upload_file_info->file_path.value()
                 << "\" for reading: " << strerror(result);
    return;
  }

  // TODO(achuith): Wire the correct destination directory here.
  FilePath destination_dir_path(FILE_PATH_LITERAL("gdata"));

  file_system_->InitiateUpload(
      upload_file_info->title,
      upload_file_info->content_type,
      upload_file_info->content_length,
      destination_dir_path,
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
    return;
  }

  upload_file_info->upload_location = upload_location;

  // Start the upload from the beginning of the file.
  // start_range:end_range is inclusive, so for example, the range 0:8 is 9
  // bytes. 0:-1 represents 0 bytes, which is what we want here. The arithmetic
  // in UploadNextChunk works out correctly with end_range -1.
  UploadNextChunk(HTTP_RESUME_INCOMPLETE, 0, -1, upload_file_info);
}

void GDataUploader::UploadNextChunk(
    GDataErrorCode code,
    int64 start_range_received,
    int64 end_range_received,
    UploadFileInfo* upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Check that |upload_file_info| is in pending_uploads_.
  DCHECK(upload_file_info == GetUploadFileInfo(upload_file_info->file_url));

  // If code is 308 (RESUME_INCOMPLETE) and range_received is what has been
  // previously uploaded (i.e. = upload_file_info->end_range), proceed to upload
  // the next chunk.
  if (code != HTTP_RESUME_INCOMPLETE ||
      start_range_received != 0 ||
      end_range_received != upload_file_info->end_range) {
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
    NOTREACHED() << "UploadNextChunk http code=" << code
                 << ", start_range_received=" << start_range_received
                 << ", end_range_received=" << end_range_received
                 << ", expected end range=" << upload_file_info->end_range;

    RemovePendingUpload(upload_file_info);
    return;
  }

  DVLOG(1) << "Number of pending uploads=" << pending_uploads_.size();

  // Determine number of bytes to read for this upload iteration, which cannot
  // exceed size of buf i.e. buf_len.
  // Note that end_range = uploaded_bytes - 1.
  const size_t size_remaining = upload_file_info->file_size
                                - end_range_received - 1;
  const int bytes_to_read = std::min(size_remaining,
                                     upload_file_info->buf_len);

  // Update the content length if the file_size is known.
  // TODO(achuith): Handle the case where the upload catches up to the download.
  if (upload_file_info->download_complete)
    upload_file_info->content_length = upload_file_info->file_size;
  else
    DCHECK(size_remaining > kUploadChunkSize);

  upload_file_info->start_range = end_range_received + 1;
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

  upload_file_info->end_range = upload_file_info->start_range +
                               bytes_read - 1;

  file_system_->ResumeUpload(
      ResumeUploadParams(upload_file_info->title,
                         upload_file_info->start_range,
                         upload_file_info->end_range,
                         upload_file_info->content_length,
                         upload_file_info->content_type,
                         upload_file_info->buf,
                         upload_file_info->upload_location),
      base::Bind(&GDataUploader::OnResumeUploadResponseReceived,
                 uploader_factory_.GetWeakPtr(),
                 upload_file_info->file_url));
}

void GDataUploader::OnResumeUploadResponseReceived(
    const GURL& file_url,
    GDataErrorCode code,
    int64 start_range_received,
    int64 end_range_received) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadFileInfo* upload_file_info = GetUploadFileInfo(file_url);
  if (!upload_file_info)
    return;

  if (code != HTTP_CREATED) {
    DVLOG(1) << "Received range " << start_range_received
             << "-" << end_range_received
             << " for [" << upload_file_info->title << "]";

    // Continue uploading.
    UploadNextChunk(code, start_range_received, end_range_received,
                    upload_file_info);
    return;
  }

  DVLOG(1) << "Successfully created uploaded file=["
           << upload_file_info->title << "]";

  // Done uploading.
  RemovePendingUpload(upload_file_info);
}

}  // namespace gdata
