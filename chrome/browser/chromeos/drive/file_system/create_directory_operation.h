// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CREATE_DIRECTORY_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CREATE_DIRECTORY_OPERATION_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/drive_file_error.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "googleurl/src/gurl.h"

namespace google_apis {

class ResourceEntry;

}  // namespace google_apis

namespace drive {

class DriveEntryProto;
class DriveResourceMetadata;
class DriveScheduler;

namespace file_system {

class OperationObserver;

// This class encapsulates the drive Create Directory function.  It is
// responsible for sending the request to the drive API, then updating the
// local state and metadata to reflect the new state.
class CreateDirectoryOperation {
 public:
  CreateDirectoryOperation(DriveScheduler* drive_scheduler,
                           DriveResourceMetadata* metadata,
                           OperationObserver* observer);
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
  friend class CreateDirectoryOperationTest;
  FRIEND_TEST_ALL_PREFIXES(CreateDirectoryOperationTest,
                           FindFirstMissingParentDirectory);

  // Defines set of parameters passes to intermediate callbacks during
  // execution of CreateDirectory() method.
  struct CreateDirectoryParams;

  // Defines possible search results of FindFirstMissingParentDirectory().
  enum FindFirstMissingParentDirectoryError {
    // Target directory found, it's not a directory.
    FIND_FIRST_FOUND_INVALID,
    // Found missing directory segment while searching for given directory.
    FIND_FIRST_FOUND_MISSING,
    // Found target directory, it already exists.
    FIND_FIRST_DIRECTORY_ALREADY_PRESENT,
  };

  // The result struct for FindFirstMissingParentDirectory().
  struct FindFirstMissingParentDirectoryResult {
    FindFirstMissingParentDirectoryResult();
    ~FindFirstMissingParentDirectoryResult();

    // Initializes the struct.
    void Init(FindFirstMissingParentDirectoryError error,
              base::FilePath first_missing_parent_path,
              const std::string& last_dir_resource_id);

    FindFirstMissingParentDirectoryError error;
    // The following two fields are provided when |error| is set to
    // FIND_FIRST_FOUND_MISSING. Otherwise, the two fields are undefined.

    // Suppose "drive/foo/bar/baz/qux" is being checked, and only
    // "drive/foo/bar" is present, "drive/foo/bar/baz" is the first missing
    // parent path.
    base::FilePath first_missing_parent_path;

    // The resource id of the last found directory. In the above example, the
    // resource id of "drive/foo/bar".
    std::string last_dir_resource_id;
  };

  // Callback for FindFirstMissingParentDirectory().
  typedef base::Callback<void(
      const FindFirstMissingParentDirectoryResult& result)>
      FindFirstMissingParentDirectoryCallback;

  // Params for FindFirstMissingParentDirectory().
  struct FindFirstMissingParentDirectoryParams;

  // Part of CreateDirectory(). Called after
  // FindFirstMissingParentDirectory() is complete.
  // |callback| must not be null.
  void CreateDirectoryAfterFindFirstMissingPath(
      scoped_ptr<CreateDirectoryParams> params,
      const FindFirstMissingParentDirectoryResult& result);

  // Callback for handling directory create requests. Adds the directory
  // represented by |entry| to the local filesystem.
  void AddNewDirectory(scoped_ptr<CreateDirectoryParams> params,
                       const base::FilePath& created_directory_path,
                       google_apis::GDataErrorCode status,
                       scoped_ptr<google_apis::ResourceEntry> entry);

  // Callback for DriveResourceMetadata::AddEntryToDirectory. Continues the
  // recursive creation of a directory path by calling CreateDirectory again.
  void ContinueCreateDirectory(scoped_ptr<CreateDirectoryParams> params,
                               const base::FilePath& created_directory_path,
                               DriveFileError error,
                               const base::FilePath& moved_file_path);

  // Finds the first missing parent directory of |directory_path|.
  // |callback| must not be null.
  void FindFirstMissingParentDirectory(
      const base::FilePath& directory_path,
      const FindFirstMissingParentDirectoryCallback& callback);

  // Helper function for FindFirstMissingParentDirectory, for recursive search
  // for first missing parent.
  void FindFirstMissingParentDirectoryInternal(
      scoped_ptr<FindFirstMissingParentDirectoryParams> params);

  // Callback for ResourceMetadata::GetEntryInfoByPath from
  // FindFirstMissingParentDirectory.
  void ContinueFindFirstMissingParentDirectory(
      scoped_ptr<FindFirstMissingParentDirectoryParams> params,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  DriveScheduler* drive_scheduler_;
  DriveResourceMetadata* metadata_;
  OperationObserver* observer_;

  // WeakPtrFactory bound to the UI thread.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CreateDirectoryOperation> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CreateDirectoryOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_CREATE_DIRECTORY_OPERATION_H_
