// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_SYSTEM_BACKEND_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_SYSTEM_BACKEND_DELEGATE_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "storage/browser/fileapi/file_system_backend.h"
#include "storage/common/fileapi/file_system_types.h"

class GURL;

namespace base {
class Time;
}  // namespace base

namespace storage {
class AsyncFileUtil;
class FileSystemContext;
class FileStreamReader;
class FileSystemURL;
class FileStreamWriter;
class WatcherManager;
}  // namespace storage

namespace chromeos {

// This is delegate interface to inject the implementation of the some methods
// of FileSystemBackend.
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
      int64 max_bytes_to_read,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context) = 0;

  // Called from FileSystemBackend::CreateFileStreamWriter().
  virtual scoped_ptr<storage::FileStreamWriter> CreateFileStreamWriter(
      const storage::FileSystemURL& url,
      int64 offset,
      storage::FileSystemContext* context) = 0;

  // Called from the FileSystemWatcherService class. The returned pointer must
  // stay valid until shutdown.
  virtual storage::WatcherManager* GetWatcherManager(
      const storage::FileSystemURL& url) = 0;

  // Called from FileSystemBackend::GetRedirectURLForContents.  Please ensure
  // that the returned URL is secure to be opened in a browser tab, or referred
  // from <img>, <video>, XMLHttpRequest, etc...
  virtual void GetRedirectURLForContents(
      const storage::FileSystemURL& url,
      const storage::URLCallback& callback) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_SYSTEM_BACKEND_DELEGATE_H_
