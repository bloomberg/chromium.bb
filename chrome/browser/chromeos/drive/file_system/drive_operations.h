// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_DRIVE_OPERATIONS_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_DRIVE_OPERATIONS_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace drive {

class JobScheduler;

namespace internal {
class FileCache;
class ResourceMetadata;
}  // namespace internal

namespace file_system {

class CopyOperation;
class CreateDirectoryOperation;
class CreateFileOperation;
class MoveOperation;
class OperationObserver;
class RemoveOperation;
class SearchOperation;
class UpdateOperation;

// Callback for DriveOperations::Search.
// On success, |error| is FILE_ERROR_OK, and remaining arguments are valid to
// use. if |is_update_needed| is true, some mismatch is found between
// the result from the server and local metadata, so the caller should update
// the resource metadata.
// |next_feed| is the URL to fetch the remaining result from the server. Maybe
// empty if there is no more results.
// On error, |error| is set to other than FILE_ERROR_OK, and the caller
// shouldn't use remaining arguments.
typedef base::Callback<void(FileError error,
                            bool is_update_needed,
                            const GURL& next_feed,
                            scoped_ptr<std::vector<SearchResultInfo> > result)>
    SearchOperationCallback;

// Passes notifications from Drive operations back to the file system.
class DriveOperations {
 public:
  DriveOperations();
  ~DriveOperations();

  // Allocates the operation objects and initializes the operation pointers.
  void Init(OperationObserver* observer,
            JobScheduler* scheduler,
            internal::ResourceMetadata* metadata,
            internal::FileCache* cache,
            FileSystemInterface* file_system,
            base::SequencedTaskRunner* blocking_task_runner);

  // Wrapper function for create_directory_operation_.
  // |callback| must not be null.
  void CreateDirectory(const base::FilePath& directory_path,
                       bool is_exclusive,
                       bool is_recursive,
                       const FileOperationCallback& callback);

  // Wrapper function for create_file_operation_.
  // |callback| must not be null.
  void CreateFile(const base::FilePath& remote_file_path,
                  bool is_exclusive,
                  const FileOperationCallback& callback);

  // Wrapper function for copy_operation_.
  // |callback| must not be null.
  void Copy(const base::FilePath& src_file_path,
            const base::FilePath& dest_file_path,
            const FileOperationCallback& callback);

  // Wrapper function for copy_operation_.
  // |callback| must not be null.
  void TransferFileFromRemoteToLocal(const base::FilePath& remote_src_file_path,
                                     const base::FilePath& local_dest_file_path,
                                     const FileOperationCallback& callback);

  // Wrapper function for copy_operation_.
  // |callback| must not be null.
  void TransferFileFromLocalToRemote(
      const base::FilePath& local_src_file_path,
      const base::FilePath& remote_dest_file_path,
      const FileOperationCallback& callback);

  // Wrapper function for move_operation_.
  // |callback| must not be null.
  void Move(const base::FilePath& src_file_path,
            const base::FilePath& dest_file_path,
            const FileOperationCallback& callback);

  // Wrapper function for remove_operation_.
  // |callback| must not be null.
  void Remove(const base::FilePath& file_path,
              bool is_recursive,
              const FileOperationCallback& callback);

  // Wrapper function for update_operation_.
  // |callback| must not be null.
  void UpdateFileByResourceId(const std::string& resource_id,
                              DriveClientContext context,
                              const FileOperationCallback& callback);

  // Wrapper function for search_operation_.
  // |callback| must not be null.
  void Search(const std::string& search_query,
              const GURL& next_feed,
              const SearchOperationCallback& callback);

 private:
  scoped_ptr<CopyOperation> copy_operation_;
  scoped_ptr<CreateDirectoryOperation> create_directory_operation_;
  scoped_ptr<CreateFileOperation> create_file_operation_;
  scoped_ptr<MoveOperation> move_operation_;
  scoped_ptr<RemoveOperation> remove_operation_;
  scoped_ptr<UpdateOperation> update_operation_;
  scoped_ptr<SearchOperation> search_operation_;
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_DRIVE_OPERATIONS_H_
