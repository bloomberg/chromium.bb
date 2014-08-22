// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_ASYNC_UTIL_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_ASYNC_UTIL_H_

#include "chrome/browser/local_discovery/storage/privet_filesystem_attribute_cache.h"
#include "chrome/browser/local_discovery/storage/privet_filesystem_operations.h"
#include "content/public/browser/browser_context.h"
#include "webkit/browser/fileapi/async_file_util.h"

namespace local_discovery {

class PrivetFileSystemAsyncUtil : public storage::AsyncFileUtil {
 public:
  explicit PrivetFileSystemAsyncUtil(content::BrowserContext* browser_context);
  virtual ~PrivetFileSystemAsyncUtil();

  virtual void CreateOrOpen(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      int file_flags,
      const CreateOrOpenCallback& callback) OVERRIDE;
  virtual void EnsureFileExists(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const EnsureFileExistsCallback& callback) OVERRIDE;
  virtual void CreateDirectory(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      bool exclusive,
      bool recursive,
      const StatusCallback& callback) OVERRIDE;
  virtual void GetFileInfo(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const GetFileInfoCallback& callback) OVERRIDE;
  virtual void ReadDirectory(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const ReadDirectoryCallback& callback) OVERRIDE;
  virtual void Touch(scoped_ptr<storage::FileSystemOperationContext> context,
                     const storage::FileSystemURL& url,
                     const base::Time& last_access_time,
                     const base::Time& last_modified_time,
                     const StatusCallback& callback) OVERRIDE;
  virtual void Truncate(scoped_ptr<storage::FileSystemOperationContext> context,
                        const storage::FileSystemURL& url,
                        int64 length,
                        const StatusCallback& callback) OVERRIDE;
  virtual void CopyFileLocal(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      CopyOrMoveOption option,
      const CopyFileProgressCallback& progress_callback,
      const StatusCallback& callback) OVERRIDE;
  virtual void MoveFileLocal(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      CopyOrMoveOption option,
      const StatusCallback& callback) OVERRIDE;
  virtual void CopyInForeignFile(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const base::FilePath& src_file_path,
      const storage::FileSystemURL& dest_url,
      const StatusCallback& callback) OVERRIDE;
  virtual void DeleteFile(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const StatusCallback& callback) OVERRIDE;
  virtual void DeleteDirectory(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const StatusCallback& callback) OVERRIDE;
  virtual void DeleteRecursively(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const StatusCallback& callback) OVERRIDE;
  virtual void CreateSnapshotFile(
      scoped_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      const CreateSnapshotFileCallback& callback) OVERRIDE;

 private:
  PrivetFileSystemOperationFactory* operation_factory_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_ASYNC_UTIL_H_
