// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_FILE_SYSTEM_BACKEND_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_FILE_SYSTEM_BACKEND_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/browser/fileapi/file_system_backend.h"

namespace base {
class SequencedTaskRunner;
}

class MediaPathFilter;

class DeviceMediaAsyncFileUtil;

class MediaFileSystemBackend : public fileapi::FileSystemBackend {
 public:
  static const char kMediaTaskRunnerName[];

  MediaFileSystemBackend(
      const base::FilePath& profile_path,
      base::SequencedTaskRunner* media_task_runner);
  virtual ~MediaFileSystemBackend();

  static bool CurrentlyOnMediaTaskRunnerThread();
  static scoped_refptr<base::SequencedTaskRunner> MediaTaskRunner();

  // FileSystemBackend implementation.
  virtual bool CanHandleType(fileapi::FileSystemType type) const OVERRIDE;
  virtual void Initialize(fileapi::FileSystemContext* context) OVERRIDE;
  virtual void OpenFileSystem(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      fileapi::OpenFileSystemMode mode,
      const OpenFileSystemCallback& callback) OVERRIDE;
  virtual fileapi::AsyncFileUtil* GetAsyncFileUtil(
      fileapi::FileSystemType type) OVERRIDE;
  virtual fileapi::CopyOrMoveFileValidatorFactory*
  GetCopyOrMoveFileValidatorFactory(
      fileapi::FileSystemType type,
      base::PlatformFileError* error_code) OVERRIDE;
  virtual fileapi::FileSystemOperation* CreateFileSystemOperation(
      const fileapi::FileSystemURL& url,
      fileapi::FileSystemContext* context,
      base::PlatformFileError* error_code) const OVERRIDE;
  virtual scoped_ptr<webkit_blob::FileStreamReader> CreateFileStreamReader(
      const fileapi::FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time,
      fileapi::FileSystemContext* context) const OVERRIDE;
  virtual scoped_ptr<fileapi::FileStreamWriter> CreateFileStreamWriter(
      const fileapi::FileSystemURL& url,
      int64 offset,
      fileapi::FileSystemContext* context) const OVERRIDE;
  virtual fileapi::FileSystemQuotaUtil* GetQuotaUtil() OVERRIDE;

 private:
  // Store the profile path. We need this to create temporary snapshot files.
  const base::FilePath profile_path_;

  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  scoped_ptr<MediaPathFilter> media_path_filter_;
  scoped_ptr<fileapi::CopyOrMoveFileValidatorFactory>
      media_copy_or_move_file_validator_factory_;

  scoped_ptr<fileapi::AsyncFileUtil> native_media_file_util_;
  scoped_ptr<DeviceMediaAsyncFileUtil> device_media_async_file_util_;
#if defined(OS_WIN) || defined(OS_MACOSX)
  scoped_ptr<fileapi::AsyncFileUtil> picasa_file_util_;
  scoped_ptr<fileapi::AsyncFileUtil> itunes_file_util_;
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
#if defined(OS_MACOSX)
  scoped_ptr<fileapi::AsyncFileUtil> iphoto_file_util_;
#endif  // defined(OS_MACOSX)

  DISALLOW_COPY_AND_ASSIGN(MediaFileSystemBackend);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_MEDIA_FILE_SYSTEM_BACKEND_H_
