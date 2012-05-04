// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_download_observer.h"

#include "base/file_util.h"
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
  UploadingExternalData(GDataUploader* uploader, int upload_id)
      : uploader_(uploader),
        upload_id_(upload_id),
        is_complete_(false) {
  }
  virtual ~UploadingExternalData() {}

  void MarkAsComplete() { is_complete_ = true; }

  int upload_id() const { return upload_id_; }
  bool is_complete() const { return is_complete_; }
  GDataUploader* uploader() { return uploader_; }

 private:
  GDataUploader* uploader_;
  int upload_id_;
  bool is_complete_;
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
  return static_cast<UploadingExternalData*>(
      download->GetExternalData(&kUploadingKey));
}

// Extracts GDataExternalData* from |download|.
GDataExternalData* GetGDataExternalData(DownloadItem* download) {
  return static_cast<GDataExternalData*>(
      download->GetExternalData(&kGDataPathKey));
}

}  // namespace

GDataDownloadObserver::GDataDownloadObserver()
    : gdata_uploader_(NULL),
      download_manager_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

GDataDownloadObserver::~GDataDownloadObserver() {
  if (download_manager_)
    download_manager_->RemoveObserver(this);

  for (DownloadMap::iterator iter = pending_downloads_.begin();
       iter != pending_downloads_.end(); ++iter) {
    DetachFromDownload(iter->second);
  }
}

void GDataDownloadObserver::Initialize(
    GDataUploader* gdata_uploader,
    DownloadManager* download_manager,
    const FilePath& gdata_tmp_download_path) {
  DCHECK(gdata_uploader);
  DCHECK(!gdata_tmp_download_path.empty());
  gdata_uploader_ = gdata_uploader;
  download_manager_ = download_manager;
  if (download_manager_)
    download_manager_->AddObserver(this);
  gdata_tmp_download_path_ = gdata_tmp_download_path;
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
  GDataExternalData* data = GetGDataExternalData(download);
  // If data is NULL, we've somehow lost the gdata path selected by the file
  // picker.
  DCHECK(data);
  return data ? util::ExtractGDataPath(data->file_path()) : FilePath();
}

// static
bool GDataDownloadObserver::IsGDataDownload(DownloadItem* download) {
  // We use the existence of the GDataExternalData object in download as a
  // signal that this is a GDataDownload.
  return !!GetGDataExternalData(download);
}

// static
bool GDataDownloadObserver::IsReadyToComplete(DownloadItem* download) {
  // |download| is ready for completion (as far as GData is concerned) if:
  // 1. It's not a GData download.
  //  - or -
  // 2. The upload has completed.
  UploadingExternalData* upload_data = GetUploadingExternalData(download);
  return !IsGDataDownload(download) ||
         (upload_data && upload_data->is_complete());
}

// static
int64 GDataDownloadObserver::GetUploadedBytes(DownloadItem* download) {
  UploadingExternalData* upload_data = GetUploadingExternalData(download);
  if (!upload_data || !upload_data->uploader())
    return 0;
  return upload_data->uploader()->GetUploadedBytes(upload_data->upload_id());
}

// static
int GDataDownloadObserver::PercentComplete(DownloadItem* download) {
  // Progress is unknown until the upload starts.
  if (!GetUploadingExternalData(download))
    return -1;
  int64 complete = GetUploadedBytes(download);
  // Once AllDataSaved() is true, we can use the GetReceivedBytes() as the total
  // size. GetTotalBytes() may be set to 0 if there was a mismatch between the
  // count of received bytes and the size of the download as given by the
  // Content-Length header.
  int64 total = (download->AllDataSaved() ? download->GetReceivedBytes() :
                                            download->GetTotalBytes());
  DCHECK(total <= 0 || complete < total);
  if (total > 0)
    return static_cast<int>((complete * 100.0) / total);
  return -1;
}

// |gdata_tmp_download_path| is set to a temporary local download path in
// ~/GCache/v1/tmp/downloads/
// static
void GDataDownloadObserver::GetGDataTempDownloadPath(
    const FilePath& gdata_tmp_download_dir,
    FilePath* gdata_tmp_download_path) {
  bool created = file_util::CreateDirectory(gdata_tmp_download_dir);
  DCHECK(created) << "Can not create temp download directory at "
                  << gdata_tmp_download_dir.value();
  created = file_util::CreateTemporaryFileInDir(gdata_tmp_download_dir,
                                                gdata_tmp_download_path);
  DCHECK(created) << "Temporary download file creation failed";
}

void GDataDownloadObserver::ManagerGoingDown(
    DownloadManager* download_manager) {
  download_manager->RemoveObserver(this);
  download_manager_ = NULL;
}

void GDataDownloadObserver::ModelChanged(DownloadManager* download_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadManager::DownloadVector downloads;
  // GData downloads are considered temporary downloads.
  download_manager->GetTemporaryDownloads(gdata_tmp_download_path_,
                                          &downloads);
  for (size_t i = 0; i < downloads.size(); ++i) {
    // Only accept downloads that have the GData meta data associated with
    // them. Otherwise we might trip over non-GData downloads being saved to
    // gdata_tmp_download_path_.
    if (IsGDataDownload(downloads[i]))
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
  if (pending_downloads_.count(download->GetId()) == 0) {
    pending_downloads_[download->GetId()] = download;
    download->AddObserver(this);
    DVLOG(1) << "new download total bytes=" << download->GetTotalBytes()
             << ", full path=" << download->GetFullPath().value()
             << ", mime type=" << download->GetMimeType();
  }
}

void GDataDownloadObserver::RemovePendingDownload(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download->IsInProgress());

  DownloadMap::iterator it = pending_downloads_.find(download->GetId());
  if (it != pending_downloads_.end()) {
    DetachFromDownload(download);
    pending_downloads_.erase(it);
  }
}

void GDataDownloadObserver::DetachFromDownload(DownloadItem* download) {
  download->SetExternalData(&kUploadingKey, NULL);
  download->SetExternalData(&kGDataPathKey, NULL);
  download->RemoveObserver(this);
}

void GDataDownloadObserver::UploadDownloadItem(DownloadItem* download) {
  // Update metadata of ongoing upload.
  UpdateUpload(download);

  if (!ShouldUpload(download))
    return;

  scoped_ptr<UploadFileInfo> upload_file_info = CreateUploadFileInfo(download);
  const int upload_id = gdata_uploader_->UploadFile(upload_file_info.Pass());

  // TODO(achuith): Fix this.
  // We won't know the upload ID until the after the GDataUploader::UploadFile()
  // call.
  download->SetExternalData(&kUploadingKey,
      new UploadingExternalData(gdata_uploader_, upload_id));
}

void GDataDownloadObserver::UpdateUpload(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadingExternalData* upload_data = GetUploadingExternalData(download);
  if (!upload_data) {
    DVLOG(1) << "No UploadingExternalData for download " << download->GetId();
    return;
  }

  gdata_uploader_->UpdateUpload(upload_data->upload_id(), download);
}

bool GDataDownloadObserver::ShouldUpload(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Upload if the item is in pending_downloads_,
  // is complete or large enough to stream, and,
  // is not already being uploaded.
  return (pending_downloads_.count(download->GetId()) != 0) &&
         (download->AllDataSaved() ||
          download->GetReceivedBytes() > kStreamingFileSize) &&
         (GetUploadingExternalData(download) == NULL);
}

scoped_ptr<UploadFileInfo> GDataDownloadObserver::CreateUploadFileInfo(
    DownloadItem* download) {
  scoped_ptr<UploadFileInfo> upload_file_info(new UploadFileInfo());

  // GetFullPath will be a temporary location if we're streaming.
  upload_file_info->file_path = download->GetFullPath();
  upload_file_info->file_size = download->GetReceivedBytes();

  // Extract the final path from DownloadItem.
  upload_file_info->gdata_path = GetGDataPath(download);

  // Use the file name as the title.
  upload_file_info->title = upload_file_info->gdata_path.BaseName().value();
  upload_file_info->content_type = download->GetMimeType();
  // GData api handles -1 as unknown file length.
  upload_file_info->content_length = download->AllDataSaved() ?
                                     download->GetReceivedBytes() : -1;

  upload_file_info->all_bytes_present = download->AllDataSaved();

  upload_file_info->completion_callback =
      base::Bind(&GDataDownloadObserver::OnUploadComplete,
                 weak_ptr_factory_.GetWeakPtr(),
                 download->GetId());

  return upload_file_info.Pass();
}

void GDataDownloadObserver::OnUploadComplete(int32 download_id,
                                             base::PlatformFileError error,
                                             UploadFileInfo* upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DownloadMap::iterator iter = pending_downloads_.find(download_id);
  if (iter == pending_downloads_.end()) {
    DVLOG(1) << "Pending download not found" << download_id;
    return;
  }
  DVLOG(1) << "Completing upload for download ID " << download_id;
  DownloadItem* download = iter->second;
  UploadingExternalData* upload_data = GetUploadingExternalData(download);
  DCHECK(upload_data);
  upload_data->MarkAsComplete();
  download->MaybeCompleteDownload();
  // MaybeCompleteDownload() only works for non-SavePackage downloads.
  // SavePackage::Finish() is the SavePackage equivalent of
  // MaybeCompleteDownload(). MHTML SavePackages may upload to GData (see
  // SavePackageFilePickerChromeOS), so MHTML SavePackages use
  // OnDownloadUpdated() to wait for the upload to complete before calling
  // Finish(). Call UpdateObservers() manually now in case this download is an
  // MHTML SavePackage.
  download->UpdateObservers();
}

}  // namespace gdata
