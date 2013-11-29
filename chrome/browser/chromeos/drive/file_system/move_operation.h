// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_MOVE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_MOVE_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace google_apis {
class ResourceEntry;
}  // namespace google_apis

namespace drive {

class JobScheduler;
class ResourceEntry;

namespace internal {
class ResourceMetadata;
}  // namespace internal

namespace file_system {

class OperationObserver;

// This class encapsulates the drive Move function.  It is responsible for
// sending the request to the drive API, then updating the local state and
// metadata to reflect the new state.
class MoveOperation {
 public:
  MoveOperation(base::SequencedTaskRunner* blocking_task_runner,
                OperationObserver* observer,
                JobScheduler* scheduler,
                internal::ResourceMetadata* metadata);
  ~MoveOperation();

  // Performs the move operation on the file at drive path |src_file_path|
  // with a target of |dest_file_path|.
  // If |preserve_last_modified| is set to true, this tries to preserve
  // last modified time stamp.
  // Invokes |callback| when finished with the result of the operation.
  // |callback| must not be null.
  void Move(const base::FilePath& src_file_path,
            const base::FilePath& dest_file_path,
            bool preserve_last_modified,
            const FileOperationCallback& callback);

 private:
  // Params of Move().
  struct MoveParams;

  // Part of Move(). Called after local metadata look up.
  void MoveAfterPrepare(const MoveParams& params,
                        scoped_ptr<ResourceEntry> src_entry,
                        scoped_ptr<ResourceEntry> src_parent_entry,
                        scoped_ptr<ResourceEntry> dest_parent_entry,
                        FileError error);

  // Part of Move(). Called after UpdateResource is completed.
  void MoveAfterUpdateResource(
      const MoveParams& params,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);

  // Part of Move(). Called after ResourceMetadata::RefreshEntry is completed.
  void MoveAfterRefreshEntry(const MoveParams& params, FileError error);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  OperationObserver* observer_;
  JobScheduler* scheduler_;
  internal::ResourceMetadata* metadata_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<MoveOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(MoveOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_MOVE_OPERATION_H_
