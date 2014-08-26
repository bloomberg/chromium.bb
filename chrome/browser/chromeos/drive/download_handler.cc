// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/download_handler.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/supports_user_data.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/write_on_cache_file.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
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
  explicit DriveUserData(const base::FilePath& path) : file_path_(path),
                                                       is_complete_(false) {}
  virtual ~DriveUserData() {}

  const base::FilePath& file_path() const { return file_path_; }
  const base::FilePath& cache_file_path() const { return cache_file_path_; }
  void set_cache_file_path(const base::FilePath& path) {
    cache_file_path_ = path;
  }
  bool is_complete() const { return is_complete_; }
  void set_complete() { is_complete_ = true; }

 private:
  const base::FilePath file_path_;
  base::FilePath cache_file_path_;
  bool is_complete_;
};

// Extracts DriveUserData* from |download|.
const DriveUserData* GetDriveUserData(const DownloadItem* download) {
  return static_cast<const DriveUserData*>(
      download->GetUserData(&kDrivePathKey));
}

DriveUserData* GetDriveUserData(DownloadItem* download) {
  return static_cast<DriveUserData*>(download->GetUserData(&kDrivePathKey));
}

// Creates a temporary file |drive_tmp_download_path| in
// |drive_tmp_download_dir|. Must be called on a thread that allows file
// operations.
base::FilePath GetDriveTempDownloadPath(
    const base::FilePath& drive_tmp_download_dir) {
  bool created = base::CreateDirectory(drive_tmp_download_dir);
  DCHECK(created) << "Can not create temp download directory at "
                  << drive_tmp_download_dir.value();
  base::FilePath drive_tmp_download_path;
  created = base::CreateTemporaryFileInDir(drive_tmp_download_dir,
                                           &drive_tmp_download_path);
  DCHECK(created) << "Temporary download file creation failed";
  return drive_tmp_download_path;
}

// Moves downloaded file to Drive.
void MoveDownloadedFile(const base::FilePath& downloaded_file,
                        base::FilePath* cache_file_path,
                        FileError error,
                        const base::FilePath& dest_path) {
  if (error != FILE_ERROR_OK ||
      !base::Move(downloaded_file, dest_path))
    return;
  *cache_file_path = dest_path;
}

// Used to implement CheckForFileExistence().
void ContinueCheckingForFileExistence(
    const content::CheckForFileExistenceCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry) {
  callback.Run(error == FILE_ERROR_OK);
}

// Returns true if |download| is a Drive download created from data persisted
// on the download history DB.
bool IsPersistedDriveDownload(const base::FilePath& drive_tmp_download_path,
                              DownloadItem* download) {
  if (!drive_tmp_download_path.IsParent(download->GetTargetFilePath()))
    return false;

  DownloadService* download_service =
      DownloadServiceFactory::GetForBrowserContext(
          download->GetBrowserContext());
  DownloadHistory* download_history = download_service->GetDownloadHistory();

  return download_history && download_history->WasRestoredFromHistory(download);
}

}  // namespace

DownloadHandler::DownloadHandler(FileSystemInterface* file_system)
    : file_system_(file_system),
      weak_ptr_factory_(this) {
}

DownloadHandler::~DownloadHandler() {
}

// static
DownloadHandler* DownloadHandler::GetForProfile(Profile* profile) {
  DriveIntegrationService* service =
      DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!service || !service->IsMounted())
    return NULL;
  return service->download_handler();
}

void DownloadHandler::Initialize(
    DownloadManager* download_manager,
    const base::FilePath& drive_tmp_download_path) {
  DCHECK(!drive_tmp_download_path.empty());

  drive_tmp_download_path_ = drive_tmp_download_path;

  if (download_manager) {
    notifier_.reset(new AllDownloadItemNotifier(download_manager, this));
    // Remove any persisted Drive DownloadItem. crbug.com/171384
    content::DownloadManager::DownloadVector downloads;
    download_manager->GetAllDownloads(&downloads);
    for (size_t i = 0; i < downloads.size(); ++i) {
      if (IsPersistedDriveDownload(drive_tmp_download_path_, downloads[i]))
        downloads[i]->Remove();
    }
  }
}

void DownloadHandler::ObserveIncognitoDownloadManager(
    DownloadManager* download_manager) {
  notifier_incognito_.reset(new AllDownloadItemNotifier(download_manager,
                                                        this));
}

void DownloadHandler::SubstituteDriveDownloadPath(
    const base::FilePath& drive_path,
    content::DownloadItem* download,
    const SubstituteDriveDownloadPathCallback& callback) {
  DVLOG(1) << "SubstituteDriveDownloadPath " << drive_path.value();

  SetDownloadParams(drive_path, download);

  if (util::IsUnderDriveMountPoint(drive_path)) {
    // Prepare the destination directory.
    const bool is_exclusive = false, is_recursive = true;
    file_system_->CreateDirectory(
        util::ExtractDrivePath(drive_path.DirName()),
        is_exclusive, is_recursive,
        base::Bind(&DownloadHandler::OnCreateDirectory,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback));
  } else {
    callback.Run(drive_path);
  }
}

void DownloadHandler::SetDownloadParams(const base::FilePath& drive_path,
                                        DownloadItem* download) {
  if (!download || (download->GetState() != DownloadItem::IN_PROGRESS))
    return;

  if (util::IsUnderDriveMountPoint(drive_path)) {
    download->SetUserData(&kDrivePathKey, new DriveUserData(drive_path));
    download->SetDisplayName(drive_path.BaseName());
  } else if (IsDriveDownload(download)) {
    // This may have been previously set if the default download folder is
    // /drive, and the user has now changed the download target to a local
    // folder.
    download->SetUserData(&kDrivePathKey, NULL);
    download->SetDisplayName(base::FilePath());
  }
}

base::FilePath DownloadHandler::GetTargetPath(
    const DownloadItem* download) {
  const DriveUserData* data = GetDriveUserData(download);
  // If data is NULL, we've somehow lost the drive path selected by the file
  // picker.
  DCHECK(data);
  return data ? data->file_path() : base::FilePath();
}

base::FilePath DownloadHandler::GetCacheFilePath(const DownloadItem* download) {
  const DriveUserData* data = GetDriveUserData(download);
  return data ? data->cache_file_path() : base::FilePath();
}

bool DownloadHandler::IsDriveDownload(const DownloadItem* download) {
  // We use the existence of the DriveUserData object in download as a
  // signal that this is a download to Drive.
  return GetDriveUserData(download) != NULL;
}

void DownloadHandler::CheckForFileExistence(
    const DownloadItem* download,
    const content::CheckForFileExistenceCallback& callback) {
  file_system_->GetResourceEntry(
      util::ExtractDrivePath(GetTargetPath(download)),
      base::Bind(&ContinueCheckingForFileExistence,
                 callback));
}

void DownloadHandler::OnDownloadCreated(DownloadManager* manager,
                                        DownloadItem* download) {
  // Remove any persisted Drive DownloadItem. crbug.com/171384
  if (IsPersistedDriveDownload(drive_tmp_download_path_, download)) {
    // Remove download later, since doing it here results in a crash.
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&DownloadHandler::RemoveDownload,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       static_cast<void*>(manager),
                                       download->GetId()));
  }
}

void DownloadHandler::RemoveDownload(void* manager_id, int id) {
  DownloadManager* manager = GetDownloadManager(manager_id);
  if (!manager)
    return;
  DownloadItem* download = manager->GetDownload(id);
  if (!download)
    return;
  download->Remove();
}

void DownloadHandler::OnDownloadUpdated(
    DownloadManager* manager, DownloadItem* download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Only accept downloads that have the Drive meta data associated with them.
  DriveUserData* data = GetDriveUserData(download);
  if (!drive_tmp_download_path_.IsParent(download->GetTargetFilePath()) ||
      !data ||
      data->is_complete())
    return;

  switch (download->GetState()) {
    case DownloadItem::IN_PROGRESS:
      break;

    case DownloadItem::COMPLETE:
      UploadDownloadItem(manager, download);
      data->set_complete();
      break;

    case DownloadItem::CANCELLED:
      download->SetUserData(&kDrivePathKey, NULL);
      break;

    case DownloadItem::INTERRUPTED:
      // Interrupted downloads can be resumed. Keep the Drive user data around
      // so that it can be used when the download resumes. The download is truly
      // done when it's complete, is cancelled or is removed.
      break;

    default:
      NOTREACHED();
  }
}

void DownloadHandler::OnCreateDirectory(
    const SubstituteDriveDownloadPathCallback& callback,
    FileError error) {
  DVLOG(1) << "OnCreateDirectory " << FileErrorToString(error);
  if (error == FILE_ERROR_OK) {
    base::PostTaskAndReplyWithResult(
        BrowserThread::GetBlockingPool(),
        FROM_HERE,
        base::Bind(&GetDriveTempDownloadPath, drive_tmp_download_path_),
        callback);
  } else {
    LOG(WARNING) << "Failed to create directory, error = "
                 << FileErrorToString(error);
    callback.Run(base::FilePath());
  }
}

void DownloadHandler::UploadDownloadItem(DownloadManager* manager,
                                         DownloadItem* download) {
  DCHECK_EQ(DownloadItem::COMPLETE, download->GetState());
  base::FilePath* cache_file_path = new base::FilePath;
  WriteOnCacheFileAndReply(
      file_system_,
      util::ExtractDrivePath(GetTargetPath(download)),
      download->GetMimeType(),
      base::Bind(&MoveDownloadedFile, download->GetTargetFilePath(),
                 cache_file_path),
      base::Bind(&DownloadHandler::SetCacheFilePath,
                 weak_ptr_factory_.GetWeakPtr(),
                 static_cast<void*>(manager),
                 download->GetId(),
                 base::Owned(cache_file_path)));
}

void DownloadHandler::SetCacheFilePath(void* manager_id,
                                       int id,
                                       const base::FilePath* cache_file_path,
                                       FileError error) {
  if (error != FILE_ERROR_OK)
    return;
  DownloadManager* manager = GetDownloadManager(manager_id);
  if (!manager)
    return;
  DownloadItem* download = manager->GetDownload(id);
  if (!download)
    return;
  DriveUserData* data = GetDriveUserData(download);
  if (!data)
    return;
  data->set_cache_file_path(*cache_file_path);
}

DownloadManager* DownloadHandler::GetDownloadManager(void* manager_id) {
  if (manager_id == notifier_->GetManager())
    return notifier_->GetManager();
  if (notifier_incognito_ && manager_id == notifier_incognito_->GetManager())
    return notifier_incognito_->GetManager();
  return NULL;
}

}  // namespace drive
