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
class Time;
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
  // with a target of |dest_file_path|.
  // If |preserve_last_modified| is set to true, this tries to preserve
  // last modified time stamp. This is supported only on Drive API v2.
  // Invokes |callback| when finished with the result of the operation.
  // |callback| must not be null.
  void Copy(const base::FilePath& src_file_path,
            const base::FilePath& dest_file_path,
            bool preserve_last_modified,
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
  // Params for Copy().
  struct CopyParams;

  // Part of Copy(). Called after prepartion is done.
  void CopyAfterPrepare(const CopyParams& params,
                        ResourceEntry* src_entry,
                        std::string* parent_resource_id,
                        FileError error);

  // Part of Copy(). Called after the file content is downloaded.
  void CopyAfterDownload(const base::FilePath& dest_file_path,
                         const FileOperationCallback& callback,
                         FileError error,
                         const base::FilePath& local_file_path,
                         scoped_ptr<ResourceEntry> entry);

  // Part of TransferFileFromLocalToRemote(). Called after preparation is done.
  // |gdoc_resource_id| and |parent_resource_id| is available only if the file
  // is JSON GDoc file.
  void TransferFileFromLocalToRemoteAfterPrepare(
      const base::FilePath& local_src_path,
      const base::FilePath& remote_dest_path,
      const FileOperationCallback& callback,
      std::string* gdoc_resource_id,
      std::string* parent_resource_id,
      FileError error);

  // Copies resource with |resource_id| into the directory |parent_resource_id|
  // with renaming it to |new_title|.
  void CopyResourceOnServer(const std::string& resource_id,
                            const std::string& parent_resource_id,
                            const std::string& new_title,
                            const base::Time& last_modified,
                            const FileOperationCallback& callback);

  // Part of CopyResourceOnServer. Called after server side copy is done.
  void CopyResourceOnServerAfterServerSideCopy(
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);

  // Part of CopyResourceOnServer. Called after local state update is done.
  void CopyResourceOnServerAfterUpdateLocalState(
      const FileOperationCallback& callback,
      base::FilePath* file_path,
      FileError error);

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
      std::string* local_id,
      FileError error);

  // Copies the hosted document with |resource_id| to the |dest_path|.
  // This method is designed based on GData WAPI. It cannot create a copy in
  // any directory but my drive root. So, this method creates the copy at
  // my drive root first, then move it into the destination directory.
  // For Drive API v2, this should work, but CopyResourceOnServer should work
  // more efficiently.
  void CopyHostedDocument(const std::string& resource_id,
                          const base::FilePath& dest_path,
                          const FileOperationCallback& callback);

  // Part of CopyHostedDocument(). Called after server side copy is done.
  void CopyHostedDocumentAfterServerSideCopy(
      const base::FilePath& dest_path,
      const FileOperationCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);

  // Part of CopyHostedDocument(). Called after the local metadata is updated.
  void CopyHostedDocumentAfterUpdateLocalState(
      const base::FilePath& dest_path,
      const FileOperationCallback& callback,
      base::FilePath* file_path,
      FileError error);

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
