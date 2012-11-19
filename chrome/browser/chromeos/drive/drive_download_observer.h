// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_DOWNLOAD_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_DOWNLOAD_OBSERVER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/drive_file_error.h"
#include "chrome/browser/download/all_download_item_notifier.h"

class Profile;

namespace content {
class DownloadItem;
class DownloadManager;
}

namespace google_apis {
class DocumentEntry;
class DriveUploader;
}

namespace drive {

class DriveEntryProto;
class DriveFileSystemInterface;

// Observes downloads to temporary local drive folder. Schedules these
// downloads for upload to drive service.
class DriveDownloadObserver : public AllDownloadItemNotifier::Observer {
 public:
  DriveDownloadObserver(google_apis::DriveUploader* uploader,
                        DriveFileSystemInterface* file_system);
  virtual ~DriveDownloadObserver();

  // Become an observer of DownloadManager.
  void Initialize(content::DownloadManager* download_manager,
                  const FilePath& drive_tmp_download_path);

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

  // Checks if |download| is ready to complete. Returns true if |download| has
  // no Drive upload associated with it or if the Drive upload has already
  // completed. This method is called by the ChromeDownloadManagerDelegate to
  // check if the download is ready to complete.  If the download is not yet
  // ready to complete and |complete_callback| is not null, then
  // |complete_callback| will be called on the UI thread when the download
  // becomes ready to complete.  If this method is called multiple times with
  // the download not ready to complete, only the last |complete_callback|
  // passed to this method for |download| will be called.
  static bool IsReadyToComplete(content::DownloadItem* download,
                                const base::Closure& complete_callback);

  // Returns the count of bytes confirmed as uploaded so far for |download|.
  static int64 GetUploadedBytes(const content::DownloadItem* download);

  // Returns the progress of the upload of |download| as a percentage. If the
  // progress is unknown, returns -1.
  static int PercentComplete(const content::DownloadItem* download);

 private:
  // Structure containing arguments required to process uploading.
  // For internal use, to avoid passing all of the parameters every time
  // separately.
  struct UploaderParams;

  // AllDownloadItemNotifier::Observer
  virtual void OnDownloadUpdated(content::DownloadManager* manager,
                                 content::DownloadItem* download) OVERRIDE;

  // Starts the upload of a downloaded/downloading file.
  void UploadDownloadItem(content::DownloadItem* download);

  // Updates metadata of ongoing upload if it exists.
  void UpdateUpload(content::DownloadItem* download);

  // Checks if this DownloadItem should be uploaded.
  bool ShouldUpload(content::DownloadItem* download);

  // Creates UploaderParams and initializes it using DownloadItem*.
  void CreateUploaderParams(content::DownloadItem* download);

  // Callback for checking if the file already exists.  If so, the file is
  // overwritten, and StartUpload() to actually start the upload.  If not, the
  // directory is queried to determine where to store the file.
  void CreateUploaderParamsAfterCheckExistence(
    int32 download_id,
    scoped_ptr<UploaderParams> upload_params,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto);

  // Callback for handling results of DriveFileSystem::GetEntryInfoByPath()
  // initiated by CreateUploadParamsAfterCheckExistence(). This callback
  // reads the directory entry to determine the upload path, then calls
  // StartUpload() to actually start the upload.
  void CreateUploaderParamsAfterCheckTargetDir(
      int32 download_id,
      scoped_ptr<UploaderParams> upload_params,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Starts the upload.
  void StartUpload(int32 download_id,
                   scoped_ptr<UploaderParams> upload_params);

  // Callback invoked by DriveUploader when all the asynchronous callbacks
  // are finished and the uploader is ready to start uploading.
  //
  // Used to save upload_id to the content::DownloadItem structure which
  // is required by the UpdateUpload() method.
  //
  // Note, that it may happen that the file will be downloaded to the local
  // storage before the Uploader gets ready. Therefore, all of the UpdateUpload
  // invocations will be ignored (since upload_id is not set yet).
  //
  // We have to update the upload from OnUploadReady() to be sure that the last
  // chunk is uploaded to GDrive.
  //
  // See: http://crbug.com/145831
  void OnUploaderReady(int32 download_id, int32 upload_id);

  // Callback invoked by DriveUploader when the upload associated with
  // |download_id| has completed. |error| indicated whether the
  // call was successful. This function takes ownership of DocumentEntry from
  // for use by MoveFileToDriveCache(). It also invokes the
  // download callback method to allow it to complete.
  void OnUploadComplete(int32 download_id,
                        google_apis::DriveUploadError error,
                        const FilePath& drive_path,
                        const FilePath& file_path,
                        scoped_ptr<google_apis::DocumentEntry> document_entry);

  // Moves the downloaded file to drive cache.
  // Must be called after DriveDownloadObserver receives COMPLETE notification.
  void MoveFileToDriveCache(content::DownloadItem* download);

  // Private data.
  // The uploader owned by DriveSystemService. Used to trigger file uploads.
  google_apis::DriveUploader* drive_uploader_;
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
