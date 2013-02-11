// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UPDATE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UPDATE_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class GURL;

namespace base {
class FilePath;
}

namespace drive {

class DriveCache;
class DriveEntryProto;
class DriveScheduler;

namespace file_system {

class OperationObserver;

// This class encapsulates the drive Update function.  It is responsible for
// sending the request to the drive API, then updating the local state and
// metadata to reflect the new state.
class UpdateOperation {
 public:
  UpdateOperation(DriveCache* cache,
                  DriveResourceMetadata* metadata,
                  DriveScheduler* scheduler,
                  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
                  OperationObserver* observer);
  virtual ~UpdateOperation();

  // Updates a file by the given |resource_id| on the Drive server by
  // uploading an updated version. Used for uploading dirty files. The file
  // should already be present in the cache.
  //
  // TODO(satorux): As of now, the function only handles files with the dirty
  // bit committed. We should eliminate the restriction. crbug.com/134558.
  //
  // Can only be called from UI thread.  |callback| must not be null.
  virtual void UpdateFileByResourceId(
      const std::string& resource_id,
      DriveClientContext context,
      const FileOperationCallback& callback);

 private:
  // Part of UpdateFileByResourceId(). Called when
  // DriveResourceMetadata::GetEntryInfoByResourceId() is complete.
  // |callback| must not be null.
  void UpdateFileByEntryInfo(
      DriveClientContext context,
      const FileOperationCallback& callback,
      DriveFileError error,
      const base::FilePath& drive_file_path,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Part of UpdateFileByResourceId().
  // Called when DriveCache::GetFileOnUIThread() is completed for
  // UpdateFileByResourceId().
  // |callback| must not be null.
  void OnGetFileCompleteForUpdateFile(
      DriveClientContext context,
      const FileOperationCallback& callback,
      const base::FilePath& drive_file_path,
      scoped_ptr<DriveEntryProto> entry_proto,
      DriveFileError error,
      const base::FilePath& cache_file_path);

  // Part of UpdateFileByResourceId().
  // Called when DriveUploader::UploadUpdatedFile() is completed for
  // UpdateFileByResourceId().
  // |callback| must not be null.
  void OnUpdatedFileUploaded(
      const FileOperationCallback& callback,
      google_apis::DriveUploadError error,
      const base::FilePath& gdata_path,
      const base::FilePath& file_path,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);

  // Part of UpdateFileByResourceId().
  // |callback| must not be null.
  void OnUpdatedFileRefreshed(const FileOperationCallback& callback,
                              DriveFileError error,
                              const base::FilePath& drive_file_path,
                              scoped_ptr<DriveEntryProto> entry_proto);

  DriveCache* cache_;
  DriveResourceMetadata* metadata_;
  DriveScheduler* scheduler_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  OperationObserver* observer_;

  // WeakPtrFactory bound to the UI thread.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UpdateOperation> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UpdateOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UPDATE_OPERATION_H_
