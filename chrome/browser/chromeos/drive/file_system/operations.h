// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OPERATIONS_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OPERATIONS_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
class Time;
}  // namespace base

namespace google_apis {
class DriveServiceInterface;
}  // namespace google_apis

namespace drive {

class JobScheduler;

namespace internal {
class FileCache;
class ResourceMetadata;
}  // namespace internal

namespace file_system {

class CopyOperation;
class CreateDirectoryOperation;
class CreateFileOperation;
class DownloadOperation;
class MoveOperation;
class OperationObserver;
class RemoveOperation;
class SearchOperation;
class TouchOperation;
class UpdateOperation;

// Callback for Operations::Search.
// On success, |error| is FILE_ERROR_OK, and remaining arguments are valid to
// use.
// |next_feed| is the URL to fetch the remaining result from the server. Maybe
// empty if there is no more results.
// On error, |error| is set to other than FILE_ERROR_OK, and the caller
// shouldn't use remaining arguments.
typedef base::Callback<void(FileError error,
                            const GURL& next_feed,
                            scoped_ptr<std::vector<SearchResultInfo> > result)>
    SearchOperationCallback;

// This class is just a bundle of file system operation handlers which
// perform the actual operations. The class is introduced to make it easy to
// initialize the operation handlers.
class Operations {
 public:
  Operations();
  ~Operations();

  // Allocates the operation objects and initializes the operation pointers.
  void Init(OperationObserver* observer,
            JobScheduler* scheduler,
            internal::ResourceMetadata* metadata,
            internal::FileCache* cache,
            FileSystemInterface* file_system,
            google_apis::DriveServiceInterface* drive_service,
            base::SequencedTaskRunner* blocking_task_runner);

  // Wrapper function for create_directory_operation_.
  // |callback| must not be null.
  void CreateDirectory(const base::FilePath& directory_path,
                       bool is_exclusive,
                       bool is_recursive,
                       const FileOperationCallback& callback);

  // Wrapper function for create_file_operation_.
  // |callback| must not be null.
  void CreateFile(const base::FilePath& remote_file_path,
                  bool is_exclusive,
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

  // Wrapper function for touch_operation_.
  // |callback| must not be null.
  void TouchFile(const base::FilePath& file_path,
                 const base::Time& last_access_time,
                 const base::Time& last_modified_time,
                 const FileOperationCallback& callback);

  // Wrapper function for download_operation_.
  // |completion_callback| must not be null.
  void EnsureFileDownloadedByResourceId(
      const std::string& resource_id,
      const ClientContext& context,
      const GetFileContentInitializedCallback& initialized_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const GetFileCallback& completion_callback);

  // Wrapper function for download_operation_.
  // |completion_callback| must not be null.
  void EnsureFileDownloadedByPath(
      const base::FilePath& file_path,
      const ClientContext& context,
      const GetFileContentInitializedCallback& initialized_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const GetFileCallback& completion_callback);

  // Wrapper function for update_operation_.
  // |callback| must not be null.
  void UpdateFileByResourceId(const std::string& resource_id,
                              const ClientContext& context,
                              const FileOperationCallback& callback);

  // Wrapper function for search_operation_.
  // |callback| must not be null.
  void Search(const std::string& search_query,
              const GURL& next_feed,
              const SearchOperationCallback& callback);

 private:
  scoped_ptr<CopyOperation> copy_operation_;
  scoped_ptr<CreateDirectoryOperation> create_directory_operation_;
  scoped_ptr<CreateFileOperation> create_file_operation_;
  scoped_ptr<MoveOperation> move_operation_;
  scoped_ptr<RemoveOperation> remove_operation_;
  scoped_ptr<TouchOperation> touch_operation_;
  scoped_ptr<DownloadOperation> download_operation_;
  scoped_ptr<UpdateOperation> update_operation_;
  scoped_ptr<SearchOperation> search_operation_;
};

}  // namespace file_system
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OPERATIONS_H_
