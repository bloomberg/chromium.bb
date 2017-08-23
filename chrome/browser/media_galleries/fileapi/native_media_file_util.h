// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_NATIVE_MEDIA_FILE_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_NATIVE_MEDIA_FILE_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/fileapi/async_file_util.h"

namespace net {
class IOBuffer;
}

class MediaPathFilter;

// This class handles native file system operations with media type filtering.
// To support virtual file systems it implements the AsyncFileUtil interface
// from scratch and provides synchronous override points.
class NativeMediaFileUtil : public storage::AsyncFileUtil {
 public:
  explicit NativeMediaFileUtil(MediaPathFilter* media_path_filter);
  ~NativeMediaFileUtil() override;

  // Uses the MIME sniffer code, which actually looks into the file,
  // to determine if it is really a media file (to avoid exposing
  // non-media files with a media file extension.)
  static base::File::Error IsMediaFile(const base::FilePath& path);
  static base::File::Error BufferIsMediaHeader(net::IOBuffer* buf,
                                                     size_t length);

  // Methods to support CreateOrOpen. Public so they can be shared with
  // DeviceMediaAsyncFileUtil.
  static void CreatedSnapshotFileForCreateOrOpen(
      base::SequencedTaskRunner* media_task_runner,
      int file_flags,
      const storage::AsyncFileUtil::CreateOrOpenCallback& callback,
      base::File::Error result,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      scoped_refptr<storage::ShareableFileReference> file_ref);

  // AsyncFileUtil overrides.
  void CreateOrOpen(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      int file_flags,
      const CreateOrOpenCallback& callback) override;
  void EnsureFileExists(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const EnsureFileExistsCallback& callback) override;
  void CreateDirectory(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      bool exclusive,
      bool recursive,
      const StatusCallback& callback) override;
  void GetFileInfo(std::unique_ptr<storage::FileSystemOperationContext> context,
                   const storage::FileSystemURL& url,
                   int /* fields */,
                   const GetFileInfoCallback& callback) override;
  void ReadDirectory(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const ReadDirectoryCallback& callback) override;
  void Touch(std::unique_ptr<storage::FileSystemOperationContext> context,
             const storage::FileSystemURL& url,
             const base::Time& last_access_time,
             const base::Time& last_modified_time,
             const StatusCallback& callback) override;
  void Truncate(std::unique_ptr<storage::FileSystemOperationContext> context,
                const storage::FileSystemURL& url,
                int64_t length,
                const StatusCallback& callback) override;
  void CopyFileLocal(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      CopyOrMoveOption option,
      const CopyFileProgressCallback& progress_callback,
      const StatusCallback& callback) override;
  void MoveFileLocal(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      CopyOrMoveOption option,
      const StatusCallback& callback) override;
  void CopyInForeignFile(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const base::FilePath& src_file_path,
      const storage::FileSystemURL& dest_url,
      const StatusCallback& callback) override;
  void DeleteFile(std::unique_ptr<storage::FileSystemOperationContext> context,
                  const storage::FileSystemURL& url,
                  const StatusCallback& callback) override;
  void DeleteDirectory(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const StatusCallback& callback) override;
  void DeleteRecursively(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const StatusCallback& callback) override;
  void CreateSnapshotFile(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const CreateSnapshotFileCallback& callback) override;

 protected:
  virtual void CreateDirectoryOnTaskRunnerThread(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      bool exclusive,
      bool recursive,
      const StatusCallback& callback);
  virtual void GetFileInfoOnTaskRunnerThread(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const GetFileInfoCallback& callback);
  virtual void ReadDirectoryOnTaskRunnerThread(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const ReadDirectoryCallback& callback);
  virtual void CopyOrMoveFileLocalOnTaskRunnerThread(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      CopyOrMoveOption option,
      bool copy,
      const StatusCallback& callback);
  virtual void CopyInForeignFileOnTaskRunnerThread(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const base::FilePath& src_file_path,
      const storage::FileSystemURL& dest_url,
      const StatusCallback& callback);
  virtual void DeleteFileOnTaskRunnerThread(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const StatusCallback& callback);
  virtual void DeleteDirectoryOnTaskRunnerThread(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const StatusCallback& callback);
  virtual void CreateSnapshotFileOnTaskRunnerThread(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const CreateSnapshotFileCallback& callback);

  // The following methods should only be called on the task runner thread.

  // Necessary for copy/move to succeed.
  virtual base::File::Error CreateDirectorySync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      bool exclusive,
      bool recursive);
  virtual base::File::Error CopyOrMoveFileSync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      CopyOrMoveOption option,
      bool copy);
  virtual base::File::Error CopyInForeignFileSync(
      storage::FileSystemOperationContext* context,
      const base::FilePath& src_file_path,
      const storage::FileSystemURL& dest_url);
  virtual base::File::Error GetFileInfoSync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      base::File::Info* file_info,
      base::FilePath* platform_path);
  // Called by GetFileInfoSync. Meant to be overridden by subclasses that
  // have special mappings from URLs to platform paths (virtual filesystems).
  virtual base::File::Error GetLocalFilePath(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& file_system_url,
      base::FilePath* local_file_path);
  virtual base::File::Error ReadDirectorySync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      EntryList* file_list);
  virtual base::File::Error DeleteFileSync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url);
  // Necessary for move to succeed.
  virtual base::File::Error DeleteDirectorySync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url);
  virtual base::File::Error CreateSnapshotFileSync(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      base::File::Info* file_info,
      base::FilePath* platform_path,
      scoped_refptr<storage::ShareableFileReference>* file_ref);

  MediaPathFilter* media_path_filter() {
    return media_path_filter_;
  }

 private:
  // Like GetLocalFilePath(), but always take media_path_filter() into
  // consideration. If the media_path_filter() check fails, return
  // Fila::FILE_ERROR_SECURITY. |local_file_path| does not have to exist.
  base::File::Error GetFilteredLocalFilePath(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& file_system_url,
      base::FilePath* local_file_path);

  // Like GetLocalFilePath(), but if the file does not exist, then return
  // |failure_error|.
  // If |local_file_path| is a file, then take media_path_filter() into
  // consideration.
  // If the media_path_filter() check fails, return |failure_error|.
  // If |local_file_path| is a directory, return File::FILE_OK.
  base::File::Error GetFilteredLocalFilePathForExistingFileOrDirectory(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& file_system_url,
      base::File::Error failure_error,
      base::FilePath* local_file_path);

  // Not owned, owned by the backend which owns this.
  MediaPathFilter* const media_path_filter_;

  base::WeakPtrFactory<NativeMediaFileUtil> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeMediaFileUtil);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_NATIVE_MEDIA_FILE_UTIL_H_
