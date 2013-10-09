// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_NATIVE_MEDIA_FILE_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_NATIVE_MEDIA_FILE_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "webkit/browser/fileapi/async_file_util.h"

class MediaPathFilter;

// This class handles native file system operations with media type filtering.
// To support virtual file systems it implements the AsyncFileUtil interface
// from scratch and provides synchronous override points.
class NativeMediaFileUtil : public fileapi::AsyncFileUtil {
 public:
  explicit NativeMediaFileUtil(MediaPathFilter* media_path_filter);
  virtual ~NativeMediaFileUtil();

  // Uses the MIME sniffer code, which actually looks into the file,
  // to determine if it is really a media file (to avoid exposing
  // non-media files with a media file extension.)
  static base::PlatformFileError IsMediaFile(const base::FilePath& path);

  // AsyncFileUtil overrides.
  virtual void CreateOrOpen(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      int file_flags,
      const CreateOrOpenCallback& callback) OVERRIDE;
  virtual void EnsureFileExists(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const EnsureFileExistsCallback& callback) OVERRIDE;
  virtual void CreateDirectory(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      bool exclusive,
      bool recursive,
      const StatusCallback& callback) OVERRIDE;
  virtual void GetFileInfo(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const GetFileInfoCallback& callback) OVERRIDE;
  virtual void ReadDirectory(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const ReadDirectoryCallback& callback) OVERRIDE;
  virtual void Touch(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time,
      const StatusCallback& callback) OVERRIDE;
  virtual void Truncate(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      int64 length,
      const StatusCallback& callback) OVERRIDE;
  virtual void CopyFileLocal(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& src_url,
      const fileapi::FileSystemURL& dest_url,
      CopyOrMoveOption option,
      const CopyFileProgressCallback& progress_callback,
      const StatusCallback& callback) OVERRIDE;
  virtual void MoveFileLocal(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& src_url,
      const fileapi::FileSystemURL& dest_url,
      CopyOrMoveOption option,
      const StatusCallback& callback) OVERRIDE;
  virtual void CopyInForeignFile(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const base::FilePath& src_file_path,
      const fileapi::FileSystemURL& dest_url,
      const StatusCallback& callback) OVERRIDE;
  virtual void DeleteFile(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const StatusCallback& callback) OVERRIDE;
  virtual void DeleteDirectory(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const StatusCallback& callback) OVERRIDE;
  virtual void DeleteRecursively(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const StatusCallback& callback) OVERRIDE;
  virtual void CreateSnapshotFile(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const CreateSnapshotFileCallback& callback) OVERRIDE;

 protected:
  virtual void CreateDirectoryOnTaskRunnerThread(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      bool exclusive,
      bool recursive,
      const StatusCallback& callback);
  virtual void GetFileInfoOnTaskRunnerThread(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const GetFileInfoCallback& callback);
  virtual void ReadDirectoryOnTaskRunnerThread(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const ReadDirectoryCallback& callback);
  virtual void CopyOrMoveFileLocalOnTaskRunnerThread(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& src_url,
      const fileapi::FileSystemURL& dest_url,
      CopyOrMoveOption option,
      bool copy,
      const StatusCallback& callback);
  virtual void CopyInForeignFileOnTaskRunnerThread(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const base::FilePath& src_file_path,
      const fileapi::FileSystemURL& dest_url,
      const StatusCallback& callback);
  virtual void DeleteDirectoryOnTaskRunnerThread(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const StatusCallback& callback);
  virtual void CreateSnapshotFileOnTaskRunnerThread(
      scoped_ptr<fileapi::FileSystemOperationContext> context,
      const fileapi::FileSystemURL& url,
      const CreateSnapshotFileCallback& callback);

  // The following methods should only be called on the task runner thread.

  // Necessary for copy/move to succeed.
  virtual base::PlatformFileError CreateDirectorySync(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url,
      bool exclusive,
      bool recursive);
  virtual base::PlatformFileError CopyOrMoveFileSync(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& src_url,
      const fileapi::FileSystemURL& dest_url,
      CopyOrMoveOption option,
      bool copy);
  virtual base::PlatformFileError CopyInForeignFileSync(
      fileapi::FileSystemOperationContext* context,
      const base::FilePath& src_file_path,
      const fileapi::FileSystemURL& dest_url);
  virtual base::PlatformFileError GetFileInfoSync(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url,
      base::PlatformFileInfo* file_info,
      base::FilePath* platform_path);
  // Called by GetFileInfoSync. Meant to be overridden by subclasses that
  // have special mappings from URLs to platform paths (virtual filesystems).
  virtual base::PlatformFileError GetLocalFilePath(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& file_system_url,
      base::FilePath* local_file_path);
  virtual base::PlatformFileError ReadDirectorySync(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url,
      EntryList* file_list);
  // Necessary for move to succeed.
  virtual base::PlatformFileError DeleteDirectorySync(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url);
  virtual base::PlatformFileError CreateSnapshotFileSync(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url,
      base::PlatformFileInfo* file_info,
      base::FilePath* platform_path,
      scoped_refptr<webkit_blob::ShareableFileReference>* file_ref);

 protected:
  MediaPathFilter* media_path_filter() {
    return media_path_filter_;
  }

 private:
  // Like GetLocalFilePath(), but always take media_path_filter() into
  // consideration. If the media_path_filter() check fails, return
  // PLATFORM_FILE_ERROR_SECURITY. |local_file_path| does not have to exist.
  base::PlatformFileError GetFilteredLocalFilePath(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& file_system_url,
      base::FilePath* local_file_path);

  // Like GetLocalFilePath(), but if the file does not exist, then return
  // |failure_error|.
  // If |local_file_path| is a file, then take media_path_filter() into
  // consideration.
  // If the media_path_filter() check fails, return |failure_error|.
  // If |local_file_path| is a directory, return PLATFORM_FILE_OK.
  base::PlatformFileError GetFilteredLocalFilePathForExistingFileOrDirectory(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& file_system_url,
      base::PlatformFileError failure_error,
      base::FilePath* local_file_path);


  // Not owned, owned by the backend which owns this.
  MediaPathFilter* media_path_filter_;

  base::WeakPtrFactory<NativeMediaFileUtil> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeMediaFileUtil);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_NATIVE_MEDIA_FILE_UTIL_H_
