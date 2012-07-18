// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_download_observer.h"

#include "base/callback.h"
#include "base/file_util.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_documents_service.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system_interface.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata_upload_file_info.h"
#include "chrome/browser/chromeos/gdata/gdata_uploader.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/download/download_completion_blocker.h"
#include "chrome/browser/profiles/profile_manager.h"
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
class UploadingExternalData : public DownloadCompletionBlocker {
 public:
  explicit UploadingExternalData(GDataUploader* uploader)
      : uploader_(uploader),
        upload_id_(-1) {
  }
  virtual ~UploadingExternalData() {}

  GDataUploader* uploader() { return uploader_; }
  void set_upload_id(int upload_id) { upload_id_ = upload_id; }
  int upload_id() const { return upload_id_; }
  void set_virtual_dir_path(const FilePath& path) { virtual_dir_path_ = path; }
  const FilePath& virtual_dir_path() const { return virtual_dir_path_; }
  void set_entry(scoped_ptr<DocumentEntry> entry) { entry_ = entry.Pass(); }
  scoped_ptr<DocumentEntry> entry_passed() { return entry_.Pass(); }

 private:
  GDataUploader* uploader_;
  int upload_id_;
  FilePath virtual_dir_path_;
  scoped_ptr<DocumentEntry> entry_;

  DISALLOW_COPY_AND_ASSIGN(UploadingExternalData);
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

void RunSubstituteGDataDownloadCallback(
    const GDataDownloadObserver::SubstituteGDataDownloadPathCallback& callback,
    const FilePath* file_path) {
  callback.Run(*file_path);
}

gdata::GDataSystemService* GetSystemService(Profile* profile) {
  gdata::GDataSystemService* system_service =
      gdata::GDataSystemServiceFactory::GetForProfile(
        profile ? profile : ProfileManager::GetDefaultProfile());
  DCHECK(system_service);
  return system_service;
}

// Substitutes virtual gdata path for local temporary path.
void SubstituteGDataDownloadPathInternal(Profile* profile,
    const GDataDownloadObserver::SubstituteGDataDownloadPathCallback&
        callback) {
  DVLOG(1) << "SubstituteGDataDownloadPathInternal";

  const FilePath gdata_tmp_download_dir = GetSystemService(profile)->cache()->
      GetCacheDirectoryPath(gdata::GDataCache::CACHE_TYPE_TMP_DOWNLOADS);

  // Swap the gdata path with a local path. Local path must be created
  // on a blocking thread.
  FilePath* gdata_tmp_download_path(new FilePath());
  BrowserThread::GetBlockingPool()->PostTaskAndReply(FROM_HERE,
      base::Bind(&gdata::GDataDownloadObserver::GetGDataTempDownloadPath,
                 gdata_tmp_download_dir,
                 gdata_tmp_download_path),
      base::Bind(&RunSubstituteGDataDownloadCallback,
                 callback,
                 base::Owned(gdata_tmp_download_path)));
}

// Callback for GDataFileSystem::CreateDirectory.
void OnCreateDirectory(const base::Closure& substitute_callback,
                       GDataFileError error) {
  DVLOG(1) << "OnCreateDirectory " << error;
  if (error == GDATA_FILE_OK) {
    substitute_callback.Run();
  } else {
    // TODO(achuith): Handle this.
    NOTREACHED();
  }
}

// Callback for GDataFileSystem::GetEntryInfoByPath.
void OnEntryFound(Profile* profile,
    const FilePath& gdata_dir_path,
    const base::Closure& substitute_callback,
    GDataFileError error,
    scoped_ptr<gdata::GDataEntryProto> entry_proto) {
  if (error == GDATA_FILE_ERROR_NOT_FOUND) {
    // Destination gdata directory doesn't exist, so create it.
    const bool is_exclusive = false, is_recursive = true;
    GetSystemService(profile)->file_system()->CreateDirectory(
        gdata_dir_path, is_exclusive, is_recursive,
        base::Bind(&OnCreateDirectory, substitute_callback));
  } else if (error == GDATA_FILE_OK) {
    substitute_callback.Run();
  } else {
    // TODO(achuith): Handle this.
    NOTREACHED();
  }
}

// Callback for DocumentsService::Authenticate.
void OnAuthenticate(Profile* profile,
                    const FilePath& gdata_path,
                    const base::Closure& substitute_callback,
                    GDataErrorCode error,
                    const std::string& token) {
  DVLOG(1) << "OnAuthenticate";

  if (error == HTTP_SUCCESS) {
    const FilePath gdata_dir_path =
        util::ExtractGDataPath(gdata_path.DirName());
    // Ensure the directory exists. This also forces GDataFileSystem to
    // initialize GDataRootDirectory.
    GetSystemService(profile)->file_system()->GetEntryInfoByPath(
        gdata_dir_path,
        base::Bind(&OnEntryFound, profile, gdata_dir_path,
                   substitute_callback));
  } else {
    // TODO(achuith): Handle this.
    NOTREACHED();
  }
}

}  // namespace

GDataDownloadObserver::GDataDownloadObserver(
    GDataUploader* uploader,
    GDataFileSystemInterface* file_system)
    : gdata_uploader_(uploader),
      file_system_(file_system),
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
    DownloadManager* download_manager,
    const FilePath& gdata_tmp_download_path) {
  DCHECK(!gdata_tmp_download_path.empty());
  download_manager_ = download_manager;
  if (download_manager_)
    download_manager_->AddObserver(this);
  gdata_tmp_download_path_ = gdata_tmp_download_path;
}

// static
void GDataDownloadObserver::SubstituteGDataDownloadPath(Profile* profile,
    const FilePath& gdata_path, content::DownloadItem* download,
    const SubstituteGDataDownloadPathCallback& callback) {
  DVLOG(1) << "SubstituteGDataDownloadPath " << gdata_path.value();

  SetDownloadParams(gdata_path, download);

  if (gdata::util::IsUnderGDataMountPoint(gdata_path)) {
    // Can't access drive if we're not authenticated.
    // We set off a chain of callbacks as follows:
    // DocumentsService::Authenticate
    //   OnAuthenticate calls GDataFileSystem::GetEntryInfoByPath
    //     OnEntryFound calls GDataFileSystem::CreateDirectory (if necessary)
    //       OnCreateDirectory calls SubstituteGDataDownloadPathInternal
    GetSystemService(profile)->docs_service()->Authenticate(
        base::Bind(&OnAuthenticate, profile, gdata_path,
                   base::Bind(&SubstituteGDataDownloadPathInternal,
                              profile, callback)));
  } else {
    callback.Run(gdata_path);
  }
}

// static
void GDataDownloadObserver::SetDownloadParams(const FilePath& gdata_path,
                                              DownloadItem* download) {
  if (!download)
    return;

  if (gdata::util::IsUnderGDataMountPoint(gdata_path)) {
    download->SetExternalData(&kGDataPathKey,
                              new GDataExternalData(gdata_path));
    download->SetDisplayName(gdata_path.BaseName());
    download->SetIsTemporary(true);
  } else if (IsGDataDownload(download)) {
    // This may have been previously set if the default download folder is
    // /drive, and the user has now changed the download target to a local
    // folder.
    download->SetExternalData(&kGDataPathKey, NULL);
    download->SetDisplayName(gdata_path);
    // TODO(achuith): This is not quite right.
    download->SetIsTemporary(false);
  }
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
bool GDataDownloadObserver::IsReadyToComplete(
    DownloadItem* download,
    const base::Closure& complete_callback) {
  DVLOG(1) << "GDataDownloadObserver::IsReadyToComplete";
  // |download| is ready for completion (as far as GData is concerned) if:
  // 1. It's not a GData download.
  //  - or -
  // 2. The upload has completed.
  if (!IsGDataDownload(download))
    return true;
  UploadingExternalData* upload_data = GetUploadingExternalData(download);
  DCHECK(upload_data);
  if (upload_data->is_complete())
    return true;
  upload_data->set_callback(complete_callback);
  return false;
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
      MoveFileToGDataCache(download);
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

  // Initialize uploading external data.
  download->SetExternalData(&kUploadingKey,
                            new UploadingExternalData(gdata_uploader_));

  // Create UploadFileInfo structure for the download item.
  CreateUploadFileInfo(download);
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

void GDataDownloadObserver::CreateUploadFileInfo(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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

  // Get the GDataDirectory proto for the upload directory, then extract the
  // initial upload URL in OnReadDirectoryByPath().
  const FilePath upload_dir = upload_file_info->gdata_path.DirName();
  file_system_->ReadDirectoryByPath(
      upload_dir,
      base::Bind(&GDataDownloadObserver::OnReadDirectoryByPath,
                 weak_ptr_factory_.GetWeakPtr(),
                 download->GetId(),
                 base::Passed(&upload_file_info)));
}

void GDataDownloadObserver::OnReadDirectoryByPath(
    int32 download_id,
    scoped_ptr<UploadFileInfo> upload_file_info,
    GDataFileError error,
    bool /* hide_hosted_documents */,
    scoped_ptr<GDataDirectoryProto> dir_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(upload_file_info.get());

  // TODO(hshi): if the upload directory is no longer valid, use the root
  // directory instead.
  upload_file_info->initial_upload_location =
      dir_proto.get() ? GURL(dir_proto->gdata_entry().upload_url()) : GURL();

  StartUpload(download_id, upload_file_info.Pass());
}

void GDataDownloadObserver::StartUpload(
    int32 download_id,
    scoped_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(upload_file_info.get());

  // Look up the DownloadItem for the |download_id|.
  DownloadMap::iterator iter = pending_downloads_.find(download_id);
  if (iter == pending_downloads_.end()) {
    DVLOG(1) << "Pending download not found" << download_id;
    return;
  }
  DVLOG(1) << "Starting upload for download ID " << download_id;
  DownloadItem* download_item = iter->second;

  UploadingExternalData* upload_data = GetUploadingExternalData(download_item);
  DCHECK(upload_data);
  upload_data->set_virtual_dir_path(upload_file_info->gdata_path.DirName());

  // Start upload and save the upload id for future reference.
  const int upload_id = gdata_uploader_->UploadNewFile(upload_file_info.Pass());
  upload_data->set_upload_id(upload_id);
}

void GDataDownloadObserver::OnUploadComplete(
    int32 download_id,
    GDataFileError error,
    scoped_ptr<UploadFileInfo> upload_file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(upload_file_info.get());
  DCHECK(upload_file_info->entry.get());

  // Look up the DownloadItem for the |download_id|.
  DownloadMap::iterator iter = pending_downloads_.find(download_id);
  if (iter == pending_downloads_.end()) {
    DVLOG(1) << "Pending download not found" << download_id;
    return;
  }
  DVLOG(1) << "Completing upload for download ID " << download_id;
  DownloadItem* download_item = iter->second;

  UploadingExternalData* upload_data = GetUploadingExternalData(download_item);
  DCHECK(upload_data);

  // Take ownership of the DocumentEntry from UploadFileInfo. This is used by
  // GDataFileSystem::AddUploadedFile() to add the entry to GDataCache after the
  // upload completes.
  upload_data->set_entry(upload_file_info->entry.Pass());

  // Allow the download item to complete.
  upload_data->CompleteDownload();
}

void GDataDownloadObserver::MoveFileToGDataCache(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadingExternalData* upload_data = GetUploadingExternalData(download);
  if (!upload_data) {
    NOTREACHED();
    return;
  }

  // Pass ownership of the DocumentEntry object to AddUploadedFile().
  scoped_ptr<DocumentEntry> entry = upload_data->entry_passed();
  if (!entry.get()) {
    NOTREACHED();
    return;
  }

  // Move downloaded file to gdata cache. Note that |content_file_path| should
  // use the final target path when the download item is in COMPLETE state.
  file_system_->AddUploadedFile(UPLOAD_NEW_FILE,
                                upload_data->virtual_dir_path(),
                                entry.Pass(),
                                download->GetTargetFilePath(),
                                GDataCache::FILE_OPERATION_MOVE,
                                base::Bind(&base::DoNothing));
}

}  // namespace gdata
