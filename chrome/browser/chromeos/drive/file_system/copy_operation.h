// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_COPY_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_COPY_OPERATION_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace google_apis {
class ResourceEntry;
}  // namespace google_apis

namespace drive {

class DriveServiceInterface;
class JobScheduler;
class ResourceEntry;

namespace internal {
class FileCache;
}  // namespace internal

namespace file_system {

class CreateFileOperation;
class DownloadOperation;
class MoveOperation;
class OperationObserver;

// This class encapsulates the drive Copy function.  It is responsible for
// sending the request to the drive API, then updating the local state and
// metadata to reflect the new state.
class CopyOperation {
 public:
  CopyOperation(base::SequencedTaskRunner* blocking_task_runner,
                OperationObserver* observer,
                JobScheduler* scheduler,
                internal::ResourceMetadata* metadata,
                internal::FileCache* cache,
                DriveServiceInterface* drive_service,
                const base::FilePath& temporary_file_directory);
  ~CopyOperation();

  // Performs the copy operation on the file at drive path |src_file_path|
  // with a target of |dest_file_path|. Invokes |callback| when finished with
  // the result of the operation. |callback| must not be null.
  void Copy(const base::FilePath& src_file_path,
            const base::FilePath& dest_file_path,
            const FileOperationCallback& callback);

  // Initiates transfer of |local_src_file_path| to |remote_dest_file_path|.
  // |local_src_file_path| must be a file from the local file system.
  // |remote_dest_file_path| is the virtual destination path within Drive file
  // system.
  //
  // |callback| must not be null.
  void TransferFileFromLocalToRemote(
      const base::FilePath& local_src_file_path,
      const base::FilePath& remote_dest_file_path,
      const FileOperationCallback& callback);

 private:
  // Creates an empty file on the server at |remote_dest_path| to ensure
  // the location, stores a file at |local_file_path| in cache and marks it
  // dirty, so that SyncClient will upload the data later.
  void ScheduleTransferRegularFile(const base::FilePath& local_src_path,
                                   const base::FilePath& remote_dest_path,
                                   const FileOperationCallback& callback);

  // Part of ScheduleTransferRegularFile(). Called after GetFileSize() is
  // completed.
  void ScheduleTransferRegularFileAfterGetFileSize(
      const base::FilePath& local_src_path,
      const base::FilePath& remote_dest_path,
      const FileOperationCallback& callback,
      int64 local_file_size);

  // Part of ScheduleTransferRegularFile(). Called after GetAboutResource()
  // is completed.
  void ScheduleTransferRegularFileAfterGetAboutResource(
      const base::FilePath& local_src_path,
      const base::FilePath& remote_dest_path,
      const FileOperationCallback& callback,
      int64 local_file_size,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Part of ScheduleTransferRegularFile(). Called after file creation.
  void ScheduleTransferRegularFileAfterCreate(
      const base::FilePath& local_src_path,
      const base::FilePath& remote_dest_path,
      const FileOperationCallback& callback,
      FileError error);

  // Part of ScheduleTransferRegularFile(). Called after updating local state
  // is completed.
  void ScheduleTransferRegularFileAfterUpdateLocalState(
      const FileOperationCallback& callback,
      std::string* resource_id,
      FileError error);

  // Copies a hosted document with |resource_id| to |dest_path|
  void CopyHostedDocumentToDirectory(const base::FilePath& dest_path,
                                     const std::string& resource_id,
                                     const FileOperationCallback& callback);

  // Callback for handling document copy attempt.
  void OnCopyHostedDocumentCompleted(
      const base::FilePath& dest_path,
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);

  // Moves a file or directory at |file_path| in the root directory to
  // |dest_path|. This function does nothing if |dest_path| points to a
  // resource directly under the root directory.
  void MoveEntryFromRootDirectory(const base::FilePath& dest_path,
                                  const FileOperationCallback& callback,
                                  FileError error,
                                  const base::FilePath& file_path);

  // Part of Copy(). Called after GetResourceEntryPairByPaths() is complete.
  void CopyAfterGetResourceEntryPair(const base::FilePath& dest_file_path,
                                     const FileOperationCallback& callback,
                                     scoped_ptr<EntryInfoPairResult> result);

  // Callback for handling resource copy on the server.
  // |callback| must not be null.
  void OnCopyResourceCompleted(
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);

  // Invoked upon completion of GetFileByPath initiated by Copy. If
  // GetFileByPath reports no error, calls TransferRegularFile to transfer
  // |local_file_path| to |remote_dest_file_path|.
  void OnGetFileCompleteForCopy(const base::FilePath& remote_dest_file_path,
                                const FileOperationCallback& callback,
                                FileError error,
                                const base::FilePath& local_file_path,
                                scoped_ptr<ResourceEntry> entry);

  // Part of TransferFileFromLocalToRemote(). Called after |parent_entry| is
  // retrieved in the blocking pool.
  void TransferFileFromLocalToRemoteAfterPrepare(
      const base::FilePath& local_src_file_path,
      const base::FilePath& remote_dest_file_path,
      const FileOperationCallback& callback,
      ResourceEntry* parent_entry,
      FileError error);

  // Part of TransferFileFromLocalToRemote(). Called after the result of
  // GetAboutResource() is avaiable.
  void TransferFileFromLocalToRemoteAfterGetQuota(
      const base::FilePath& local_src_file_path,
      const base::FilePath& remote_dest_file_path,
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Initiates transfer of |local_file_path| with |resource_id| to
  // |remote_dest_file_path|. |local_file_path| must be a file from the local
  // file system, |remote_dest_file_path| is the virtual destination path within
  // Drive file system. If |resource_id| is a non-empty string, the transfer is
  // handled by CopyDocumentToDirectory. Otherwise, the transfer is handled by
  // TransferRegularFile.
  void TransferFileForResourceId(const base::FilePath& local_file_path,
                                 const base::FilePath& remote_dest_file_path,
                                 const FileOperationCallback& callback,
                                 const std::string& resource_id);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  OperationObserver* observer_;
  JobScheduler* scheduler_;
  internal::ResourceMetadata* metadata_;
  internal::FileCache* cache_;
  DriveServiceInterface* drive_service_;

  // Uploading a new file is internally implemented by creating a dirty file.
  scoped_ptr<CreateFileOperation> create_file_operation_;
  // Copying from remote to local (and to remote in WAPI) involves downloading.
  scoped_ptr<DownloadOperation> download_operation_;
  // Copying a hosted document is internally implemented by using a move.
  scoped_ptr<MoveOperation> move_operation_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CopyOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(CopyOperation);
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_COPY_OPERATION_H_
