// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_DRIVE_OPERATIONS_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_DRIVE_OPERATIONS_H_

#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"

namespace base {
class FilePath;
}

namespace google_apis {
class DriveUploaderInterface;
}

namespace drive {

class DriveCache;
class DriveFileSystemInterface;
class DriveScheduler;

namespace file_system {

class CopyOperation;
class CreateDirectoryOperation;
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
  void Init(DriveScheduler* drive_scheduler,
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

  // Wrapper function for create_directory_operation_.
  // |callback| must not be null.
  void CreateDirectory(const base::FilePath& directory_path,
                       bool is_exclusive,
                       bool is_recursive,
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

  // Wrapper function for copy_operation_.
  // |callback| must not be null.
  void TransferRegularFile(const base::FilePath& local_src_file_path,
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

 private:
  scoped_ptr<CopyOperation> copy_operation_;
  scoped_ptr<CreateDirectoryOperation> create_directory_operation_;
  scoped_ptr<MoveOperation> move_operation_;
  scoped_ptr<RemoveOperation> remove_operation_;
  scoped_ptr<UpdateOperation> update_operation_;
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_DRIVE_OPERATIONS_H_
