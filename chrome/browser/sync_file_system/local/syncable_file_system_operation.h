// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNCABLE_FILE_SYSTEM_OPERATION_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNCABLE_FILE_SYSTEM_OPERATION_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "storage/browser/fileapi/file_system_operation.h"
#include "storage/browser/fileapi/file_system_url.h"

namespace storage {
class FileSystemContext;
class FileSystemOperationContext;
}

namespace sync_file_system {

class SyncableFileOperationRunner;

// A wrapper class of FileSystemOperation for syncable file system.
class SyncableFileSystemOperation
    : public NON_EXPORTED_BASE(storage::FileSystemOperation),
      public base::NonThreadSafe {
 public:
  virtual ~SyncableFileSystemOperation();

  // storage::FileSystemOperation overrides.
  virtual void CreateFile(const storage::FileSystemURL& url,
                          bool exclusive,
                          const StatusCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const storage::FileSystemURL& url,
                               bool exclusive,
                               bool recursive,
                               const StatusCallback& callback) OVERRIDE;
  virtual void Copy(const storage::FileSystemURL& src_url,
                    const storage::FileSystemURL& dest_url,
                    CopyOrMoveOption option,
                    const CopyProgressCallback& progress_callback,
                    const StatusCallback& callback) OVERRIDE;
  virtual void Move(const storage::FileSystemURL& src_url,
                    const storage::FileSystemURL& dest_url,
                    CopyOrMoveOption option,
                    const StatusCallback& callback) OVERRIDE;
  virtual void DirectoryExists(const storage::FileSystemURL& url,
                               const StatusCallback& callback) OVERRIDE;
  virtual void FileExists(const storage::FileSystemURL& url,
                          const StatusCallback& callback) OVERRIDE;
  virtual void GetMetadata(const storage::FileSystemURL& url,
                           const GetMetadataCallback& callback) OVERRIDE;
  virtual void ReadDirectory(const storage::FileSystemURL& url,
                             const ReadDirectoryCallback& callback) OVERRIDE;
  virtual void Remove(const storage::FileSystemURL& url,
                      bool recursive,
                      const StatusCallback& callback) OVERRIDE;
  virtual void Write(const storage::FileSystemURL& url,
                     scoped_ptr<storage::FileWriterDelegate> writer_delegate,
                     scoped_ptr<net::URLRequest> blob_request,
                     const WriteCallback& callback) OVERRIDE;
  virtual void Truncate(const storage::FileSystemURL& url,
                        int64 length,
                        const StatusCallback& callback) OVERRIDE;
  virtual void TouchFile(const storage::FileSystemURL& url,
                         const base::Time& last_access_time,
                         const base::Time& last_modified_time,
                         const StatusCallback& callback) OVERRIDE;
  virtual void OpenFile(const storage::FileSystemURL& url,
                        int file_flags,
                        const OpenFileCallback& callback) OVERRIDE;
  virtual void Cancel(const StatusCallback& cancel_callback) OVERRIDE;
  virtual void CreateSnapshotFile(
      const storage::FileSystemURL& path,
      const SnapshotFileCallback& callback) OVERRIDE;
  virtual void CopyInForeignFile(const base::FilePath& src_local_disk_path,
                                 const storage::FileSystemURL& dest_url,
                                 const StatusCallback& callback) OVERRIDE;
  virtual void RemoveFile(const storage::FileSystemURL& url,
                          const StatusCallback& callback) OVERRIDE;
  virtual void RemoveDirectory(const storage::FileSystemURL& url,
                               const StatusCallback& callback) OVERRIDE;
  virtual void CopyFileLocal(const storage::FileSystemURL& src_url,
                             const storage::FileSystemURL& dest_url,
                             CopyOrMoveOption option,
                             const CopyFileProgressCallback& progress_callback,
                             const StatusCallback& callback) OVERRIDE;
  virtual void MoveFileLocal(const storage::FileSystemURL& src_url,
                             const storage::FileSystemURL& dest_url,
                             CopyOrMoveOption option,
                             const StatusCallback& callback) OVERRIDE;
  virtual base::File::Error SyncGetPlatformPath(
      const storage::FileSystemURL& url,
      base::FilePath* platform_path) OVERRIDE;

 private:
  typedef SyncableFileSystemOperation self;
  class QueueableTask;

  // Only SyncFileSystemBackend can create a new operation directly.
  friend class SyncFileSystemBackend;

  SyncableFileSystemOperation(
      const storage::FileSystemURL& url,
      storage::FileSystemContext* file_system_context,
      scoped_ptr<storage::FileSystemOperationContext> operation_context);

  void DidFinish(base::File::Error status);
  void DidWrite(const WriteCallback& callback,
                base::File::Error result,
                int64 bytes,
                bool complete);

  void OnCancelled();

  const storage::FileSystemURL url_;

  scoped_ptr<storage::FileSystemOperation> impl_;
  base::WeakPtr<SyncableFileOperationRunner> operation_runner_;
  std::vector<storage::FileSystemURL> target_paths_;

  StatusCallback completion_callback_;

  base::WeakPtrFactory<SyncableFileSystemOperation> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncableFileSystemOperation);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNCABLE_FILE_SYSTEM_OPERATION_H_
