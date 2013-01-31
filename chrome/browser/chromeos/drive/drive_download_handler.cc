// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_download_handler.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/supports_user_data.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/chromeos/drive/file_write_helper.h"
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

// Moves downloaded file to Drive.
void MoveDownloadedFile(const FilePath& downloaded_file,
                        DriveFileError error,
                        const FilePath& dest_path) {
  if (error != DRIVE_FILE_OK)
    return;
  file_util::Move(downloaded_file, dest_path);
}

}  // namespace

DriveDownloadHandler::DriveDownloadHandler(
    FileWriteHelper* file_write_helper,
    DriveFileSystemInterface* file_system)
    : file_write_helper_(file_write_helper),
      file_system_(file_system),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

DriveDownloadHandler::~DriveDownloadHandler() {
}

// static
DriveDownloadHandler* DriveDownloadHandler::GetForProfile(Profile* profile) {
  DriveSystemService* system_service =
      DriveSystemServiceFactory::FindForProfile(profile);
  return system_service ? system_service->download_handler() : NULL;
}

void DriveDownloadHandler::Initialize(
    DownloadManager* download_manager,
    const FilePath& drive_tmp_download_path) {
  DCHECK(!drive_tmp_download_path.empty());
  if (download_manager)
    notifier_.reset(new AllDownloadItemNotifier(download_manager, this));
  drive_tmp_download_path_ = drive_tmp_download_path;
}

void DriveDownloadHandler::SubstituteDriveDownloadPath(
    const FilePath& drive_path,
    content::DownloadItem* download,
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
    file_system_->GetEntryInfoByPath(
        drive_dir_path,
        base::Bind(&DriveDownloadHandler::OnEntryFound,
                   weak_ptr_factory_.GetWeakPtr(),
                   drive_dir_path,
                   callback));
  } else {
    callback.Run(drive_path);
  }
}

void DriveDownloadHandler::SetDownloadParams(const FilePath& drive_path,
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

FilePath DriveDownloadHandler::GetTargetPath(const DownloadItem* download) {
  const DriveUserData* data = GetDriveUserData(download);
  // If data is NULL, we've somehow lost the drive path selected by the file
  // picker.
  DCHECK(data);
  return data ? data->file_path() : FilePath();
}

bool DriveDownloadHandler::IsDriveDownload(const DownloadItem* download) {
  // We use the existence of the DriveUserData object in download as a
  // signal that this is a DriveDownload.
  return GetDriveUserData(download) != NULL;
}

void DriveDownloadHandler::OnDownloadUpdated(
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

void DriveDownloadHandler::OnEntryFound(
    const FilePath& drive_dir_path,
    const SubstituteDriveDownloadPathCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  if (error == DRIVE_FILE_ERROR_NOT_FOUND) {
    // Destination Drive directory doesn't exist, so create it.
    const bool is_exclusive = false, is_recursive = true;
    file_system_->CreateDirectory(
        drive_dir_path, is_exclusive, is_recursive,
        base::Bind(&DriveDownloadHandler::OnCreateDirectory,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  } else if (error == DRIVE_FILE_OK) {
    // Directory is already ready.
    OnCreateDirectory(callback, DRIVE_FILE_OK);
  } else {
    LOG(WARNING) << "Failed to get entry info for path: "
                 << drive_dir_path.value() << ", error = " << error;
    callback.Run(FilePath());
  }
}

void DriveDownloadHandler::OnCreateDirectory(
    const SubstituteDriveDownloadPathCallback& callback,
    DriveFileError error) {
  DVLOG(1) << "OnCreateDirectory " << error;
  if (error == DRIVE_FILE_OK) {
    base::PostTaskAndReplyWithResult(
        BrowserThread::GetBlockingPool(),
        FROM_HERE,
        base::Bind(&GetDriveTempDownloadPath, drive_tmp_download_path_),
        callback);
  } else {
    LOG(WARNING) << "Failed to create directory, error = " << error;
    callback.Run(FilePath());
  }
}

void DriveDownloadHandler::UploadDownloadItem(DownloadItem* download) {
  DCHECK(download->IsComplete());
  file_write_helper_->PrepareWritableFileAndRun(
      util::ExtractDrivePath(GetTargetPath(download)),
      base::Bind(&MoveDownloadedFile, download->GetTargetFilePath()));
}

}  // namespace drive
