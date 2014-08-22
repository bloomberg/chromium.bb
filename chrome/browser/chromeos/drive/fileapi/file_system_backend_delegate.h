// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILEAPI_FILE_SYSTEM_BACKEND_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILEAPI_FILE_SYSTEM_BACKEND_DELEGATE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend_delegate.h"

namespace storage {
class AsyncFileUtil;
}  // namespace storage

namespace drive {

// Delegate implementation of the some methods in chromeos::FileSystemBackend
// for Drive file system.
class FileSystemBackendDelegate : public chromeos::FileSystemBackendDelegate {
 public:
  FileSystemBackendDelegate();
  virtual ~FileSystemBackendDelegate();

  // FileSystemBackend::Delegate overrides.
  virtual storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) OVERRIDE;
  virtual scoped_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const storage::FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context) OVERRIDE;
  virtual scoped_ptr<storage::FileStreamWriter> CreateFileStreamWriter(
      const storage::FileSystemURL& url,
      int64 offset,
      storage::FileSystemContext* context) OVERRIDE;

 private:
  scoped_ptr<storage::AsyncFileUtil> async_file_util_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemBackendDelegate);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILEAPI_FILE_SYSTEM_BACKEND_DELEGATE_H_
