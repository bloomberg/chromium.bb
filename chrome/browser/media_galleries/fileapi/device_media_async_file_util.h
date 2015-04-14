// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_DEVICE_MEDIA_ASYNC_FILE_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_DEVICE_MEDIA_ASYNC_FILE_UTIL_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/fileapi/async_file_util.h"
#include "storage/browser/fileapi/watcher_manager.h"

namespace storage {
class FileSystemOperationContext;
class FileSystemURL;
}

namespace storage {
class FileStreamReader;
}

enum MediaFileValidationType {
  NO_MEDIA_FILE_VALIDATION,
  APPLY_MEDIA_FILE_VALIDATION,
};

class DeviceMediaAsyncFileUtil : public storage::AsyncFileUtil {
 public:
  ~DeviceMediaAsyncFileUtil() override;

  // Returns an instance of DeviceMediaAsyncFileUtil.
  static scoped_ptr<DeviceMediaAsyncFileUtil> Create(
      const base::FilePath& profile_path,
      MediaFileValidationType validation_type);

  bool SupportsStreaming(const storage::FileSystemURL& url);

  // AsyncFileUtil overrides.
  void CreateOrOpen(scoped_ptr<storage::FileSystemOperationContext> context,
                    const storage::FileSystemURL& url,
                    int file_flags,
                    const CreateOrOpenCallback& callback) override;
  void EnsureFileExists(scoped_ptr<storage::FileSystemOperationContext> context,
                        const storage::FileSystemURL& url,
                        const EnsureFileExistsCallback& callback) override;
  void CreateDirectory(scoped_ptr<storage::FileSystemOperationContext> context,
                       const storage::FileSystemURL& url,
                       bool exclusive,
                       bool recursive,
                       const StatusCallback& callback) override;
  void GetFileInfo(scoped_ptr<storage::FileSystemOperationContext> context,
                   const storage::FileSystemURL& url,
                   const GetFileInfoCallback& callback) override;
  void ReadDirectory(scoped_ptr<storage::FileSystemOperationContext> context,
                     const storage::FileSystemURL& url,
                     const ReadDirectoryCallback& callback) override;
  void Touch(scoped_ptr<storage::FileSystemOperationContext> context,
             const storage::FileSystemURL& url,
             const base::Time& last_access_time,
             const base::Time& last_modified_time,
             const StatusCallback& callback) override;
  void Truncate(scoped_ptr<storage::FileSystemOperationContext> context,
                const storage::FileSystemURL& url,
                int64 length,
                const StatusCallback& callback) override;
  void CopyFileLocal(scoped_ptr<storage::FileSystemOperationContext> context,
                     const storage::FileSystemURL& src_url,
                     const storage::FileSystemURL& dest_url,
                     CopyOrMoveOption option,
                     const CopyFileProgressCallback& progress_callback,
                     const StatusCallback& callback) override;
  void MoveFileLocal(scoped_ptr<storage::FileSystemOperationContext> context,
                     const storage::FileSystemURL& src_url,
                     const storage::FileSystemURL& dest_url,
                     CopyOrMoveOption option,
                     const StatusCallback& callback) override;
  void CopyInForeignFile(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const base::FilePath& src_file_path,
      const storage::FileSystemURL& dest_url,
      const StatusCallback& callback) override;
  void DeleteFile(scoped_ptr<storage::FileSystemOperationContext> context,
                  const storage::FileSystemURL& url,
                  const StatusCallback& callback) override;
  void DeleteDirectory(scoped_ptr<storage::FileSystemOperationContext> context,
                       const storage::FileSystemURL& url,
                       const StatusCallback& callback) override;
  void DeleteRecursively(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const StatusCallback& callback) override;
  void CreateSnapshotFile(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const CreateSnapshotFileCallback& callback) override;

  // This method is called when existing Blobs are read.
  // |expected_modification_time| indicates the expected snapshot state of the
  // underlying storage. The returned FileStreamReader must return an error
  // when the state of the underlying storage changes. Any errors associated
  // with reading this file are returned by the FileStreamReader itself.
  virtual scoped_ptr<storage::FileStreamReader> GetFileStreamReader(
      const storage::FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context);

  // Adds watcher to |url|.
  void AddWatcher(const storage::FileSystemURL& url,
                  bool recursive,
                  const storage::WatcherManager::StatusCallback& callback,
                  const storage::WatcherManager::NotificationCallback&
                      notification_callback);

  // Removes watcher of |url|.
  void RemoveWatcher(const storage::FileSystemURL& url,
                     const bool recursive,
                     const storage::WatcherManager::StatusCallback& callback);

 private:
  class MediaPathFilterWrapper;

  // Use Create() to get an instance of DeviceMediaAsyncFileUtil.
  DeviceMediaAsyncFileUtil(const base::FilePath& profile_path,
                           MediaFileValidationType validation_type);

  // Called when CreateDirectory method call succeeds. |callback| is invoked to
  // complete the CreateDirectory request.
  void OnDidCreateDirectory(const StatusCallback& callback);

  // Called when GetFileInfo method call succeeds. |file_info| contains the
  // file details of the requested url. |callback| is invoked to complete the
  // GetFileInfo request.
  void OnDidGetFileInfo(
      base::SequencedTaskRunner* task_runner,
      const base::FilePath& path,
      const GetFileInfoCallback& callback,
      const base::File::Info& file_info);

  // Called when ReadDirectory method call succeeds. |callback| is invoked to
  // complete the ReadDirectory request.
  //
  // If the contents of the given directory are reported in one batch, then
  // |file_list| will have the list of all files/directories in the given
  // directory and |has_more| will be false.
  //
  // If the contents of the given directory are reported in multiple chunks,
  // |file_list| will have only a subset of all contents (the subsets reported
  // in any two calls are disjoint), and |has_more| will be true, except for
  // the last chunk.
  void OnDidReadDirectory(
      base::SequencedTaskRunner* task_runner,
      const ReadDirectoryCallback& callback,
      const EntryList& file_list,
      bool has_more);

  // Called when MoveFileLocal method call succeeds. |callback| is invoked to
  // complete the MoveFileLocal request.
  void OnDidMoveFileLocal(const StatusCallback& callback);

  // Called when CopyFileLocal method call succeeds. |callback| is invoked to
  // complete the CopyFileLocal request.
  void OnDidCopyFileLocal(const StatusCallback& callback);

  // Called when CopyInForeignFile method call succeeds. |callback| is invoked
  // to complete the CopyInForeignFile request.
  void OnDidCopyInForeignFile(const StatusCallback& callback);

  // Called when DeleteFile method call succeeeds. |callback| is invoked to
  // complete the DeleteFile request.
  void OnDidDeleteFile(const StatusCallback& callback);

  // Called when DeleteDirectory method call succeeds. |callback| is invoked to
  // complete the DeleteDirectory request.
  void OnDidDeleteDirectory(const StatusCallback& callback);

  bool validate_media_files() const;

  // Profile path.
  const base::FilePath profile_path_;

  scoped_refptr<MediaPathFilterWrapper> media_path_filter_wrapper_;

  // For callbacks that may run after destruction.
  base::WeakPtrFactory<DeviceMediaAsyncFileUtil> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMediaAsyncFileUtil);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_DEVICE_MEDIA_ASYNC_FILE_UTIL_H_
