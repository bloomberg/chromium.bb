// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CREATE_DIRECTORY_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CREATE_DIRECTORY_OPERATION_H_

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

// This class encapsulates the drive Create Directory function.  It is
// responsible for sending the request to the drive API, then updating the
// local state and metadata to reflect the new state.
class CreateDirectoryOperation {
 public:
  CreateDirectoryOperation(base::SequencedTaskRunner* blocking_task_runner,
                           OperationObserver* observer,
                           JobScheduler* scheduler,
                           internal::ResourceMetadata* metadata);
  ~CreateDirectoryOperation();

  // Creates a new directory at |directory_path|.
  // If |is_exclusive| is true, an error is raised in case a directory exists
  // already at the |directory_path|.
  // If |is_recursive| is true, the invocation creates parent directories as
  // needed just like mkdir -p does.
  // Invokes |callback| when finished with the result of the operation.
  // |callback| must not be null.
  void CreateDirectory(const base::FilePath& directory_path,
                       bool is_exclusive,
                       bool is_recursive,
                       const FileOperationCallback& callback);

 private:
  // Returns the file path to the existing deepest directory, which appears
  // in the |file_path|, with |entry| storing the directory's resource entry.
  // If not found, returns an empty file path.
  // This should run on |blocking_task_runner_|.
  static base::FilePath GetExistingDeepestDirectory(
      internal::ResourceMetadata* metadata,
      const base::FilePath& directory_path,
      ResourceEntry* entry);

  // Part of CreateDirectory(). Called after GetExistingDeepestDirectory
  // is completed.
  void CreateDirectoryAfterGetExistingDeepestDirectory(
      const base::FilePath& directory_path,
      bool is_exclusive,
      bool is_recursive,
      const FileOperationCallback& callback,
      ResourceEntry* existing_deepest_directory_entry,
      const base::FilePath& existing_deepest_directory_path);

  // Creates directories specified by |relative_file_path| under the directory
  // with |parent_resource_id|.
  // For example, if |relative_file_path| is "a/b/c", then "a", "a/b" and
  // "a/b/c" directories will be created.
  // Runs |callback| upon completion.
  void CreateDirectoryRecursively(
      const std::string& parent_resource_id,
      const base::FilePath& relative_file_path,
      const FileOperationCallback& callback);

  // Part of CreateDirectoryRecursively(). Called after AddNewDirectory() on
  // the server is completed.
  void CreateDirectoryRecursivelyAfterAddNewDirectory(
      const base::FilePath& remaining_path,
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode gdata_error,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);

  // Part of CreateDirectoryRecursively(). Called after updating local state
  // is completed.
  void CreateDirectoryRecursivelyAfterUpdateLocalState(
      const std::string& resource_id,
      const base::FilePath& remaining_path,
      const FileOperationCallback& callback,
      base::FilePath* file_path,
      FileError error);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  OperationObserver* observer_;
  JobScheduler* scheduler_;
  internal::ResourceMetadata* metadata_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CreateDirectoryOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(CreateDirectoryOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CREATE_DIRECTORY_OPERATION_H_
