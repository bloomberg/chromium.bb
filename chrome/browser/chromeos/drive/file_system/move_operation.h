// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_MOVE_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_MOVE_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class GURL;

namespace base {
class FilePath;
}

namespace drive {

class DriveCache;
class DriveEntryProto;
class DriveResourceMetadata;
class DriveScheduler;

namespace file_system {

class OperationObserver;

// This class encapsulates the drive Move function.  It is responsible for
// sending the request to the drive API, then updating the local state and
// metadata to reflect the new state.
class MoveOperation {
 public:
  MoveOperation(DriveScheduler* drive_scheduler,
                DriveResourceMetadata* metadata,
                OperationObserver* observer);
  virtual ~MoveOperation();

  // Performs the move operation on the file at drive path |src_file_path|
  // with a target of |dest_file_path|.  Invokes |callback| when finished with
  // the result of the operation. |callback| must not be null.
  virtual void Move(const base::FilePath& src_file_path,
                    const base::FilePath& dest_file_path,
                    const FileOperationCallback& callback);
 private:
  // Step 1 of Move(), called after the entry info of the source resource and
  // the destination directory is obtained. It renames the resource in the
  // source directory, before moving between directories.
  void MoveAfterGetEntryInfoPair(const base::FilePath& dest_file_path,
                                 const FileOperationCallback& callback,
                                 scoped_ptr<EntryInfoPairResult> src_dest_info);

  // Step 2 of Move(), called after renaming is completed. It adds the resource
  // to the destination directory.
  void MoveAfterRename(const FileOperationCallback& callback,
                       scoped_ptr<EntryInfoPairResult> src_dest_info,
                       DriveFileError error,
                       const base::FilePath& src_path);

  // Step 3 of Move(), called after the resource is added to the new directory.
  // It removes the resource from the old directory in the remote server. While
  // our local metadata assumes tree structure, on the server side a resource
  // can belong to multiple collections (directories). At this point the
  // resource is contained in both the new and the old directories.
  void MoveAfterAddToDirectory(const FileOperationCallback& callback,
                               scoped_ptr<EntryInfoPairResult> src_dest_info,
                               DriveFileError error,
                               const base::FilePath& src_path);

  // Step 4 of Move(), called after the resource is removed from the old
  // directory. It calls back to the caller of Move().
  void MoveAfterRemoveFromDirectory(
      const FileOperationCallback& callback,
      scoped_ptr<EntryInfoPairResult> src_dest_info,
      google_apis::GDataErrorCode status);

  // Renames a resource |src_id| at |src_path| to |new_name| in the same
  // directory. |callback| will receive the new file path if the operation is
  // successful. If the new name already exists in the same directory, the file
  // name is uniquified by adding a parenthesized serial number like
  // "foo (2).txt".
  void Rename(const std::string& src_id,
              const base::FilePath& src_path,
              const base::FilePath& new_name,
              bool new_name_has_hosted_extension,
              const FileMoveCallback& callback);

  // Called in Rename() to reflect the rename on the local metadata.
  void RenameLocally(const base::FilePath& src_path,
                     const base::FilePath& new_name,
                     const FileMoveCallback& callback,
                     google_apis::GDataErrorCode status);


  // Moves a resource |src_id| at |src_path| to another directory |dest_dir_id|
  // at |dest_dir_path|. |callback| will receive the new file path if the
  // operation is successful.
  void AddToDirectory(const std::string& src_id,
                      const std::string& dest_dir_id,
                      const base::FilePath& src_path,
                      const base::FilePath& dest_dir_path,
                      const FileMoveCallback& callback);

  // Called in AddToDirectory() to reflect the move on the local metadata.
  void AddToDirectoryLocally(const base::FilePath& src_path,
                             const base::FilePath& dest_dir_path,
                             const FileMoveCallback& callback,
                             google_apis::GDataErrorCode status);

  // Removes a resource |resource_id| from |dir_path|.
  void RemoveFromDirectory(const std::string& resource_id,
                           const base::FilePath& dir_path,
                           const FileOperationCallback& callback);

  // Called in RemoveFromDirectory().
  void RemoveFromDirectoryAfterGetEntryInfo(
      const std::string& resource_id,
      const FileOperationCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Called in RemoveFromDirectory().
  void RemoveFromDirectoryCompleted(const FileOperationCallback& callback,
                                    google_apis::GDataErrorCode status);

  DriveScheduler* drive_scheduler_;
  DriveResourceMetadata* metadata_;
  OperationObserver* observer_;

  // WeakPtrFactory bound to the UI thread.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<MoveOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(MoveOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_MOVE_OPERATION_H_
