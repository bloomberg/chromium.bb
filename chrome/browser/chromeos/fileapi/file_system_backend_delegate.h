// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_SYSTEM_BACKEND_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_SYSTEM_BACKEND_DELEGATE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/common/fileapi/file_system_types.h"

namespace base {
class Time;
}  // namespace base

namespace storage {
class AsyncFileUtil;
class FileSystemContext;
class FileSystemURL;
class FileStreamWriter;
}  // namespace storage

namespace storage {
class FileStreamReader;
}  // namespace storage

namespace chromeos {

// This is delegate interface to inject the implementation of the some methods
// of FileSystemBackend. The main goal is to inject Drive File System.
class FileSystemBackendDelegate {
 public:
  virtual ~FileSystemBackendDelegate() {}

  // Called from FileSystemBackend::GetAsyncFileUtil().
  virtual storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) = 0;

  // Called from FileSystemBackend::CreateFileStreamReader().
  virtual scoped_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const storage::FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context) = 0;

  // Called from FileSystemBackend::CreateFileStreamWriter().
  virtual scoped_ptr<storage::FileStreamWriter> CreateFileStreamWriter(
      const storage::FileSystemURL& url,
      int64 offset,
      storage::FileSystemContext* context) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_SYSTEM_BACKEND_DELEGATE_H_
