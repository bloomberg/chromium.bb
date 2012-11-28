// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_download_observer.h"

#include <string>

#include "base/callback.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/download/download_completion_blocker.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "net/base/net_util.h"

using content::BrowserThread;
using content::DownloadManager;
using content::DownloadItem;

namespace drive {
namespace {

// Threshold file size after which we stream the file.
const int64 kStreamingFileSize = 1 << 20;  // 1MB

// Keys for base::SupportsUserData::Data.
const char kUploadingKey[] = "Uploading";
const char kGDataPathKey[] = "GDataPath";

void ResetDownloadUserData(DownloadItem* download) {
  download->SetUserData(&kUploadingKey, NULL);
  download->SetUserData(&kGDataPathKey, NULL);
}

DownloadItem* GetDownload(
    AllDownloadItemNotifier* notifier,
    int32 download_id) {
  return ((notifier && notifier->GetManager()) ?
      notifier->GetManager()->GetDownload(download_id) : NULL);
}

// User Data stored in DownloadItem for ongoing uploads.
class UploadingUserData : public DownloadCompletionBlocker {
 public:
  explicit UploadingUserData(google_apis::DriveUploader* uploader)
      : uploader_(uploader),
        upload_id_(-1),
        is_overwrite_(false) {
  }
  virtual ~UploadingUserData() {}

  google_apis::DriveUploader* uploader() const { return uploader_; }
  void set_upload_id(int upload_id) { upload_id_ = upload_id; }
  int upload_id() const { return upload_id_; }
  void set_virtual_dir_path(const FilePath& path) { virtual_dir_path_ = path; }
  const FilePath& virtual_dir_path() const { return virtual_dir_path_; }
  void set_entry(scoped_ptr<google_apis::DocumentEntry> entry) {
    entry_ = entry.Pass();
  }
  scoped_ptr<google_apis::DocumentEntry> entry_passed() {
    return entry_.Pass();
  }
  void set_overwrite(bool overwrite) { is_overwrite_ = overwrite; }
  bool is_overwrite() const { return is_overwrite_; }
  void set_resource_id(const std::string& resource_id) {
    resource_id_ = resource_id;
  }
  const std::string& resource_id() const { return resource_id_; }
  void set_md5(const std::string& md5) { md5_ = md5; }
  const std::string& md5() const { return md5_; }

 private:
  google_apis::DriveUploader* uploader_;
  int upload_id_;
  FilePath virtual_dir_path_;
  scoped_ptr<google_apis::DocumentEntry> entry_;
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
  return static_cast<UploadingUserData*>(download->GetUserData(&kUploadingKey));
}

const UploadingUserData* GetUploadingUserData(const DownloadItem* download) {
  return static_cast<const UploadingUserData*>(
      download->GetUserData(&kUploadingKey));
}

// Extracts DriveUserData* from |download|.
DriveUserData* GetDriveUserData(DownloadItem* download) {
  return static_cast<DriveUserData*>(download->GetUserData(&kGDataPathKey));
}

const DriveUserData* GetDriveUserData(const DownloadItem* download) {
  return static_cast<const DriveUserData*>(
      download->GetUserData(&kGDataPathKey));
}

DriveSystemService* GetSystemService(Profile* profile) {
  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(
        profile ? profile : ProfileManager::GetDefaultProfile());
  DCHECK(system_service);
  return system_service;
}

// Creates a temporary file |drive_tmp_download_path| in
// |drive_tmp_download_dir|. Must be called on a thread that allows file
// operations.
FilePath GetDriveTempDownloadPath(const FilePath& drive_tmp_download_dir) {
  bool created = file_util::CreateDirectory(drive_tmp_download_dir);
  DCHECK(created) << "Can not create temp download directory at "
                  << drive_tmp_download_dir.value();
  FilePath drive_tmp_download_path;
  created = file_util::CreateTemporaryFileInDir(drive_tmp_download_dir,
                                                &drive_tmp_download_path);
  DCHECK(created) << "Temporary download file creation failed";
  return drive_tmp_download_path;
}

// Substitutes virtual drive path for local temporary path.
void SubstituteDriveDownloadPathInternal(
    Profile* profile,
    const DriveDownloadObserver::SubstituteDriveDownloadPathCallback&
        callback) {
  DVLOG(1) << "SubstituteDriveDownloadPathInternal";

  const FilePath drive_tmp_download_dir = GetSystemService(profile)->cache()->
      GetCacheDirectoryPath(DriveCache::CACHE_TYPE_TMP_DOWNLOADS);

  // Swap the drive path with a local path. Local path must be created
  // on a blocking thread.
  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&GetDriveTempDownloadPath, drive_tmp_download_dir),
      callback);
}

// Callback for DriveFileSystem::CreateDirectory.
void OnCreateDirectory(
    Profile* profile,
    const DriveDownloadObserver::SubstituteDriveDownloadPathCallback& callback,
    DriveFileError error) {
  DVLOG(1) << "OnCreateDirectory " << error;
  if (error == DRIVE_FILE_OK) {
    SubstituteDriveDownloadPathInternal(profile, callback);
  } else {
    // TODO(achuith): Handle this.
    NOTREACHED();
  }
}

// Callback for DriveFileSystem::GetEntryInfoByPath.
void OnEntryFound(
    Profile* profile,
    const FilePath& drive_dir_path,
    const DriveDownloadObserver::SubstituteDriveDownloadPathCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  if (error == DRIVE_FILE_ERROR_NOT_FOUND) {
    // Destination Drive directory doesn't exist, so create it.
    const bool is_exclusive = false, is_recursive = true;
    GetSystemService(profile)->file_system()->CreateDirectory(
        drive_dir_path, is_exclusive, is_recursive,
        base::Bind(&OnCreateDirectory, profile, callback));
  } else if (error == DRIVE_FILE_OK) {
    SubstituteDriveDownloadPathInternal(profile, callback);
  } else {
    // TODO(achuith): Handle this.
    NOTREACHED();
  }
}

// Callback for DriveFileSystem::UpdateEntryData.
void OnEntryUpdated(DriveFileError error) {
  LOG_IF(ERROR, error != DRIVE_FILE_OK) << "Failed to update entry: " << error;
}

}  // namespace

struct DriveDownloadObserver::UploaderParams {
  UploaderParams() : file_size(0),
                     content_length(-1) {}
  ~UploaderParams() {}

  FilePath file_path; // The path of the file to be uploaded.
  int64 file_size; // The last known size of the file.

  // TODO(zelirag, achuith): Make this string16.
  std::string title; // Title to be used for file to be uploaded.
  std::string content_type; // Content-Type of file.
  int64 content_length; // Header Content-Length.
  GURL upload_location; // Initial upload location for the file.

  // Final path in gdata. Looks like /special/drive/MyFolder/MyFile.
  FilePath drive_path;

  // Callback to be invoked once the uploader is ready to upload.
  google_apis::UploaderReadyCallback ready_callback;

  // Callback to be invoked once the upload has completed.
  google_apis::UploadCompletionCallback completion_callback;
};

DriveDownloadObserver::DriveDownloadObserver(
    google_apis::DriveUploader* uploader,
    DriveFileSystemInterface* file_system)
    : drive_uploader_(uploader),
      file_system_(file_system),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

DriveDownloadObserver::~DriveDownloadObserver() {
}

void DriveDownloadObserver::Initialize(
    DownloadManager* download_manager,
    const FilePath& drive_tmp_download_path) {
  DCHECK(!drive_tmp_download_path.empty());
  if (download_manager)
    notifier_.reset(new AllDownloadItemNotifier(download_manager, this));
  drive_tmp_download_path_ = drive_tmp_download_path;
}

// static
void DriveDownloadObserver::SubstituteDriveDownloadPath(Profile* profile,
    const FilePath& drive_path, content::DownloadItem* download,
    const SubstituteDriveDownloadPathCallback& callback) {
  DVLOG(1) << "SubstituteDriveDownloadPath " << drive_path.value();

  SetDownloadParams(drive_path, download);

  if (util::IsUnderDriveMountPoint(drive_path)) {
    // Can't access drive if the directory does not exist on Drive.
    // We set off a chain of callbacks as follows:
    // DriveFileSystem::GetEntryInfoByPath
    //   OnEntryFound calls DriveFileSystem::CreateDirectory (if necessary)
    //     OnCreateDirectory calls SubstituteDriveDownloadPathInternal
    const FilePath drive_dir_path =
        util::ExtractDrivePath(drive_path.DirName());
    // Ensure the directory exists. This also forces DriveFileSystem to
    // initialize DriveRootDirectory.
    GetSystemService(profile)->file_system()->GetEntryInfoByPath(
        drive_dir_path,
        base::Bind(&OnEntryFound, profile, drive_dir_path, callback));
  } else {
    callback.Run(drive_path);
  }
}

// static
void DriveDownloadObserver::SetDownloadParams(const FilePath& drive_path,
                                              DownloadItem* download) {
  if (!download || (download->GetState() != DownloadItem::IN_PROGRESS))
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
FilePath DriveDownloadObserver::GetDrivePath(const DownloadItem* download) {
  const DriveUserData* data = GetDriveUserData(download);
  // If data is NULL, we've somehow lost the drive path selected by the file
  // picker.
  DCHECK(data);
  return data ? util::ExtractDrivePath(data->file_path()) : FilePath();
}

// static
bool DriveDownloadObserver::IsDriveDownload(const DownloadItem* download) {
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
int64 DriveDownloadObserver::GetUploadedBytes(const DownloadItem* download) {
  const UploadingUserData* upload_data = GetUploadingUserData(download);
  if (!upload_data || !upload_data->uploader())
    return 0;
  return upload_data->uploader()->GetUploadedBytes(upload_data->upload_id());
}

// static
int DriveDownloadObserver::PercentComplete(const DownloadItem* download) {
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

void DriveDownloadObserver::OnDownloadUpdated(
    DownloadManager* manager, DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Drive downloads are considered temporary downloads. Only accept downloads
  // that have the Drive meta data associated with them. Otherwise we might trip
  // over non-Drive downloads being saved to drive_tmp_download_path_.
  if (!download->IsTemporary() ||
      (download->GetTargetFilePath().DirName() != drive_tmp_download_path_) ||
      !IsDriveDownload(download))
    return;

  const DownloadItem::DownloadState state = download->GetState();
  switch (state) {
    case DownloadItem::IN_PROGRESS:
      UploadDownloadItem(download);
      break;

    case DownloadItem::COMPLETE:
      UploadDownloadItem(download);
      MoveFileToDriveCache(download);
      ResetDownloadUserData(download);
      break;

    // TODO(achuith): Stop the pending upload and delete the file.
    case DownloadItem::CANCELLED:
    case DownloadItem::INTERRUPTED:
      ResetDownloadUserData(download);
      break;

    default:
      NOTREACHED();
  }
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
  if (!upload_data || upload_data->upload_id() == -1) {
    DVLOG(1) << "No UploadingUserData for download: " << download->GetId()
             << " or Uploader is not ready yet.";
    return;
  }

  drive_uploader_->UpdateUpload(upload_data->upload_id(), download);
}

bool DriveDownloadObserver::ShouldUpload(DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Upload if the item is either in progress or complete,
  // has a filename,
  // is complete or large enough to stream, and,
  // is not already being uploaded.
  return ((download->GetState() == DownloadItem::IN_PROGRESS) ||
          (download->GetState() == DownloadItem::COMPLETE)) &&
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

  uploader_params->completion_callback =
      base::Bind(&DriveDownloadObserver::OnUploadComplete,
                 weak_ptr_factory_.GetWeakPtr(),
                 download->GetId());

  uploader_params->ready_callback =
      base::Bind(&DriveDownloadObserver::OnUploaderReady,
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

    DownloadItem* download_item = GetDownload(notifier_.get(), download_id);
    if (!download_item) {
      DVLOG(1) << "Pending download not found" << download_id;
      return;
    }
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

  DownloadItem* download_item = GetDownload(notifier_.get(), download_id);
  if (!download_item) {
    DVLOG(1) << "Pending download not found" << download_id;
    return;
  }
  DVLOG(1) << "Starting upload for download ID " << download_id;

  UploadingUserData* upload_data = GetUploadingUserData(download_item);
  DCHECK(upload_data);
  upload_data->set_virtual_dir_path(uploader_params->drive_path.DirName());

  // Start upload and save the upload id for future reference.
  if (upload_data->is_overwrite()) {
    drive_uploader_->StreamExistingFile(
        uploader_params->upload_location,
        uploader_params->drive_path,
        uploader_params->file_path,
        uploader_params->content_type,
        uploader_params->content_length,
        uploader_params->file_size,
        uploader_params->completion_callback,
        uploader_params->ready_callback);
  } else {
    drive_uploader_->UploadNewFile(uploader_params->upload_location,
                                   uploader_params->drive_path,
                                   uploader_params->file_path,
                                   uploader_params->title,
                                   uploader_params->content_type,
                                   uploader_params->content_length,
                                   uploader_params->file_size,
                                   uploader_params->completion_callback,
                                   uploader_params->ready_callback);
  }
}

void DriveDownloadObserver::OnUploaderReady(int32 download_id,
                                            int32 upload_id) {
  DownloadItem* download_item = GetDownload(notifier_.get(), download_id);
  if (!download_item) {
    DVLOG(1) << "Pending download not found" << download_id;
    return;
  }
  DVLOG(1) << "Uploader for download id: " << download_id
           << "ready with the upload id: " << upload_id;

  UploadingUserData* upload_data = GetUploadingUserData(download_item);
  DCHECK(upload_data);

  // Let's start uploading just when the uploader gets ready.
  drive_uploader_->UpdateUpload(upload_id, download_item);

  // Store upload id in the uploading data structure.
  upload_data->set_upload_id(upload_id);
}

void DriveDownloadObserver::OnUploadComplete(
    int32 download_id,
    google_apis::DriveUploadError error,
    const FilePath& drive_path,
    const FilePath& file_path,
    scoped_ptr<google_apis::DocumentEntry> document_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(document_entry.get());

  DownloadItem* download_item = GetDownload(notifier_.get(), download_id);
  if (!download_item) {
    DVLOG(1) << "Pending download not found" << download_id;
    return;
  }
  DVLOG(1) << "Completing upload for download ID " << download_id;

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

  // Pass ownership of the google_apis::DocumentEntry object.
  scoped_ptr<google_apis::DocumentEntry> entry = upload_data->entry_passed();
  if (!entry.get()) {
    NOTREACHED();
    return;
  }

  if (upload_data->is_overwrite()) {
    file_system_->UpdateEntryData(entry.Pass(),
                                  download->GetTargetFilePath(),
                                  base::Bind(&OnEntryUpdated));
  } else {
    // Move downloaded file to drive cache. Note that |content_file_path| should
    // use the final target path (download->GetTargetFilePath()) when the
    // download item has transitioned to the DownloadItem::COMPLETE state.
    file_system_->AddUploadedFile(upload_data->virtual_dir_path(),
                                  entry.Pass(),
                                  download->GetTargetFilePath(),
                                  DriveCache::FILE_OPERATION_MOVE,
                                  base::Bind(&OnEntryUpdated));
  }
}

}  // namespace drive
