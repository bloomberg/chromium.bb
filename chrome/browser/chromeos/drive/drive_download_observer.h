// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_DOWNLOAD_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_DOWNLOAD_OBSERVER_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/drive_file_error.h"
#include "chrome/browser/download/all_download_item_notifier.h"

class Profile;

namespace content {
class DownloadItem;
class DownloadManager;
}

namespace drive {

class DriveFileSystemInterface;
class FileWriteHelper;

// Observes downloads to temporary local drive folder. Schedules these
// downloads for upload to drive service.
class DriveDownloadObserver : public AllDownloadItemNotifier::Observer {
 public:
  DriveDownloadObserver(FileWriteHelper* file_write_helper,
                        DriveFileSystemInterface* file_system);
  virtual ~DriveDownloadObserver();

  // Become an observer of DownloadManager.
  void Initialize(content::DownloadManager* download_manager,
                  const FilePath& drive_tmp_download_path);

  // Callback used to return results from SubstituteDriveDownloadPath.
  // TODO(hashimoto): Report error with a DriveFileError. crbug.com/171345
  typedef base::Callback<void(const FilePath&)>
      SubstituteDriveDownloadPathCallback;

  static void SubstituteDriveDownloadPath(
      Profile* profile,
      const FilePath& drive_path,
      content::DownloadItem* download,
      const SubstituteDriveDownloadPathCallback& callback);

  // Sets drive path, for example, '/special/drive/MyFolder/MyFile',
  // to external data in |download|. Also sets display name and
  // makes |download| a temporary.
  static void SetDownloadParams(const FilePath& drive_path,
                                content::DownloadItem* download);

  // Gets the drive_path from external data in |download|.
  // GetDrivePath may return an empty path in case SetDrivePath was not
  // previously called or there was some other internal error
  // (there is a DCHECK for this).
  static FilePath GetDrivePath(const content::DownloadItem* download);

  // Checks if there is a Drive upload associated with |download|
  static bool IsDriveDownload(const content::DownloadItem* download);

 private:
  // AllDownloadItemNotifier::Observer
  virtual void OnDownloadUpdated(content::DownloadManager* manager,
                                 content::DownloadItem* download) OVERRIDE;

  // Starts the upload of a downloaded/downloading file.
  void UploadDownloadItem(content::DownloadItem* download);

  FileWriteHelper* file_write_helper_;
  // The file system owned by DriveSystemService.
  DriveFileSystemInterface* file_system_;
  // Observe the DownloadManager for new downloads.
  scoped_ptr<AllDownloadItemNotifier> notifier_;

  // Temporary download location directory.
  FilePath drive_tmp_download_path_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveDownloadObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveDownloadObserver);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_DOWNLOAD_OBSERVER_H_
