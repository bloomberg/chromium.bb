// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_DRIVE_OPERATIONS_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_DRIVE_OPERATIONS_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"

class FilePath;

namespace drive {

class DriveCache;
class DriveServiceInterface;

namespace file_system {

class MoveOperation;
class OperationObserver;
class RemoveOperation;

// Passes notifications from Drive operations back to the file system.
class DriveOperations {
 public:
  DriveOperations();
  ~DriveOperations();

  // Allocates the operation objects and initializes the operation pointers.
  void Init(DriveServiceInterface* drive_service,
            DriveCache* cache,
            DriveResourceMetadata* metadata,
            OperationObserver* observer);

  // Initializes the operation pointers.  For testing only.
  void InitForTesting(MoveOperation* move_operation,
                      RemoveOperation* remove_operation);

  // Wrapper function for move_operation_
  void Move(const FilePath& src_file_path,
            const FilePath& dest_file_path,
            const FileOperationCallback& callback);

  // Wrapper function for remove_operation_
  void Remove(const FilePath& file_path,
                      bool is_recursive,
                      const FileOperationCallback& callback);

 private:
  scoped_ptr<MoveOperation> move_operation_;
  scoped_ptr<RemoveOperation> remove_operation_;
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_DRIVE_OPERATIONS_H_
