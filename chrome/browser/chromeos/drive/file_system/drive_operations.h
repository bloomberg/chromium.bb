// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_DRIVE_OPERATIONS_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_DRIVE_OPERATIONS_H_

#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"

class FilePath;

namespace google_apis {
class DriveServiceInterface;
class DriveUploaderInterface;
}

namespace drive {

class DriveCache;
class DriveFileSystemInterface;

namespace file_system {

class CopyOperation;
class MoveOperation;
class OperationObserver;
class RemoveOperation;
class UpdateOperation;

// Passes notifications from Drive operations back to the file system.
class DriveOperations {
 public:
  DriveOperations();
  ~DriveOperations();

  // Allocates the operation objects and initializes the operation pointers.
  void Init(google_apis::DriveServiceInterface* drive_service,
            DriveFileSystemInterface* drive_file_system,
            DriveCache* cache,
            DriveResourceMetadata* metadata,
            google_apis::DriveUploaderInterface* uploader,
            scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
            OperationObserver* observer);

  // Initializes the operation pointers.  For testing only.
  void InitForTesting(CopyOperation* copy_operation,
                      MoveOperation* move_operation,
                      RemoveOperation* remove_operation,
                      UpdateOperation* update_operation);

  // Wrapper function for copy_operation_.
  // |callback| must not be null.
  void Copy(const FilePath& src_file_path,
            const FilePath& dest_file_path,
            const FileOperationCallback& callback);

  // Wrapper function for copy_operation_.
  // |callback| must not be null.
  void TransferFileFromRemoteToLocal(const FilePath& remote_src_file_path,
                                     const FilePath& local_dest_file_path,
                                     const FileOperationCallback& callback);

  // Wrapper function for copy_operation_.
  // |callback| must not be null.
  void TransferFileFromLocalToRemote(const FilePath& local_src_file_path,
                                     const FilePath& remote_dest_file_path,
                                     const FileOperationCallback& callback);

  // Wrapper function for copy_operation_.
  // |callback| must not be null.
  void TransferRegularFile(const FilePath& local_src_file_path,
                           const FilePath& remote_dest_file_path,
                           const FileOperationCallback& callback);

  // Wrapper function for move_operation_.
  // |callback| must not be null.
  void Move(const FilePath& src_file_path,
            const FilePath& dest_file_path,
            const FileOperationCallback& callback);

  // Wrapper function for remove_operation_.
  // |callback| must not be null.
  void Remove(const FilePath& file_path,
              bool is_recursive,
              const FileOperationCallback& callback);

  // Wrapper function for update_operation_.
  // |callback| must not be null.
  void UpdateFileByResourceId(const std::string& resource_id,
                              const FileOperationCallback& callback);

 private:
  scoped_ptr<CopyOperation> copy_operation_;
  scoped_ptr<MoveOperation> move_operation_;
  scoped_ptr<RemoveOperation> remove_operation_;
  scoped_ptr<UpdateOperation> update_operation_;
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_DRIVE_OPERATIONS_H_
