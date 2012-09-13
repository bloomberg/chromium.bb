// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_download_observer.h"

#include <string>

#include "base/callback.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_interface.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/gdata/drive_service_interface.h"
#include "chrome/browser/chromeos/gdata/drive_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
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

// Keys for base::SupportsUserData::Data.
const char kUploadingKey[] = "Uploading";
const char kGDataPathKey[] = "GDataPath";

// User Data stored in DownloadItem for ongoing uploads.
class UploadingUserData : public DownloadCompletionBlocker {
 public:
  explicit UploadingUserData(DriveUploader* uploader)
      : uploader_(uploader),
        upload_id_(-1),
        is_overwrite_(false) {
  }
  virtual ~UploadingUserData() {}

  DriveUploader* uploader() { return uploader_; }
  void set_upload_id(int upload_id) { upload_id_ = upload_id; }
  int upload_id() const { return upload_id_; }
  void set_virtual_dir_path(const FilePath& path) { virtual_dir_path_ = path; }
  const FilePath& virtual_dir_path() const { return virtual_dir_path_; }
  void set_entry(scoped_ptr<DocumentEntry> entry) { entry_ = entry.Pass(); }
  scoped_ptr<DocumentEntry> entry_passed() { return entry_.Pass(); }
  void set_overwrite(bool overwrite) { is_overwrite_ = overwrite; }
  bool is_overwrite() { return is_overwrite_; }
  void set_resource_id(const std::string& resource_id) {
    resource_id_ = resource_id;
  }
  const std::string& resource_id() const { return resource_id_; }
  void set_md5(const std::string& md5) { md5_ = md5; }
  const std::string& md5() const { return md5_; }

 private:
  DriveUploader* uploader_;
  int upload_id_;
  FilePath virtual_dir_path_;
  scoped_ptr<DocumentEntry> entry_;
  bool is_overwrite_;
  std::string resource_id_;
  std::string md5_;

  DISALLOW_COPY_AND_ASSIGN(UploadingUserData);
};

// User Data stored in DownloadItem for drive path.
class DriveUserData : public base::SupportsUserData::Data {
 public:
  explicit DriveUserData(const FilePath& path) : file_path_(path) {}
  virtual ~DriveUserData() {}

  const FilePath& file_path() const { return file_path_; }

 private:
  FilePath file_path_;
};

// Extracts UploadingUserData* from |download|.
UploadingUserData* GetUploadingUserData(DownloadItem* download) {
  return static_cast<UploadingUserData*>(
      download->GetUserData(&kUploadingKey));
}

// Extracts DriveUserData* from |download|.
DriveUserData* GetDriveUserData(DownloadItem* download) {
  return static_cast<DriveUserData*>(
      download->GetUserData(&kGDataPathKey));
}

void RunSubstituteDriveDownloadCallback(
    const DriveDownloadObserver::SubstituteDriveDownloadPathCallback& callback,
    const FilePath* file_path) {
  callback.Run(*file_path);
}

DriveSystemService* GetSystemService(Profile* profile) {
  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(
        profile ? profile : ProfileManager::GetDefaultProfile());
  DCHECK(system_service);
  return system_service;
}

// Substitutes virtual drive path for local temporary path.
void SubstituteDriveDownloadPathInternal(Profile* profile,
    const DriveDownloadObserver::SubstituteDriveDownloadPathCallback&
        callback) {
  DVLOG(1) << "SubstituteDriveDownloadPathInternal";

  const FilePath drive_tmp_download_dir = GetSystemService(profile)->cache()->
      GetCacheDirectoryPath(DriveCache::CACHE_TYPE_TMP_DOWNLOADS);

  // Swap the drive path with a local path. Local path must be created
  // on a blocking thread.
  FilePath* drive_tmp_download_path(new FilePath());
  BrowserThread::GetBlockingPool()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&DriveDownloadObserver::GetDriveTempDownloadPath,
                 drive_tmp_download_dir,
                 drive_tmp_download_path),
      base::Bind(&RunSubstituteDriveDownloadCallback,
                 callback,
                 base::Owned(drive_tmp_download_path)));
}

// Callback for DriveFileSystem::CreateDirectory.
void OnCreateDirectory(const base::Closure& substitute_callback,
                       DriveFileError error) {
  DVLOG(1) << "OnCreateDirectory " << error;
  if (error == DRIVE_FILE_OK) {
    substitute_callback.Run();
  } else {
    // TODO(achuith): Handle this.
    NOTREACHED();
  }
}

// Callback for DriveFileSystem::GetEntryInfoByPath.
void OnEntryFound(Profile* profile,
    const FilePath& drive_dir_path,
    const base::Closure& substitute_callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  if (error == DRIVE_FILE_ERROR_NOT_FOUND) {
    // Destination Drive directory doesn't exist, so create it.
    const bool is_exclusive = false, is_recursive = true;
    GetSystemService(profile)->file_system()->CreateDirectory(
        drive_dir_path, is_exclusive, is_recursive,
        base::Bind(&OnCreateDirectory, substitute_callback));
  } else if (error == DRIVE_FILE_OK) {
    substitute_callback.Run();
  } else {
    // TODO(achuith): Handle this.
    NOTREACHED();
  }
}

// Callback for DriveServiceInterface::Authenticate.
void OnAuthenticate(Profile* profile,
                    const FilePath& drive_path,
                    const base::Closure& substitute_callback,
                    GDataErrorCode error,
                    const std::string& token) {
  DVLOG(1) << "OnAuthenticate";

  if (error == HTTP_SUCCESS) {
    const FilePath drive_dir_path =
        util::ExtractDrivePath(drive_path.DirName());
    // Ensure the directory exists. This also forces DriveFileSystem to
    // initialize DriveRootDirectory.
    GetSystemService(profile)->file_system()->GetEntryInfoByPath(
        drive_dir_path,
        base::Bind(&OnEntryFound, profile, drive_dir_path,
                   substitute_callback));
  } else {
    // TODO(achuith): Handle this.
    NOTREACHED();
  }
}

}  // namespace

DriveDownloadObserver::DriveDownloadObserver(
    DriveUploader* uploader,
    DriveFileSystemInterface* file_system)
    : drive_uploader_(uploader),
      file_system_(file_system),
      download_manager_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

DriveDownloadObserver::~DriveDownloadObserver() {
  if (download_manager_)
    download_manager_->RemoveObserver(this);

  for (DownloadMap::iterator iter = pending_downloads_.begin();
       iter != pending_downloads_.end(); ++iter) {
    DetachFromDownload(iter->second);
  }
}

void DriveDownloadObserver::Initialize(
    DownloadManager* download_manager,
    const FilePath& drive_tmp_download_path) {
  DCHECK(!drive_tmp_download_path.empty());
  download_manager_ = download_manager;
  if (download_manager_)
    download_manager_->AddObserver(this);
  drive_tmp_download_path_ = drive_tmp_download_path;
}

// static
void DriveDownloadObserver::SubstituteDriveDownloadPath(Profile* profile,
    const FilePath& drive_path, content::DownloadItem* download,
    const SubstituteDriveDownloadPathCallback& callback) {
  DVLOG(1) << "SubstituteDriveDownloadPath " << drive_path.value();

  SetDownloadParams(drive_path, download);

  if (util::IsUnderDriveMountPoint(drive_path)) {
    // Can't access drive if we're not authenticated.
    // We set off a chain of callbacks as follows:
    // DriveServiceInterface::Authenticate
    //   OnAuthenticate calls DriveFileSystem::GetEntryInfoByPath
    //     OnEntryFound calls DriveFileSystem::CreateDirectory (if necessary)
    //       OnCreateDirectory calls SubstituteDriveDownloadPathInternal
    GetSystemService(profile)->drive_service()->Authenticate(
        base::Bind(&OnAuthenticate, profile, drive_path,
                   base::Bind(&SubstituteDriveDownloadPathInternal,
                              profile, callback)));
  } else {
    callback.Run(drive_path);
  }
}

// static
void DriveDownloadObserver::SetDownloadParams(const FilePath& drive_path,
                                              DownloadItem* download) {
  if (!download)
    return;

  if (util::IsUnderDriveMountPoint(drive_path)) {
    download->SetUserData(&kGDataPathKey,
                          new DriveUserData(drive_path));
    download->SetDisplayName(drive_path.BaseName());
    download->SetIsTemporary(true);
  } else if (IsDriveDownload(download)) {
    // This may have been previously set if the default download folder is
    // /drive, and the user has now changed the download target to a local
    // folder.
    download->SetUserData(&kGDataPathKey, NULL);
    download->SetDisplayName(drive_path);
    // TODO(achuith): This is not quite right.
    download->SetIsTemporary(false);
  }
}

// static
FilePath DriveDownloadObserver::GetDrivePath(DownloadItem* download) {
  DriveUserData* data = GetDriveUserData(download);
  // If data is NULL, we've somehow lost the drive path selected by the file
  // picker.
  DCHECK(data);
  return data ? util::ExtractDrivePath(data->file_path()) : FilePath();
}

// static
bool DriveDownloadObserver::IsDriveDownload(DownloadItem* download) {
  // We use the existence of the DriveUserData object in download as a
  // signal that this is a DriveDownload.
  return !!GetDriveUserData(download);
}

// static
bool DriveDownloadObserver::IsReadyToComplete(
    DownloadItem* download,
    const base::Closure& complete_callback) {
  DVLOG(1) << "DriveDownloadObserver::IsReadyToComplete";
  // |download| is ready for completion (as far as Drive is concerned) if:
  // 1. It's not a Drive download.
  //  - or -
  // 2. The upload has completed.
  if (!IsDriveDownload(download))
    return true;
  UploadingUserData* upload_data = GetUploadingUserData(download);
  DCHECK(upload_data);
  if (upload_data->is_complete())
    return true;
  upload_data->set_callback(complete_callback);
  return false;
}

// static
int64 DriveDownloadObserver::GetUploadedBytes(DownloadItem* download) {
  UploadingUserData* upload_data = GetUploadingUserData(download);
  if (!upload_data || !upload_data->uploader())
    return 0;
  return upload_data->uploader()->GetUploadedBytes(upload_data->upload_id());
}

// static
int DriveDownloadObserver::PercentComplete(DownloadItem* download) {
  // Progress is unknown until the upload starts.
  if (!GetUploadingUserData(download))
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

// |drive_tmp_download_path| is set to a temporary local download path in
// ~/GCache/v1/tmp/downloads/
// static
void DriveDownloadObserver::GetDriveTempDownloadPath(
    const FilePath& drive_tmp_download_dir,
    FilePath* drive_tmp_download_path) {
  bool created = file_util::CreateDirectory(drive_tmp_download_dir);
  DCHECK(created) << "Can not create temp download directory at "
                  << drive_tmp_download_dir.value();
  created = file_util::CreateTemporaryFileInDir(drive_tmp_download_dir,
                                                drive_tmp_download_path);
  DCHECK(created) << "Temporary download file creation failed";
}

void DriveDownloadObserver::ManagerGoingDown(
    DownloadManager* download_manager) {
  download_manager->RemoveObserver(this);
  download_manager_ = NULL;
}

void DriveDownloadObserver::ModelChanged(DownloadManager* download_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadManager::DownloadVector downloads;
  // Drive downloads are considered temporary downloads.
  download_manager->GetAllDownloads(&downloads);
  for (size_t i = 0; i < downloads.size(); ++i) {
    // Only accept downloads that have the Drive meta data associated with
    // them. Otherwise we might trip over non-Drive downloads being saved to
    // drive_tmp_download_path_.
    if (downloads[i]->IsTemporary() &&
        (downloads[i]->GetTargetFilePath().DirName() ==
         drive_tmp_download_path_) &&
        IsDriveDownload(downloads[i]))
      OnDownloadUpdated(downloads[i]);
  }
}

void DriveDownloadObserver::OnDownloadUpdated(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const DownloadItem::DownloadState state = download->GetState();
  switch (state) {
    case DownloadItem::IN_PROGRESS:
      AddPendingDownload(download);
      UploadDownloadItem(download);
      break;

    case DownloadItem::COMPLETE:
      UploadDownloadItem(download);
      MoveFileToDriveCache(download);
      RemovePendingDownload(download);
      break;

    // TODO(achuith): Stop the pending upload and delete the file.
    case DownloadItem::CANCELLED:
    case DownloadItem::INTERRUPTED:
      RemovePendingDownload(download);
      break;

    default:
      NOTREACHED();
  }

  DVLOG(1) << "Number of pending downloads=" << pending_downloads_.size();
}

void DriveDownloadObserver::OnDownloadDestroyed(DownloadItem* download) {
  RemovePendingDownload(download);
}

void DriveDownloadObserver::AddPendingDownload(DownloadItem* download) {
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

void DriveDownloadObserver::RemovePendingDownload(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download->IsInProgress());

  DownloadMap::iterator it = pending_downloads_.find(download->GetId());
  if (it != pending_downloads_.end()) {
    DetachFromDownload(download);
    pending_downloads_.erase(it);
  }
}

void DriveDownloadObserver::DetachFromDownload(DownloadItem* download) {
  download->SetUserData(&kUploadingKey, NULL);
  download->SetUserData(&kGDataPathKey, NULL);
  download->RemoveObserver(this);
}

void DriveDownloadObserver::UploadDownloadItem(DownloadItem* download) {
  // Update metadata of ongoing upload.
  UpdateUpload(download);

  if (!ShouldUpload(download))
    return;

  // Initialize uploading userdata.
  download->SetUserData(&kUploadingKey,
                            new UploadingUserData(drive_uploader_));

  // Create UploaderParams structure for the download item.
  CreateUploaderParams(download);
}

void DriveDownloadObserver::UpdateUpload(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadingUserData* upload_data = GetUploadingUserData(download);
  if (!upload_data) {
    DVLOG(1) << "No UploadingUserData for download " << download->GetId();
    return;
  }

  drive_uploader_->UpdateUpload(upload_data->upload_id(), download);
}

bool DriveDownloadObserver::ShouldUpload(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Upload if the item is in pending_downloads_,
  // has a filename,
  // is complete or large enough to stream, and,
  // is not already being uploaded.
  return (pending_downloads_.count(download->GetId()) != 0) &&
         !download->GetFullPath().empty() &&
         (download->AllDataSaved() ||
          download->GetReceivedBytes() > kStreamingFileSize) &&
         (GetUploadingUserData(download) == NULL);
}

void DriveDownloadObserver::CreateUploaderParams(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<UploaderParams> uploader_params(new UploaderParams());

  // GetFullPath will be a temporary location if we're streaming.
  uploader_params->file_path = download->GetFullPath();
  uploader_params->file_size = download->GetReceivedBytes();

  // Extract the final path from DownloadItem.
  uploader_params->drive_path = GetDrivePath(download);

  // Use the file name as the title.
  uploader_params->title = uploader_params->drive_path.BaseName().value();
  uploader_params->content_type = download->GetMimeType();
  // GData api handles -1 as unknown file length.
  uploader_params->content_length = download->AllDataSaved() ?
                                    download->GetReceivedBytes() : -1;

  uploader_params->all_bytes_present = download->AllDataSaved();

  uploader_params->completion_callback =
      base::Bind(&DriveDownloadObserver::OnUploadComplete,
                 weak_ptr_factory_.GetWeakPtr(),
                 download->GetId());

  // First check if |path| already exists. If so, we'll be overwriting an
  // existing file.
  const FilePath path = uploader_params->drive_path;
  file_system_->GetEntryInfoByPath(
      path,
      base::Bind(
          &DriveDownloadObserver::CreateUploaderParamsAfterCheckExistence,
          weak_ptr_factory_.GetWeakPtr(),
          download->GetId(),
          base::Passed(&uploader_params)));
}

void DriveDownloadObserver::CreateUploaderParamsAfterCheckExistence(
    int32 download_id,
    scoped_ptr<UploaderParams> uploader_params,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(uploader_params.get());

  if (entry_proto.get()) {
    // Make sure this isn't a directory.
    if (entry_proto->file_info().is_directory()) {
      DVLOG(1) << "Filename conflicts with existing directory: "
               << uploader_params->title;
      return;
    }

    // An entry already exists at the target path, so overwrite the existing
    // file.
    uploader_params->upload_location = GURL(entry_proto->upload_url());
    uploader_params->title = "";

    // Look up the DownloadItem for the |download_id|.
    DownloadMap::iterator iter = pending_downloads_.find(download_id);
    if (iter == pending_downloads_.end()) {
      DVLOG(1) << "Pending download not found" << download_id;
      return;
    }
    DownloadItem* download_item = iter->second;

    UploadingUserData* upload_data = GetUploadingUserData(download_item);
    DCHECK(upload_data);

    upload_data->set_resource_id(entry_proto->resource_id());
    upload_data->set_md5(entry_proto->file_specific_info().file_md5());
    upload_data->set_overwrite(true);

    StartUpload(download_id, uploader_params.Pass());
  } else {
    // No file exists at the target path, so upload as a new file.

    // Get the DriveDirectory proto for the upload directory, then extract the
    // initial upload URL in OnReadDirectoryByPath().
    const FilePath upload_dir = uploader_params->drive_path.DirName();
    file_system_->GetEntryInfoByPath(
        upload_dir,
        base::Bind(
            &DriveDownloadObserver::CreateUploaderParamsAfterCheckTargetDir,
            weak_ptr_factory_.GetWeakPtr(),
            download_id,
            base::Passed(&uploader_params)));
  }
}

void DriveDownloadObserver::CreateUploaderParamsAfterCheckTargetDir(
    int32 download_id,
    scoped_ptr<UploaderParams> uploader_params,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(uploader_params.get());

  if (entry_proto.get()) {
    // TODO(hshi): if the upload directory is no longer valid, use the root
    // directory instead.
    uploader_params->upload_location = GURL(entry_proto->upload_url());
  } else {
    uploader_params->upload_location = GURL();
  }

  StartUpload(download_id, uploader_params.Pass());
}

void DriveDownloadObserver::StartUpload(
    int32 download_id,
    scoped_ptr<UploaderParams> uploader_params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(uploader_params.get());

  // Look up the DownloadItem for the |download_id|.
  DownloadMap::iterator iter = pending_downloads_.find(download_id);
  if (iter == pending_downloads_.end()) {
    DVLOG(1) << "Pending download not found" << download_id;
    return;
  }
  DVLOG(1) << "Starting upload for download ID " << download_id;
  DownloadItem* download_item = iter->second;

  UploadingUserData* upload_data = GetUploadingUserData(download_item);
  DCHECK(upload_data);
  upload_data->set_virtual_dir_path(uploader_params->drive_path.DirName());

  // Start upload and save the upload id for future reference.
  if (upload_data->is_overwrite()) {
    const int upload_id =
        drive_uploader_->StreamExistingFile(
            uploader_params->upload_location,
            uploader_params->drive_path,
            uploader_params->file_path,
            uploader_params->content_type,
            uploader_params->content_length,
            uploader_params->file_size,
            uploader_params->completion_callback);
    upload_data->set_upload_id(upload_id);
  } else {
    const int upload_id =
        drive_uploader_->UploadNewFile(uploader_params->upload_location,
                                       uploader_params->drive_path,
                                       uploader_params->file_path,
                                       uploader_params->title,
                                       uploader_params->content_type,
                                       uploader_params->content_length,
                                       uploader_params->file_size,
                                       uploader_params->completion_callback);
    upload_data->set_upload_id(upload_id);
  }
}

void DriveDownloadObserver::OnUploadComplete(
    int32 download_id,
    DriveFileError error,
    const FilePath& drive_path,
    const FilePath& file_path,
    scoped_ptr<DocumentEntry> document_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(document_entry.get());

  // Look up the DownloadItem for the |download_id|.
  DownloadMap::iterator iter = pending_downloads_.find(download_id);
  if (iter == pending_downloads_.end()) {
    DVLOG(1) << "Pending download not found" << download_id;
    return;
  }
  DVLOG(1) << "Completing upload for download ID " << download_id;
  DownloadItem* download_item = iter->second;

  UploadingUserData* upload_data = GetUploadingUserData(download_item);
  DCHECK(upload_data);

  // Take ownership of the DocumentEntry from UploadFileInfo. This is used by
  // DriveFileSystem::AddUploadedFile() to add the entry to DriveCache after the
  // upload completes.
  upload_data->set_entry(document_entry.Pass());

  // Allow the download item to complete.
  upload_data->CompleteDownload();
}

void DriveDownloadObserver::MoveFileToDriveCache(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  UploadingUserData* upload_data = GetUploadingUserData(download);
  if (!upload_data) {
    NOTREACHED();
    return;
  }

  // Pass ownership of the DocumentEntry object.
  scoped_ptr<DocumentEntry> entry = upload_data->entry_passed();
  if (!entry.get()) {
    NOTREACHED();
    return;
  }

  if (upload_data->is_overwrite()) {
    file_system_->UpdateEntryData(upload_data->resource_id(),
                                  upload_data->md5(),
                                  entry.Pass(),
                                  download->GetTargetFilePath(),
                                  base::Bind(&base::DoNothing));
  } else {
    // Move downloaded file to drive cache. Note that |content_file_path| should
    // use the final target path (download->GetTargetFilePath()) when the
    // download item has transitioned to the DownloadItem::COMPLETE state.
    file_system_->AddUploadedFile(UPLOAD_NEW_FILE,
                                  upload_data->virtual_dir_path(),
                                  entry.Pass(),
                                  download->GetTargetFilePath(),
                                  DriveCache::FILE_OPERATION_MOVE,
                                  base::Bind(&base::DoNothing));
  }
}

DriveDownloadObserver::UploaderParams::UploaderParams()
    : file_size(0),
      content_length(-1),
      all_bytes_present(false) {
}

DriveDownloadObserver::UploaderParams::~UploaderParams() {
}

// Useful for printf debugging.
std::string DriveDownloadObserver::UploaderParams::DebugString() const {
  return "title=[" + title +
         "], file_path=[" + file_path.value() +
         "], content_type=[" + content_type +
         "], content_length=[" + base::UintToString(content_length) +
         "], upload_location=[" + upload_location.possibly_invalid_spec() +
         "], drive_path=[" + drive_path.value() +
         "], all_bytes_present=[" + (all_bytes_present ?  "true" : "false") +
         "]";
}

}  // namespace gdata
