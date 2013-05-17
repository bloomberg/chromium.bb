// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UPDATE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UPDATE_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

namespace base {
class FilePath;
}  // namespace base

namespace drive {

class JobScheduler;
class ResourceEntry;

namespace internal {
class FileCache;
class ResourceMetadata;
}  // namespace internal

namespace file_system {

class OperationObserver;

// This class encapsulates the drive Update function.  It is responsible for
// sending the request to the drive API, then updating the local state and
// metadata to reflect the new state.
class UpdateOperation {
 public:
  UpdateOperation(OperationObserver* observer,
                  JobScheduler* scheduler,
                  internal::ResourceMetadata* metadata,
                  internal::FileCache* cache);
  ~UpdateOperation();

  // Updates a file by the given |resource_id| on the Drive server by
  // uploading an updated version. Used for uploading dirty files. The file
  // should already be present in the cache.
  //
  // |callback| must not be null.
  void UpdateFileByResourceId(const std::string& resource_id,
                              DriveClientContext context,
                              const FileOperationCallback& callback);

 private:
  void UpdateFileAfterGetEntryInfo(DriveClientContext context,
                                   const FileOperationCallback& callback,
                                   FileError error,
                                   const base::FilePath& drive_file_path,
                                   scoped_ptr<ResourceEntry> entry);

  void UpdateFileAfterGetFile(DriveClientContext context,
                              const FileOperationCallback& callback,
                              const base::FilePath& drive_file_path,
                              scoped_ptr<ResourceEntry> entry,
                              FileError error,
                              const base::FilePath& cache_file_path);

  void UpdateFileAfterUpload(
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);

  void UpdateFileAfterRefresh(const FileOperationCallback& callback,
                              FileError error,
                              const base::FilePath& drive_file_path,
                              scoped_ptr<ResourceEntry> entry);

  OperationObserver* observer_;
  JobScheduler* scheduler_;
  internal::ResourceMetadata* metadata_;
  internal::FileCache* cache_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UpdateOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(UpdateOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_UPDATE_OPERATION_H_
