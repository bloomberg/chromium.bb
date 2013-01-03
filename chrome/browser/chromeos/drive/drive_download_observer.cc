// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_download_observer.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/supports_user_data.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/chromeos/drive/file_write_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using content::DownloadManager;
using content::DownloadItem;

namespace drive {
namespace {

// Key for base::SupportsUserData::Data.
const char kDrivePathKey[] = "DrivePath";

// User Data stored in DownloadItem for drive path.
class DriveUserData : public base::SupportsUserData::Data {
 public:
  explicit DriveUserData(const FilePath& path) : file_path_(path) {}
  virtual ~DriveUserData() {}

  const FilePath& file_path() const { return file_path_; }

 private:
  const FilePath file_path_;
};

// Extracts DriveUserData* from |download|.
const DriveUserData* GetDriveUserData(const DownloadItem* download) {
  return static_cast<const DriveUserData*>(
      download->GetUserData(&kDrivePathKey));
}

DriveSystemService* GetSystemService(Profile* profile) {
  DCHECK(profile);
  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(profile);
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
  DCHECK(profile);
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
  DCHECK(profile);
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
  DCHECK(profile);
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

// Moves downloaded file to Drive.
void MoveDownloadedFile(const FilePath& downloaded_file,
                        DriveFileError error,
                        const FilePath& dest_path) {
  if (error != DRIVE_FILE_OK)
    return;
  file_util::Move(downloaded_file, dest_path);
}

}  // namespace

DriveDownloadObserver::DriveDownloadObserver(
    FileWriteHelper* file_write_helper,
    DriveFileSystemInterface* file_system)
    : file_write_helper_(file_write_helper),
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
  DCHECK(profile);
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
    download->SetUserData(&kDrivePathKey, new DriveUserData(drive_path));
    download->SetDisplayName(drive_path.BaseName());
    download->SetIsTemporary(true);
  } else if (IsDriveDownload(download)) {
    // This may have been previously set if the default download folder is
    // /drive, and the user has now changed the download target to a local
    // folder.
    download->SetUserData(&kDrivePathKey, NULL);
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

  switch (download->GetState()) {
    case DownloadItem::IN_PROGRESS:
      break;

    case DownloadItem::COMPLETE:
      UploadDownloadItem(download);
      // Reset user data here to filter out subsequent updates from this item.
      download->SetUserData(&kDrivePathKey, NULL);
      break;

    case DownloadItem::CANCELLED:
    case DownloadItem::INTERRUPTED:
      download->SetUserData(&kDrivePathKey, NULL);
      break;

    default:
      NOTREACHED();
  }
}

void DriveDownloadObserver::UploadDownloadItem(DownloadItem* download) {
  file_write_helper_->PrepareWritableFileAndRun(
      GetDrivePath(download),
      base::Bind(&MoveDownloadedFile, download->GetFullPath()));
}

}  // namespace drive
