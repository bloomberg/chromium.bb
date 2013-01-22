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

class FilePath;
class GURL;

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
  virtual void Move(const FilePath& src_file_path,
                    const FilePath& dest_file_path,
                    const FileOperationCallback& callback);
 private:
  // Part of Move(). Called after GetEntryInfoPairByPaths() is
  // complete. |callback| must not be null.
  void MoveAfterGetEntryInfoPair(
    const FilePath& dest_file_path,
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> result);

  // A pass-through callback used for bridging from
  // FileMoveCallback to FileOperationCallback.
  void OnFilePathUpdated(const FileOperationCallback& cllback,
                         DriveFileError error,
                         const FilePath& file_path);

  // Renames a file or directory at |file_path| to |new_name| in the same
  // directory. |callback| will receive the new file path if the operation is
  // successful. If the new name already exists in the same directory, the file
  // name is uniquified by adding a parenthesized serial number like
  // "foo (2).txt"
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  // |callback| must not be null.
  void Rename(const FilePath& file_path,
              const FilePath::StringType& new_name,
              const FileMoveCallback& callback);

  // Part of Rename(). Called after GetEntryInfoByPath() is complete.
  // |callback| must not be null.
  void RenameAfterGetEntryInfo(const FilePath& file_path,
                               const FilePath::StringType& new_name,
                               const FileMoveCallback& callback,
                               DriveFileError error,
                               scoped_ptr<DriveEntryProto> entry_proto);

  // Callback for handling resource rename attempt. Renames a file or
  // directory at |file_path| on the client side.
  // |callback| must not be null.
  void RenameEntryLocally(const FilePath& file_path,
                          const FilePath::StringType& new_name,
                          const FileMoveCallback& callback,
                          google_apis::GDataErrorCode status);

  // Removes a file or directory at |file_path| from the current directory.
  // It moves the entry to a dangle, no-parent state on the server side.
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  // |callback| must not be null.
  void RemoveEntryFromDirectory(const FileMoveCallback& callback,
                                DriveFileError error,
                                const FilePath& file_path);

  // Part of RemoveEntryFromDirectory(). Called after
  // GetEntryInfoPairByPaths() is complete. |callback| must not be null.
  void RemoveEntryFromDirectoryAfterEntryInfoPair(
    const FileMoveCallback& callback,
    scoped_ptr<EntryInfoPairResult> result);

  // Moves a file or directory at |file_path| to another directory at
  // |dir_path|.
  //
  // Can be called from UI thread. |callback| is run on the calling thread.
  // |callback| must not be null.
  void AddEntryToDirectory(const FilePath& directory_path,
                           const FileOperationCallback& callback,
                           DriveFileError error,
                           const FilePath& file_path);

  // Part of AddEntryToDirectory(). Called after
  // GetEntryInfoPairByPaths() is complete. |callback| must not be null.
  void AddEntryToDirectoryAfterGetEntryInfoPair(
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> result);

  // Moves entry specified by |file_path| to the directory specified by
  // |dir_path| and calls |callback| asynchronously.
  // |callback| must not be null.
  void MoveEntryToDirectory(const FilePath& file_path,
                            const FilePath& directory_path,
                            const FileMoveCallback& callback,
                            google_apis::GDataErrorCode status);

  // Callback when an entry is moved to another directory on the client side.
  // Notifies the directory change and runs |callback|.
  // |callback| must not be null.
  void NotifyAndRunFileOperationCallback(
      const FileOperationCallback& callback,
      DriveFileError error,
      const FilePath& moved_file_path);

  // Callback when an entry is moved to another directory on the client side.
  // Notifies the directory change and runs |callback|.
  // |callback| must not be null.
  void NotifyAndRunFileMoveCallback(
      const FileMoveCallback& callback,
      DriveFileError error,
      const FilePath& moved_file_path);

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
