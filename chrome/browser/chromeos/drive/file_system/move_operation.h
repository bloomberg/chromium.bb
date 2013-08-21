// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_MOVE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_MOVE_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

namespace base {
class FilePath;
}  // namespace base

namespace drive {

class JobScheduler;
class ResourceEntry;

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
  // with a target of |dest_file_path|.  Invokes |callback| when finished with
  // the result of the operation. |callback| must not be null.
  void Move(const base::FilePath& src_file_path,
            const base::FilePath& dest_file_path,
            const FileOperationCallback& callback);

 private:
  // Part of Move(). Called after local metadata look up.
  void MoveAfterPrepare(const base::FilePath& src_file_path,
                        const base::FilePath& dest_file_path,
                        const FileOperationCallback& callback,
                        scoped_ptr<ResourceEntry> src_entry,
                        scoped_ptr<ResourceEntry> dest_parent_entry,
                        FileError error);

  // Part of Move(). Called after renaming (without moving the directory)
  // is completed.
  void MoveAfterRename(const base::FilePath& src_file_path,
                       const base::FilePath& dest_file_path,
                       const FileOperationCallback& callback,
                       scoped_ptr<ResourceEntry> src_entry,
                       scoped_ptr<ResourceEntry> dest_parent_entry,
                       FileError error);

  // Part of Move(). Called after adding the entry to the parent is done.
  void MoveAfterAddToDirectory(const base::FilePath& src_file_path,
                               const base::FilePath& dest_file_path,
                               const FileOperationCallback& callback,
                               const std::string& resource_id,
                               const std::string& parent_resource_id,
                               FileError error);


  // Renames the |entry| to |new_title|. Upon completion, |callback| will be
  // called. Note that if |entry|'s title is same as |new_title|, does nothing
  // and calls |callback|.
  // |callback| must not be null.
  void Rename(const ResourceEntry& entry,
              const std::string& new_title,
              const FileOperationCallback& callback);

  // Part of Rename(). Called after server side renaming is done.
  void RenameAfterRenameResource(const std::string& resource_id,
                                 const std::string& new_title,
                                 const FileOperationCallback& callback,
                                 google_apis::GDataErrorCode status);


  // Adds the entry with |resource_id| to the directory |parent_resource_id|.
  // Upon completion, |callback| will be called.
  void AddToDirectory(const std::string& resource_id,
                      const std::string& parent_resource_id,
                      const FileOperationCallback& callback);

  // Part of AddToDirectory(). Called after server side updating is done.
  void AddToDirectoryAfterAddResourceToDirectory(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status);


  // Removes the resource with |resource_id| from the directory with
  // |directory_resource_id|.
  // Upon completion, |callback| will be called.
  void RemoveFromDirectory(const std::string& resource_id,
                           const std::string& directory_resource_id,
                           const FileOperationCallback& callback);

  // Part of RemoveFromDirectory(). Called after server side updating is done.
  void RemoveFromDirectoryAfterRemoveResourceFromDirectory(
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status);

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
