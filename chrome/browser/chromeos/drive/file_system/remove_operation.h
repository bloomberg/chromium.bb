// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_REMOVE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_REMOVE_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
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

// This class encapsulates the drive Remove function.  It is responsible for
// sending the request to the drive API, then updating the local state and
// metadata to reflect the new state.
class RemoveOperation {
 public:
  RemoveOperation(OperationObserver* observer,
                  JobScheduler* scheduler,
                  internal::ResourceMetadata* metadata,
                  internal::FileCache* cache);
  ~RemoveOperation();

  // Perform the remove operation on the file at drive path |file_path|.
  // Invokes |callback| when finished with the result of the operation.
  // |callback| must not be null.
  void Remove(const base::FilePath& file_path,
              bool is_recursive,
              const FileOperationCallback& callback);

 private:
  // Part of Remove(). Called after GetResourceEntryByPath() is complete.
  void RemoveAfterGetResourceEntry(const FileOperationCallback& callback,
                                   FileError error,
                                   scoped_ptr<ResourceEntry> entry);

  // Callback for DriveServiceInterface::DeleteResource. Removes the entry with
  // |resource_id| from the local snapshot of the filesystem and the cache.
  void RemoveResourceLocally(const FileOperationCallback& callback,
                             const std::string& resource_id,
                             google_apis::GDataErrorCode status);

  // Sends notification for directory changes. Notifies of directory changes,
  // and runs |callback| with |error|.
  void NotifyDirectoryChanged(const FileOperationCallback& callback,
                              FileError error,
                              const base::FilePath& directory_path);

  OperationObserver* observer_;
  JobScheduler* scheduler_;
  internal::ResourceMetadata* metadata_;
  internal::FileCache* cache_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RemoveOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(RemoveOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_REMOVE_OPERATION_H_
