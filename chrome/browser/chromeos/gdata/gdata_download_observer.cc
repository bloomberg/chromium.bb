// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_download_observer.h"

#include "chrome/browser/chromeos/gdata/gdata_uploader.h"
#include "chrome/browser/chromeos/gdata/gdata_upload_file_info.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "net/base/net_util.h"

using content::BrowserThread;
using content::DownloadManager;
using content::DownloadItem;

namespace gdata {
namespace {

// Threshold file size after which we stream the file.
const int64 kStreamingFileSize = 1 << 20;  // 1MB

// Keys for DownloadItem::ExternalData.
const char kUploadingKey[] = "Uploading";
const char kGDataPathKey[] = "GDataPath";

// External Data stored in DownloadItem for ongoing uploads.
class UploadingExternalData : public DownloadItem::ExternalData {
 public:
  explicit UploadingExternalData(const GURL& url) : file_url_(url) {}
  virtual ~UploadingExternalData() {}

  const GURL& file_url() const { return file_url_; }

 private:
  GURL file_url_;
};

// External Data stored in DownloadItem for gdata path.
class GDataExternalData : public DownloadItem::ExternalData {
 public:
  explicit GDataExternalData(const FilePath& path) : file_path_(path) {}
  virtual ~GDataExternalData() {}

  const FilePath& file_path() const { return file_path_; }

 private:
  FilePath file_path_;
};

// Extracts UploadingExternalData* from |download|.
UploadingExternalData* GetUploadingExternalData(DownloadItem* download) {
  return static_cast<UploadingExternalData*>(download
      ->GetExternalData(&kUploadingKey));
}

}  // namespace

GDataDownloadObserver::GDataDownloadObserver()
    : gdata_uploader_(NULL),
      download_manager_(NULL) {
}

GDataDownloadObserver::~GDataDownloadObserver() {
  if (download_manager_)
    download_manager_->RemoveObserver(this);

  for (DownloadSet::iterator iter = pending_downloads_.begin();
      iter != pending_downloads_.end(); ++iter) {
    (*iter)->RemoveObserver(this);
  }
}

void GDataDownloadObserver::Initialize(GDataUploader* gdata_uploader,
                                       DownloadManager* download_manager) {
  gdata_uploader_ = gdata_uploader;
  download_manager_ = download_manager;
  if (download_manager_)
    download_manager_->AddObserver(this);
}

// static
void GDataDownloadObserver::SetGDataPath(DownloadItem* download,
                                         const FilePath& path) {
  if (download)
      download->SetExternalData(&kGDataPathKey,
                                new GDataExternalData(path));
}

// static
FilePath GDataDownloadObserver::GetGDataPath(DownloadItem* download) {
  GDataExternalData* data = static_cast<GDataExternalData*>(
      download->GetExternalData(&kGDataPathKey));
  // If data is NULL, we've somehow lost the gdata path selected
  // by the file picker.
  DCHECK(data);
  return data ? util::ExtractGDataPath(data->file_path()) : FilePath();
}

void GDataDownloadObserver::ManagerGoingDown(
    DownloadManager* download_manager) {
  download_manager->RemoveObserver(this);
  download_manager_ = NULL;
}

void GDataDownloadObserver::ModelChanged(DownloadManager* download_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadManager::DownloadVector downloads;
  download_manager->GetAllDownloads(util::GetGDataTempDownloadFolderPath(),
                                    &downloads);
  for (size_t i = 0; i < downloads.size(); ++i) {
    OnDownloadUpdated(downloads[i]);
  }
}

void GDataDownloadObserver::OnDownloadUpdated(DownloadItem* download) {
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

void GDataDownloadObserver::AddPendingDownload(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Add ourself as an observer of this download if we've never seen it before.
  if (pending_downloads_.count(download) == 0) {
    pending_downloads_.insert(download);
    download->AddObserver(this);
    DVLOG(1) << "new download total bytes=" << download->GetTotalBytes()
             << ", full path=" << download->GetFullPath().value()
             << ", mime type=" << download->GetMimeType();
  }
}

void GDataDownloadObserver::RemovePendingDownload(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download->IsInProgress());

  DownloadSet::iterator it = pending_downloads_.find(download);
  if (it != pending_downloads_.end()) {
    pending_downloads_.erase(it);
    download->RemoveObserver(this);
  }
}

void GDataDownloadObserver::UploadDownloadItem(DownloadItem* download) {
  // Update metadata of ongoing upload.
  UpdateUpload(download);

  if (!ShouldUpload(download))
    return;

  UploadFileInfo* upload_file_info = CreateUploadFileInfo(download);
  // Set the UploadingKey in |download|.
  download->SetExternalData(&kUploadingKey,
      new UploadingExternalData(upload_file_info->file_url));

  gdata_uploader_->UploadFile(upload_file_info);
}

void GDataDownloadObserver::UpdateUpload(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadingExternalData* external_data = GetUploadingExternalData(download);
  if (!external_data)
    return;

  gdata_uploader_->UpdateUpload(external_data->file_url(),
                                download->GetReceivedBytes(),
                                download->AllDataSaved());
}

bool GDataDownloadObserver::ShouldUpload(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Upload if the item is in pending_downloads_,
  // is complete or large enough to stream, and,
  // is not already being uploaded.
  return pending_downloads_.count(download) != 0 &&
         (download->IsComplete() ||
             download->GetReceivedBytes() > kStreamingFileSize) &&
         GetUploadingExternalData(download) == NULL;
}

UploadFileInfo* GDataDownloadObserver::CreateUploadFileInfo(
    DownloadItem* download) {
  UploadFileInfo* upload_file_info = new UploadFileInfo();

  // GetFullPath will be a temporary location if we're streaming.
  upload_file_info->file_path = download->GetFullPath();
  upload_file_info->file_url = net::FilePathToFileURL(
      upload_file_info->file_path);
  upload_file_info->file_size = download->GetReceivedBytes();

  // Extract the final path from DownloadItem.
  upload_file_info->gdata_path = GetGDataPath(download);

  // Use the file name as the title.
  upload_file_info->title = upload_file_info->gdata_path.BaseName().value();
  upload_file_info->content_type = download->GetMimeType();
  // GData api handles -1 as unknown file length.
  upload_file_info->content_length = download->AllDataSaved() ?
                                     download->GetReceivedBytes() : -1;

  upload_file_info->download_complete = download->AllDataSaved();

  return upload_file_info;
}

}  // namespace gdata
