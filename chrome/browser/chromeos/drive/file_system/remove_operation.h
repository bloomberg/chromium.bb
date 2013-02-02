// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_REMOVE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_REMOVE_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class GURL;

namespace base {
class FilePath;
}

namespace google_apis {
}

namespace drive {

class DriveCache;
class DriveEntryProto;
class DriveFileSystem;
class DriveScheduler;

namespace file_system {

class OperationObserver;

// This class encapsulates the drive Remove function.  It is responsible for
// sending the request to the drive API, then updating the local state and
// metadata to reflect the new state.
class RemoveOperation {
 public:
  RemoveOperation(DriveScheduler* drive_scheduler,
                  DriveCache* cache,
                  DriveResourceMetadata* metadata,
                  OperationObserver* observer);
  virtual ~RemoveOperation();

  // Perform the remove operation on the file at drive path |file_path|.
  // Invokes |callback| when finished with the result of the operation.
  // |callback| must not be null.
  virtual void Remove(const base::FilePath& file_path,
                      bool is_recursive,
                      const FileOperationCallback& callback);

 private:
  // Part of Remove(). Called after GetEntryInfoByPath() is complete.
  // |callback| must not be null.
  void RemoveAfterGetEntryInfo(
      const FileOperationCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Callback for DriveServiceInterface::DeleteResource. Removes the entry with
  // |resource_id| from the local snapshot of the filesystem and the cache.
  // |callback| must not be null.
  void RemoveResourceLocally(
      const FileOperationCallback& callback,
      const std::string& resource_id,
      google_apis::GDataErrorCode status);

  // Sends notification for directory changes. Notifies of directory changes,
  // and runs |callback| with |error|. |callback| must not be null.
  void NotifyDirectoryChanged(
      const FileOperationCallback& callback,
      DriveFileError error,
      const base::FilePath& directory_path);

  DriveScheduler* drive_scheduler_;
  DriveCache* cache_;
  DriveResourceMetadata* metadata_;
  OperationObserver* observer_;

  // WeakPtrFactory bound to the UI thread.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RemoveOperation> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoveOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_REMOVE_OPERATION_H_
